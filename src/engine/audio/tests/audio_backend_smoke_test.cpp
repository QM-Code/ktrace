#include "karma/audio/audio_system.hpp"
#include "karma/audio/backend.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

using karma::audio_backend::BackendKind;
using karma::audio_backend::BackendKindName;
using karma::audio_backend::CompiledBackends;
using karma::audio_backend::ParseBackendKind;

bool ContainsBackend(const std::vector<BackendKind>& values, BackendKind needle) {
    for (const BackendKind value : values) {
        if (value == needle) {
            return true;
        }
    }
    return false;
}

void SetEnvVar(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value ? value : "");
#else
    if (value) {
        setenv(key, value, 1);
    } else {
        unsetenv(key);
    }
#endif
}

class ScopedEnvOverride {
 public:
    ScopedEnvOverride(const char* key, const char* value) : key_(key ? key : "") {
        if (key_.empty()) {
            return;
        }

        const char* previous = std::getenv(key_.c_str());
        had_previous_ = (previous != nullptr);
        if (had_previous_) {
            previous_value_ = previous;
        }
        SetEnvVar(key_.c_str(), value);
    }

    ~ScopedEnvOverride() {
        if (key_.empty()) {
            return;
        }
        if (had_previous_) {
            SetEnvVar(key_.c_str(), previous_value_.c_str());
        } else {
            SetEnvVar(key_.c_str(), nullptr);
        }
    }

 private:
    std::string key_{};
    bool had_previous_ = false;
    std::string previous_value_{};
};

const char* BackendForceFailEnv(BackendKind backend) {
    switch (backend) {
        case BackendKind::Sdl3Audio:
            return "KARMA_SDL3AUDIO_FORCE_INIT_FAIL";
        case BackendKind::Miniaudio:
            return "KARMA_MINIAUDIO_FORCE_INIT_FAIL";
        case BackendKind::Auto:
        default:
            return nullptr;
    }
}

bool RunSelectionChecks() {
    const std::vector<BackendKind> compiled_backends = CompiledBackends();
    if (compiled_backends.empty()) {
        std::cerr << "no audio backends compiled\n";
        return false;
    }

    {
        karma::audio::AudioSystem audio;
        audio.setBackend(BackendKind::Auto);
        audio.init();
        if (!audio.isInitialized()) {
            std::cerr << "auto audio backend failed to initialize\n";
            return false;
        }
        if (audio.selectedBackend() != compiled_backends.front()) {
            std::cerr << "auto audio backend selected unexpected backend: "
                      << BackendKindName(audio.selectedBackend()) << "\n";
            audio.shutdown();
            return false;
        }
        audio.shutdown();
    }

    for (const BackendKind backend : compiled_backends) {
        karma::audio::AudioSystem audio;
        audio.setBackend(backend);
        audio.init();
        if (!audio.isInitialized()) {
            std::cerr << "explicit audio backend failed to initialize: " << BackendKindName(backend) << "\n";
            return false;
        }
        if (audio.selectedBackend() != backend) {
            std::cerr << "explicit audio backend selection mismatch requested=" << BackendKindName(backend)
                      << " selected=" << BackendKindName(audio.selectedBackend()) << "\n";
            audio.shutdown();
            return false;
        }
        audio.shutdown();
    }

    return true;
}

bool RunAutoFallbackChecks() {
    const std::vector<BackendKind> compiled_backends = CompiledBackends();
    if (compiled_backends.size() < 2) {
        return true;
    }

    const BackendKind first_backend = compiled_backends.front();
    const BackendKind fallback_backend = compiled_backends[1];
    const char* first_fail_env = BackendForceFailEnv(first_backend);
    const char* fallback_fail_env = BackendForceFailEnv(fallback_backend);
    if (!first_fail_env || !fallback_fail_env) {
        std::cerr << "auto fallback check has unexpected backend ordering\n";
        return false;
    }

    ScopedEnvOverride force_first_fail(first_fail_env, "1");
    ScopedEnvOverride ensure_fallback_enabled(fallback_fail_env, nullptr);

    karma::audio::AudioSystem audio;
    audio.setBackend(BackendKind::Auto);
    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "auto backend failed to initialize with primary backend forced to fail\n";
        return false;
    }
    if (audio.selectedBackend() != fallback_backend) {
        std::cerr << "auto backend fallback mismatch expected=" << BackendKindName(fallback_backend)
                  << " selected=" << BackendKindName(audio.selectedBackend()) << "\n";
        audio.shutdown();
        return false;
    }
    audio.shutdown();
    return true;
}

bool RunBackendSmoke(BackendKind backend) {
    karma::audio::AudioSystem audio;
    audio.setBackend(backend);
    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend) << " failed to initialize\n";
        return false;
    }
    if (audio.selectedBackend() != backend) {
        std::cerr << "backend selection mismatch requested=" << BackendKindName(backend)
                  << " selected=" << BackendKindName(audio.selectedBackend()) << "\n";
        audio.shutdown();
        return false;
    }

    karma::audio_backend::ListenerState listener{};
    listener.position = {1.0f, 2.0f, 3.0f};
    audio.setListener(listener);

    karma::audio_backend::PlayRequest one_shot{};
    one_shot.asset_path = "audio/test/oneshot";
    one_shot.gain = 0.2f;
    one_shot.pitch = 1.0f;
    one_shot.loop = false;
    audio.playOneShot(one_shot);

    karma::audio_backend::PlayRequest looped{};
    looped.asset_path = "audio/test/loop";
    looped.gain = 0.1f;
    looped.pitch = 1.25f;
    looped.loop = true;
    const karma::audio_backend::VoiceId voice = audio.startVoice(looped);
    if (voice == karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend) << " failed to start voice\n";
        audio.shutdown();
        return false;
    }

    audio.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 6; ++i) {
        audio.update(1.0f / 60.0f);
    }
    audio.endFrame();
    audio.stopVoice(voice);
    audio.shutdown();

    if (audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend) << " still initialized after shutdown\n";
        return false;
    }
    if (audio.startVoice(looped) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend) << " allowed startVoice after shutdown\n";
        return false;
    }

    return true;
}

bool RunUninitializedApiChecks(BackendKind backend) {
    karma::audio::AudioSystem audio;
    audio.setBackend(backend);

    karma::audio_backend::ListenerState listener{};
    listener.position = {9.0f, 8.0f, 7.0f};
    audio.setListener(listener);

    karma::audio_backend::PlayRequest request{};
    request.asset_path = "audio/test/uninitialized";
    request.gain = 0.2f;
    request.pitch = 1.0f;
    request.loop = false;
    audio.playOneShot(request);
    audio.beginFrame(1.0f / 60.0f);
    audio.update(1.0f / 60.0f);
    audio.endFrame();
    if (audio.startVoice(request) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " startVoice unexpectedly succeeded before init\n";
        return false;
    }
    audio.stopVoice(karma::audio_backend::kInvalidVoiceId);

    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to initialize in uninitialized API check\n";
        return false;
    }
    audio.shutdown();

    if (audio.startVoice(request) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " startVoice unexpectedly succeeded after shutdown\n";
        return false;
    }
    audio.stopVoice(123456789);
    return true;
}

bool RunVoiceLifecycleChecks(BackendKind backend) {
    karma::audio::AudioSystem audio;
    audio.setBackend(backend);
    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to initialize in voice lifecycle check\n";
        return false;
    }

    karma::audio_backend::PlayRequest looped{};
    looped.asset_path = "audio/test/lifecycle-loop";
    looped.gain = 0.15f;
    looped.pitch = 1.0f;
    looped.loop = true;

    std::unordered_set<karma::audio_backend::VoiceId> ids{};
    for (int i = 0; i < 3; ++i) {
        const auto id = audio.startVoice(looped);
        if (id == karma::audio_backend::kInvalidVoiceId) {
            std::cerr << "backend " << BackendKindName(backend)
                      << " failed to start looped voice " << i << "\n";
            audio.shutdown();
            return false;
        }
        if (!ids.insert(id).second) {
            std::cerr << "backend " << BackendKindName(backend)
                      << " returned duplicate voice id " << id << "\n";
            audio.shutdown();
            return false;
        }
    }

    karma::audio_backend::PlayRequest one_shot{};
    one_shot.asset_path = "audio/test/lifecycle-oneshot";
    one_shot.gain = 0.25f;
    one_shot.pitch = 0.9f;
    one_shot.loop = false;
    audio.playOneShot(one_shot);

    audio.beginFrame(1.0f / 120.0f);
    for (int i = 0; i < 120; ++i) {
        audio.update(1.0f / 120.0f);
    }
    audio.endFrame();

    audio.stopVoice(karma::audio_backend::kInvalidVoiceId);
    audio.stopVoice(99999999);
    for (const auto id : ids) {
        audio.stopVoice(id);
        audio.stopVoice(id);
    }

    audio.beginFrame(1.0f / 60.0f);
    audio.update(1.0f / 60.0f);
    audio.endFrame();
    audio.shutdown();
    return true;
}

bool RunReinitCycleChecks(BackendKind backend) {
    for (int cycle = 0; cycle < 4; ++cycle) {
        karma::audio::AudioSystem audio;
        audio.setBackend(backend);
        audio.init();
        if (!audio.isInitialized()) {
            std::cerr << "backend " << BackendKindName(backend)
                      << " failed init on reinit cycle " << cycle << "\n";
            return false;
        }

        karma::audio_backend::PlayRequest request{};
        request.asset_path = "audio/test/reinit";
        request.gain = 0.1f + (0.05f * static_cast<float>(cycle));
        request.pitch = 1.0f;
        request.loop = (cycle % 2 == 0);
        const auto id = audio.startVoice(request);
        if (id == karma::audio_backend::kInvalidVoiceId) {
            std::cerr << "backend " << BackendKindName(backend)
                      << " failed startVoice on reinit cycle " << cycle << "\n";
            audio.shutdown();
            return false;
        }
        audio.beginFrame(1.0f / 60.0f);
        for (int i = 0; i < 4; ++i) {
            audio.update(1.0f / 60.0f);
        }
        audio.endFrame();
        audio.stopVoice(id);
        audio.shutdown();

        if (audio.startVoice(request) != karma::audio_backend::kInvalidVoiceId) {
            std::cerr << "backend " << BackendKindName(backend)
                      << " startVoice succeeded after shutdown on cycle " << cycle << "\n";
            return false;
        }
    }
    return true;
}

bool ParseRequestedBackends(int argc, char** argv, std::vector<BackendKind>& out_backends) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--backend") {
            if (i + 1 >= argc) {
                std::cerr << "missing value for --backend\n";
                return false;
            }
            const auto parsed = ParseBackendKind(std::string_view(argv[++i]));
            if (!parsed || *parsed == BackendKind::Auto) {
                std::cerr << "invalid backend value for --backend\n";
                return false;
            }
            out_backends.push_back(*parsed);
            continue;
        }
        std::cerr << "unknown argument: " << arg << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<BackendKind> requested_backends{};
    if (!ParseRequestedBackends(argc, argv, requested_backends)) {
        return EXIT_FAILURE;
    }

    const std::vector<BackendKind> compiled_backends = CompiledBackends();
    if (compiled_backends.empty()) {
        std::cerr << "no audio backends compiled\n";
        return EXIT_FAILURE;
    }

    if (!RunSelectionChecks()) {
        return EXIT_FAILURE;
    }
    if (!RunAutoFallbackChecks()) {
        return EXIT_FAILURE;
    }

    if (requested_backends.empty()) {
        requested_backends = compiled_backends;
    }

    for (const BackendKind backend : requested_backends) {
        if (!ContainsBackend(compiled_backends, backend)) {
            std::cerr << "requested backend not compiled: " << BackendKindName(backend) << "\n";
            return EXIT_FAILURE;
        }
        std::cout << "running audio backend smoke checks for '" << BackendKindName(backend) << "'\n";
        if (!RunUninitializedApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunVoiceLifecycleChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunReinitCycleChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBackendSmoke(backend)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "audio backend smoke checks passed\n";
    return EXIT_SUCCESS;
}

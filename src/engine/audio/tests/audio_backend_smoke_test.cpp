#include "karma/audio/audio_system.hpp"
#include "karma/audio/backend.hpp"

#include "audio/backends/spatialization_internal.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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

bool NearlyEqual(float lhs, float rhs, float epsilon = 1e-5f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

bool RunMultiChannelRoutingPolicyChecks() {
    karma::audio_backend::ListenerState listener{};
    listener.position = {0.0f, 0.0f, 0.0f};
    listener.forward = {0.0f, 0.0f, -1.0f};
    listener.up = {0.0f, 1.0f, 0.0f};

    const auto gains = karma::audio_backend::detail::ComputeSpatialGains(
        listener,
        glm::vec3{2.0f, 0.0f, 2.0f});

    if (!(gains.right > gains.left)) {
        std::cerr << "multi-channel routing check expected right gain to exceed left gain\n";
        return false;
    }

    if (!NearlyEqual(karma::audio_backend::detail::ChannelSpatialGain(gains, 0, 2), gains.left)) {
        std::cerr << "multi-channel routing check left channel mismatch\n";
        return false;
    }
    if (!NearlyEqual(karma::audio_backend::detail::ChannelSpatialGain(gains, 1, 2), gains.right)) {
        std::cerr << "multi-channel routing check right channel mismatch\n";
        return false;
    }
    if (!NearlyEqual(karma::audio_backend::detail::ChannelSpatialGain(gains, 0, 1), gains.master)) {
        std::cerr << "multi-channel routing check mono master mismatch\n";
        return false;
    }

    const float expected_fold = 0.5f * (gains.left + gains.right);
    for (const int channel_count : {3, 4, 6, 8}) {
        for (int channel = 2; channel < channel_count; ++channel) {
            const float actual = karma::audio_backend::detail::ChannelSpatialGain(gains, channel, channel_count);
            if (!NearlyEqual(actual, expected_fold)) {
                std::cerr << "multi-channel routing check mismatch for channel_count=" << channel_count
                          << " channel=" << channel << " expected=" << expected_fold << " actual=" << actual
                          << "\n";
                return false;
            }
        }
    }

    return true;
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
    one_shot.world_position = glm::vec3{1.0f, 2.0f, 6.0f};
    audio.playOneShot(one_shot);

    karma::audio_backend::PlayRequest looped{};
    looped.asset_path = "audio/test/loop";
    looped.gain = 0.1f;
    looped.pitch = 1.25f;
    looped.loop = true;
    looped.world_position = glm::vec3{-3.0f, 1.0f, 8.0f};
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
    looped.world_position = glm::vec3{0.0f, 0.0f, 2.5f};

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
    one_shot.world_position = glm::vec3{2.0f, 0.0f, 5.0f};
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
        request.world_position = glm::vec3{static_cast<float>(cycle), 0.0f, 3.0f};
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

bool RunListenerAndPositionalSemanticsChecks(BackendKind backend) {
    karma::audio::AudioSystem audio;
    audio.setBackend(backend);
    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to initialize in listener/positional check\n";
        return false;
    }

    karma::audio_backend::ListenerState listener{};
    listener.position = {0.0f, 1.0f, 0.0f};
    listener.forward = {0.0f, 0.0f, -1.0f};
    listener.up = {0.0f, 1.0f, 0.0f};
    audio.setListener(listener);

    karma::audio_backend::PlayRequest positioned{};
    positioned.asset_path = "audio/test/positioned";
    positioned.gain = 0.2f;
    positioned.pitch = 1.0f;
    positioned.loop = true;
    positioned.world_position = glm::vec3{2.0f, 1.0f, 5.0f};
    const auto positioned_voice = audio.startVoice(positioned);
    if (positioned_voice == karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to start positioned voice\n";
        audio.shutdown();
        return false;
    }

    listener.position = {1.0f, 1.0f, 0.0f};
    listener.forward = {1.0f, 0.0f, 0.0f};
    listener.up = {0.0f, 1.0f, 0.0f};
    audio.setListener(listener);

    karma::audio_backend::PlayRequest invalid_positioned = positioned;
    invalid_positioned.asset_path = "audio/test/invalid-positioned";
    invalid_positioned.loop = false;
    invalid_positioned.world_position =
        glm::vec3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f};
    if (audio.startVoice(invalid_positioned) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " accepted non-finite positioned request\n";
        audio.shutdown();
        return false;
    }
    audio.playOneShot(invalid_positioned);

    karma::audio_backend::PlayRequest non_positional = positioned;
    non_positional.asset_path = "audio/test/non-positional";
    non_positional.loop = false;
    non_positional.world_position.reset();
    const auto non_positional_voice = audio.startVoice(non_positional);
    if (non_positional_voice == karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to start non-positional voice\n";
        audio.shutdown();
        return false;
    }
    if (non_positional_voice == positioned_voice) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " returned duplicate voice id for positional/non-positional requests\n";
        audio.shutdown();
        return false;
    }
    if (non_positional_voice != positioned_voice + 1) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " invalid positional requests changed voice allocation state\n";
        audio.shutdown();
        return false;
    }

    audio.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 10; ++i) {
        listener.position.x += 0.25f;
        listener.position.z += 0.15f;
        audio.setListener(listener);
        audio.update(1.0f / 60.0f);
    }
    audio.endFrame();

    audio.stopVoice(positioned_voice);
    audio.stopVoice(non_positional_voice);
    audio.shutdown();
    return true;
}

bool RunNonFiniteGainPitchChecks(BackendKind backend) {
    karma::audio::AudioSystem audio;
    audio.setBackend(backend);
    audio.init();
    if (!audio.isInitialized()) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to initialize in non-finite gain/pitch check\n";
        return false;
    }

    karma::audio_backend::PlayRequest valid{};
    valid.asset_path = "audio/test/finite-gain-pitch";
    valid.gain = 0.2f;
    valid.pitch = 1.0f;
    valid.loop = true;
    valid.world_position.reset();

    const auto baseline_voice = audio.startVoice(valid);
    if (baseline_voice == karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to start baseline voice in non-finite gain/pitch check\n";
        audio.shutdown();
        return false;
    }

    karma::audio_backend::PlayRequest invalid_gain = valid;
    invalid_gain.asset_path = "audio/test/invalid-gain";
    invalid_gain.loop = false;
    invalid_gain.gain = std::numeric_limits<float>::quiet_NaN();
    if (audio.startVoice(invalid_gain) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " accepted non-finite gain request\n";
        audio.shutdown();
        return false;
    }
    audio.playOneShot(invalid_gain);

    karma::audio_backend::PlayRequest invalid_pitch = valid;
    invalid_pitch.asset_path = "audio/test/invalid-pitch";
    invalid_pitch.loop = false;
    invalid_pitch.pitch = std::numeric_limits<float>::infinity();
    if (audio.startVoice(invalid_pitch) != karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " accepted non-finite pitch request\n";
        audio.shutdown();
        return false;
    }
    audio.playOneShot(invalid_pitch);

    karma::audio_backend::PlayRequest followup_valid = valid;
    followup_valid.asset_path = "audio/test/followup-finite-gain-pitch";
    const auto followup_voice = audio.startVoice(followup_valid);
    if (followup_voice == karma::audio_backend::kInvalidVoiceId) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " failed to start follow-up finite voice after invalid gain/pitch requests\n";
        audio.shutdown();
        return false;
    }
    if (followup_voice != baseline_voice + 1) {
        std::cerr << "backend " << BackendKindName(backend)
                  << " non-finite gain/pitch requests changed voice allocation state\n";
        audio.shutdown();
        return false;
    }

    audio.beginFrame(1.0f / 60.0f);
    audio.update(1.0f / 60.0f);
    audio.endFrame();
    audio.stopVoice(baseline_voice);
    audio.stopVoice(followup_voice);
    audio.shutdown();
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
    if (!RunMultiChannelRoutingPolicyChecks()) {
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
        if (!RunListenerAndPositionalSemanticsChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunNonFiniteGainPitchChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBackendSmoke(backend)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "audio backend smoke checks passed\n";
    return EXIT_SUCCESS;
}

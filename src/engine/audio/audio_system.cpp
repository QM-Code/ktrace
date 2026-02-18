#include "karma/audio/audio_system.hpp"

#include "karma/common/logging/logging.hpp"

#include <sstream>

namespace karma::audio {
namespace {

std::string CompiledBackendList() {
    const auto compiled = audio_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }

    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << audio_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

} // namespace

const char* AudioSystem::selectedBackendName() const {
    return audio_backend::BackendKindName(selected_backend_);
}

void AudioSystem::init() {
    if (initialized_) {
        return;
    }

    initialized_ = false;
    selected_backend_ = audio_backend::BackendKind::Auto;

    const auto try_backend = [this](audio_backend::BackendKind backend_kind) -> bool {
        audio_backend::BackendKind selected = audio_backend::BackendKind::Auto;
        auto backend = audio_backend::CreateBackend(backend_kind, &selected);
        if (!backend) {
            KARMA_TRACE("audio.system",
                        "AudioSystem: backend '{}' unavailable at creation time",
                        audio_backend::BackendKindName(backend_kind));
            return false;
        }

        if (!backend->init()) {
            KARMA_TRACE("audio.system",
                        "AudioSystem: backend '{}' init failed",
                        backend->name());
            backend->shutdown();
            return false;
        }

        selected_backend_ = selected;
        backend_ = std::move(backend);
        initialized_ = true;
        KARMA_TRACE("audio.system",
                    "AudioSystem: backend ready selected='{}'",
                    backend_->name());
        return true;
    };

    KARMA_TRACE("audio.system",
                "AudioSystem: creating backend requested='{}' compiled='{}'",
                audio_backend::BackendKindName(requested_backend_),
                CompiledBackendList());
    if (requested_backend_ == audio_backend::BackendKind::Auto) {
        for (const auto compiled_backend : audio_backend::CompiledBackends()) {
            if (try_backend(compiled_backend)) {
                return;
            }
        }
        KARMA_TRACE("audio.system",
                    "AudioSystem: no compiled backend could be initialized for auto selection");
        return;
    }

    (void)try_backend(requested_backend_);
}

void AudioSystem::shutdown() {
    if (!backend_) {
        initialized_ = false;
        selected_backend_ = audio_backend::BackendKind::Auto;
        return;
    }

    backend_->shutdown();
    backend_.reset();
    initialized_ = false;
    selected_backend_ = audio_backend::BackendKind::Auto;
}

void AudioSystem::beginFrame(float dt) {
    if (!backend_) {
        return;
    }
    backend_->beginFrame(dt);
}

void AudioSystem::update(float dt) {
    if (!backend_) {
        return;
    }
    backend_->update(dt);
}

void AudioSystem::endFrame() {
    if (!backend_) {
        return;
    }
    backend_->endFrame();
}

void AudioSystem::setListener(const audio_backend::ListenerState& state) {
    if (!backend_) {
        return;
    }
    backend_->setListener(state);
}

void AudioSystem::playOneShot(const audio_backend::PlayRequest& request) {
    if (!backend_) {
        return;
    }
    backend_->playOneShot(request);
}

audio_backend::VoiceId AudioSystem::startVoice(const audio_backend::PlayRequest& request) {
    if (!backend_) {
        return audio_backend::kInvalidVoiceId;
    }
    return backend_->startVoice(request);
}

void AudioSystem::stopVoice(audio_backend::VoiceId voice) {
    if (!backend_) {
        return;
    }
    backend_->stopVoice(voice);
}

} // namespace karma::audio

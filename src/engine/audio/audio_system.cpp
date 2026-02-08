#include "karma/audio/audio_system.hpp"

#include "karma/common/logging.hpp"

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

    KARMA_TRACE("audio.system",
                "AudioSystem: creating backend requested='{}' compiled='{}'",
                audio_backend::BackendKindName(requested_backend_),
                CompiledBackendList());
    backend_ = audio_backend::CreateBackend(requested_backend_, &selected_backend_);
    if (!backend_) {
        selected_backend_ = audio_backend::BackendKind::Auto;
        return;
    }

    if (!backend_->init()) {
        KARMA_TRACE("audio.system",
                    "AudioSystem: backend '{}' init failed",
                    backend_->name());
        backend_.reset();
        selected_backend_ = audio_backend::BackendKind::Auto;
        return;
    }

    initialized_ = true;
    KARMA_TRACE("audio.system",
                "AudioSystem: backend ready selected='{}'",
                backend_->name());
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


#pragma once

#include "karma/audio/backend.hpp"

#include <memory>
#include <string>

namespace karma::audio {

class AudioSystem {
 public:
    void setBackend(audio_backend::BackendKind backend) { requested_backend_ = backend; }
    audio_backend::BackendKind requestedBackend() const { return requested_backend_; }
    audio_backend::BackendKind selectedBackend() const { return selected_backend_; }
    const char* selectedBackendName() const;
    bool isInitialized() const { return initialized_; }

    void init();
    void shutdown();
    void beginFrame(float dt);
    void update(float dt);
    void endFrame();

    void setListener(const audio_backend::ListenerState& state);
    void playOneShot(const audio_backend::PlayRequest& request);
    audio_backend::VoiceId startVoice(const audio_backend::PlayRequest& request);
    void stopVoice(audio_backend::VoiceId voice);

 private:
    audio_backend::BackendKind requested_backend_ = audio_backend::BackendKind::Auto;
    audio_backend::BackendKind selected_backend_ = audio_backend::BackendKind::Auto;
    std::unique_ptr<audio_backend::Backend> backend_{};
    bool initialized_ = false;
};

} // namespace karma::audio


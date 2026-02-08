#include "audio/backends/backend_factory_internal.hpp"

#include "karma/common/logging.hpp"

#include <string>
#include <unordered_map>

namespace karma::audio_backend {
namespace {

class MiniaudioBackendStub final : public Backend {
 public:
    const char* name() const override { return "miniaudio"; }

    bool init() override {
        KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: initialized stub backend");
        return true;
    }

    void shutdown() override {
        voices_.clear();
        KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: shutdown");
    }

    void beginFrame(float dt) override {
        (void)dt;
        frame_play_requests_ = 0;
    }

    void update(float dt) override {
        (void)dt;
    }

    void endFrame() override {
        KARMA_TRACE_CHANGED("audio.miniaudio",
                            std::to_string(voices_.size()) + ":" + std::to_string(frame_play_requests_),
                            "AudioBackend[miniaudio]: active voices={} frame_requests={}",
                            voices_.size(),
                            frame_play_requests_);
    }

    void setListener(const ListenerState& state) override {
        listener_ = state;
    }

    void playOneShot(const PlayRequest& request) override {
        (void)request;
        ++frame_play_requests_;
    }

    VoiceId startVoice(const PlayRequest& request) override {
        const VoiceId id = next_voice_id_++;
        voices_[id] = request;
        ++frame_play_requests_;
        return id;
    }

    void stopVoice(VoiceId voice) override {
        voices_.erase(voice);
    }

 private:
    VoiceId next_voice_id_ = 1;
    uint32_t frame_play_requests_ = 0;
    ListenerState listener_{};
    std::unordered_map<VoiceId, PlayRequest> voices_{};
};

} // namespace

std::unique_ptr<Backend> CreateMiniaudioBackend() {
    return std::make_unique<MiniaudioBackendStub>();
}

} // namespace karma::audio_backend


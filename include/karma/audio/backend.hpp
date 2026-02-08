#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace karma::audio_backend {

enum class BackendKind {
    Auto,
    Sdl3Audio,
    Miniaudio
};

const char* BackendKindName(BackendKind kind);
std::optional<BackendKind> ParseBackendKind(std::string_view name);
std::vector<BackendKind> CompiledBackends();

using VoiceId = uint64_t;
inline constexpr VoiceId kInvalidVoiceId = 0;

struct ListenerState {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};
};

struct PlayRequest {
    std::string asset_path{};
    float gain = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
};

class Backend {
 public:
    virtual ~Backend() = default;

    virtual const char* name() const = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt) = 0;
    virtual void update(float dt) = 0;
    virtual void endFrame() = 0;
    virtual void setListener(const ListenerState& state) = 0;

    virtual void playOneShot(const PlayRequest& request) = 0;
    virtual VoiceId startVoice(const PlayRequest& request) = 0;
    virtual void stopVoice(VoiceId voice) = 0;
};

std::unique_ptr<Backend> CreateBackend(BackendKind preferred = BackendKind::Auto,
                                       BackendKind* out_selected = nullptr);

} // namespace karma::audio_backend


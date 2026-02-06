#pragma once

#include "karma/platform/events.hpp"
#include "karma/renderer/types.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace karma::ui {

struct UiOverlayFrame {
    const renderer::MeshData::TextureData* texture = nullptr;
    uint64_t texture_revision = 0;
    float distance = 0.75f;
    float width = 1.2f;
    float height = 0.7f;
    bool wants_mouse_capture = false;
    bool wants_keyboard_capture = false;
};

class UiBackend {
 public:
    virtual ~UiBackend() = default;
    virtual const char* name() const = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt, const std::vector<platform::Event>& events) = 0;
    virtual void build(UiOverlayFrame& out) = 0;
};

std::unique_ptr<UiBackend> CreateSoftwareOverlayBackend();
std::unique_ptr<UiBackend> CreateImGuiBackend();

} // namespace karma::ui

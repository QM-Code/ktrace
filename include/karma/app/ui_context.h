#pragma once

#include "karma/app/ui_draw_data.h"
#include "karma/platform/events.hpp"
#include "karma/graphics/texture_handle.hpp"
#include <unordered_map>

class Input;

namespace karma::app {

class UIContext {
public:
    UIFrameInfo frame() const { return frame_; }

    UITextureHandle registerExternalTexture(const graphics::TextureHandle &texture);
    bool resolveExternalTexture(UITextureHandle handle, graphics::TextureHandle &out) const;

    UIDrawData &drawData() { return draw_data_; }
    const UIDrawData &drawData() const { return draw_data_; }

    Input *input() const { return input_; }

    float brightness() const { return brightness_; }
    void setBrightness(float brightness) { brightness_ = brightness; }

private:
    friend class EngineApp;
    void setFrameInfo(const UIFrameInfo &frame) { frame_ = frame; }
    void clearFrame() { draw_data_.clear(); }
    void setInput(Input *input) { input_ = input; }

    UIFrameInfo frame_{};
    UIDrawData draw_data_{};
    Input *input_ = nullptr;
    float brightness_ = 1.0f;
    UITextureHandle next_texture_handle_ = 1;
    std::unordered_map<UITextureHandle, graphics::TextureHandle> textures_{};
};

class UiLayer {
public:
    virtual ~UiLayer() = default;
    virtual void onFrame(UIContext &ctx) = 0;
    virtual void onEvent(const platform::Event &event) { (void)event; }
    virtual void onShutdown() {}
};

} // namespace karma::app

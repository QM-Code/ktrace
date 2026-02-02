#pragma once

#include "karma/graphics/texture_handle.hpp"
#include "karma_extras/ui/bridges/ui_render_target_bridge.hpp"

namespace ui {

class RendererBridge {
public:
    virtual ~RendererBridge() = default;
    virtual graphics::TextureHandle getRadarTexture() const = 0;
    virtual ui::UiRenderTargetBridge* getUiRenderTargetBridge() const { return nullptr; }
};

} // namespace ui

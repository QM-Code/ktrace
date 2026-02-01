#pragma once

#include "karma/graphics/texture_handle.hpp"

struct ImDrawData;
struct ImFontAtlas;

namespace ui {

class UiRenderTargetBridge {
public:
    virtual ~UiRenderTargetBridge() = default;

    virtual void* toImGuiTextureId(const graphics::TextureHandle& texture) const = 0;
    virtual void rebuildImGuiFonts(ImFontAtlas* atlas) = 0;
    virtual void renderImGuiToTarget(ImDrawData* drawData) = 0;
    virtual bool isImGuiReady() const = 0;
    virtual void ensureImGuiRenderTarget(int width, int height) = 0;
    virtual graphics::TextureHandle getImGuiRenderTarget() const = 0;
};

} // namespace ui

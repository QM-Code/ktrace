#pragma once

#include <cstdint>

#include "karma/graphics/texture_handle.hpp"
#include "karma/ui/types.hpp"

namespace ui {

class UiRenderBridge {
public:
    static RenderOutput MakeOutput(const graphics::TextureHandle &texture, bool visible) {
        RenderOutput output;
        output.texture = texture;
        output.visible = visible && texture.valid();
        return output;
    }

    static RenderOutput MakeOutput(uint64_t textureId,
                                   uint32_t width,
                                   uint32_t height,
                                   graphics::TextureFormat format,
                                   bool visible) {
        graphics::TextureHandle texture;
        texture.id = textureId;
        texture.width = width;
        texture.height = height;
        texture.format = format;
        return MakeOutput(texture, visible);
    }
};

} // namespace ui

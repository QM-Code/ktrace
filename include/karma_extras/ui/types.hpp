#pragma once

#include "karma/graphics/texture_handle.hpp"

namespace ui {

struct RenderOutput {
    graphics::TextureHandle texture{};
    bool visible = true;

    bool valid() const {
        return visible && texture.valid();
    }
};

} // namespace ui

#pragma once

struct ImVec2;
struct ImVec4;
struct ImVec4;

#include "karma/graphics/texture_handle.hpp"

namespace ui {

class ImGuiHudRadar {
public:
    void setTexture(const graphics::TextureHandle& texture);
    void draw(const ImVec2 &pos, const ImVec2 &size, const ImVec4 &backgroundColor);

private:
    graphics::TextureHandle radarTexture{};
};

} // namespace ui

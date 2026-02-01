#pragma once

#include "karma/graphics/texture_handle.hpp"
#include <cstdint>

namespace ui {

void RegisterImGuiTexture(const graphics::TextureHandle& texture);
bool LookupImGuiTexture(uint64_t id, graphics::TextureHandle& out);
void ClearImGuiTextureRegistry();

} // namespace ui


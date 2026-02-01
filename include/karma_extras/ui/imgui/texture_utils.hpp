#pragma once

#include <cstdint>
#include <cstring>

#include <imgui.h>

#include "karma/graphics/texture_handle.hpp"

namespace ui {

inline ImTextureID ToImGuiTextureId(uint64_t token) {
    if (token == 0) {
        return static_cast<ImTextureID>(0);
    }
    ImTextureID out{};
    std::memcpy(&out, &token, sizeof(ImTextureID));
    return out;
}

inline uint64_t FromImGuiTextureId(ImTextureID textureId) {
    uint64_t value = 0;
    std::memcpy(&value, &textureId, sizeof(ImTextureID));
    return value;
}

inline ImTextureID ToImGuiTextureId(const graphics::TextureHandle& texture) {
    return ToImGuiTextureId(texture.id);
}

} // namespace ui

#pragma once

#include "ui/backend.hpp"

#include <memory>

#if defined(KARMA_HAS_IMGUI)

#include <cstdint>
#include <vector>

#include <imgui.h>

namespace karma::ui::imgui {

struct RasterStats {
    uint32_t cmd_lists = 0;
    uint32_t cmds = 0;
    uint32_t vertices = 0;
    uint32_t indices = 0;
};

ImGuiKey MapKey(platform::Key key);
int MapMouseButton(platform::MouseButton button);
void PushModifiers(ImGuiIO& io, const platform::Modifiers& mods);

void RasterizeDrawData(const ImDrawData* draw_data,
                       int target_w,
                       int target_h,
                       ImTextureID atlas_texture_id,
                       const std::vector<uint8_t>& atlas_pixels,
                       int atlas_w,
                       int atlas_h,
                       std::vector<uint8_t>& target_pixels,
                       RasterStats& stats);

} // namespace karma::ui::imgui

#endif // defined(KARMA_HAS_IMGUI)

namespace karma::ui::imgui {

std::unique_ptr<BackendDriver> CreateStubBackend();

} // namespace karma::ui::imgui

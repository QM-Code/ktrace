#include "ui/frontends/imgui/hud/fps.hpp"

#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include "karma/common/i18n.hpp"

namespace ui {

void ImGuiHudFps::setVisible(bool show) {
    visible = show;
}

void ImGuiHudFps::setValue(float value) {
    fpsValue = value;
}

void ImGuiHudFps::draw(ImGuiIO &io,
                       const ImVec4 &backgroundColor,
                       const ImVec4 &textColor,
                       float textScale) {
    if (!visible) {
        return;
    }
    const float margin = 16.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImVec4 bg = backgroundColor;
    bg.x = std::clamp(bg.x, 0.0f, 1.0f);
    bg.y = std::clamp(bg.y, 0.0f, 1.0f);
    bg.z = std::clamp(bg.z, 0.0f, 1.0f);
    bg.w = std::clamp(bg.w, 0.0f, 1.0f);
    ImVec4 fg = textColor;
    fg.x = std::clamp(fg.x, 0.0f, 1.0f);
    fg.y = std::clamp(fg.y, 0.0f, 1.0f);
    fg.z = std::clamp(fg.z, 0.0f, 1.0f);
    fg.w = std::clamp(fg.w, 0.0f, 1.0f);
    const float scale = std::clamp(textScale, 0.5f, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Text, fg);
    const ImVec2 labelSize = ImGui::CalcTextSize("FPS: 999");
    const float width = labelSize.x * scale + (ImGui::GetStyle().WindowPadding.x * 2.0f) + 6.0f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
    ImGui::Begin("##FPSOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(scale);
    char fpsBuffer[32];
    std::snprintf(fpsBuffer, sizeof(fpsBuffer), "%.0f", fpsValue);
    const std::string fpsText = karma::i18n::Get().format("ui.hud.fps_label", {{"value", fpsBuffer}});
    const ImVec2 textSize = ImGui::CalcTextSize(fpsText.c_str());
    const float windowWidth = ImGui::GetWindowSize().x;
    const float centeredX = std::max(0.0f, (windowWidth - textSize.x) * 0.5f);
    ImGui::SetCursorPosX(centeredX);
    ImGui::TextUnformatted(fpsText.c_str());
    ImGui::End();
    ImGui::PopStyleColor(2);
}

} // namespace ui

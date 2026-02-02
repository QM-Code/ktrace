#include "ui/frontends/imgui/hud/scoreboard.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

void ImGuiHudScoreboard::setEntries(const std::vector<ScoreboardEntry> &entriesIn) {
    entries = entriesIn;
}

void ImGuiHudScoreboard::draw(ImGuiIO &io,
                              const ImVec4 &backgroundColor,
                              const ImVec4 &textColor,
                              float textScale) {
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
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(500, 200));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Text, fg);

    ImGui::Begin("TopLeftText", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );
    ImGui::SetWindowFontScale(scale);

    for (const auto& entry : entries) {
        const char *prefix = "  ";
        if (entry.communityAdmin) {
            prefix = "@ ";
        } else if (entry.localAdmin) {
            prefix = "* ";
        } else if (entry.registeredUser) {
            prefix = "+ ";
        }
        ImGui::Text("%s%s  (%d)", prefix, entry.name.c_str(), entry.score);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

} // namespace ui

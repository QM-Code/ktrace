#include "ui/frontends/imgui/hud/hud.hpp"

#include <algorithm>
#include <imgui.h>

namespace ui {

void ImGuiHud::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboard.setEntries(entries);
}

void ImGuiHud::setDialogText(const std::string &text) {
    dialog.setText(text);
}

void ImGuiHud::setDialogVisible(bool show) {
    dialogVisible = show;
    dialog.setVisible(show);
}

void ImGuiHud::setRadarTexture(const graphics::TextureHandle& texture) {
    radar.setTexture(texture);
}

void ImGuiHud::setScoreboardVisible(bool show) {
    scoreboardVisible = show;
}

void ImGuiHud::setChatVisible(bool show) {
    chatVisible = show;
    if (!chatVisible) {
        chat.clearSubmittedInput();
        chat.clearFocus();
    }
}

void ImGuiHud::setRadarVisible(bool show) {
    radarVisible = show;
}

void ImGuiHud::setCrosshairVisible(bool show) {
    crosshairVisible = show;
}

void ImGuiHud::setFpsValue(float value) {
    fps.setValue(value);
}

void ImGuiHud::setChatLines(const std::vector<std::string> &lines) {
    chat.setLines(lines);
}

void ImGuiHud::addConsoleLine(const std::string &playerName, const std::string &line) {
    chat.addLine(playerName, line);
}

std::string ImGuiHud::getChatInputBuffer() const {
    return chat.getSubmittedInput();
}

void ImGuiHud::clearChatInputBuffer() {
    chat.clearSubmittedInput();
}

void ImGuiHud::focusChatInput() {
    if (chatVisible) {
        chat.focusInput();
    }
}

bool ImGuiHud::getChatInputFocus() const {
    return chatVisible && chat.isFocused();
}

void ImGuiHud::setShowFps(bool show) {
    fpsVisible = show;
    fps.setVisible(show);
}

void ImGuiHud::setHudBackgroundColor(const ImVec4 &color) {
    hudBackgroundColor = color;
}

void ImGuiHud::setHudTextColor(const ImVec4 &color) {
    hudTextColor = color;
}

void ImGuiHud::setHudTextScale(float scale) {
    hudTextScale = scale;
}

void ImGuiHud::draw(ImGuiIO &io, ImFont *bigFont) {
    if (scoreboardVisible) {
        scoreboard.draw(io, hudBackgroundColor, hudTextColor, hudTextScale);
    }

    const float margin = 12.0f;
    const float panelHeight = 260.0f;
    const float inputHeight = 34.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 vpPos  = vp->Pos;
    ImVec2 vpSize = vp->Size;

    float radarSize = std::clamp(vpSize.y * 0.35f, 240.0f, 460.0f);
    radarSize = std::min(radarSize, vpSize.y - 2.0f * margin);
    radarSize = std::min(radarSize, vpSize.x - 2.0f * margin);

    const ImVec2 radarPos = ImVec2(vpPos.x + margin, vpPos.y + vpSize.y - margin - radarSize);
    const ImVec2 radarWindowSize = ImVec2(radarSize, radarSize);
    if (radarVisible) {
        radar.draw(radarPos, radarWindowSize, hudBackgroundColor);
    }

    const float consoleLeft = vpPos.x + margin + (radarVisible ? radarSize + margin : 0.0f);
    const float consoleWidth = std::max(50.0f, vpSize.x - (radarVisible ? (radarSize + 3.0f * margin) : (2.0f * margin)));
    ImVec2 pos  = ImVec2(consoleLeft, vpPos.y + vpSize.y - margin - panelHeight);
    ImVec2 size = ImVec2(consoleWidth, panelHeight);

    if (chatVisible) {
        chat.draw(pos, size, inputHeight, hudBackgroundColor, hudTextColor, hudTextScale);
    }

    dialog.draw(io, bigFont);
    if (crosshairVisible) {
        crosshair.draw(io);
    }
    if (fpsVisible) {
        fps.draw(io, hudBackgroundColor, hudTextColor, hudTextScale);
    }
}

} // namespace ui

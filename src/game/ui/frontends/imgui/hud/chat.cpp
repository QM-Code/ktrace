#include "ui/frontends/imgui/hud/chat.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

void ImGuiHudChat::addLine(const std::string &playerName, const std::string &line) {
    std::string displayName = playerName;
    if (!displayName.empty() && displayName.front() != '[') {
        displayName = "[" + displayName + "]";
    }
    std::string fullLine = displayName.empty() ? line : (displayName + " " + line);
    consoleLines.push_back(fullLine);
}

void ImGuiHudChat::setLines(const std::vector<std::string> &lines) {
    consoleLines = lines;
}

std::string ImGuiHudChat::getSubmittedInput() const {
    return submittedInputBuffer;
}

void ImGuiHudChat::clearSubmittedInput() {
    submittedInputBuffer.clear();
}

void ImGuiHudChat::focusInput() {
    chatFocus = true;
}

void ImGuiHudChat::clearFocus() {
    chatFocus = false;
}

bool ImGuiHudChat::isFocused() const {
    return chatFocus;
}

void ImGuiHudChat::draw(const ImVec2 &pos,
                        const ImVec2 &size,
                        float inputHeight,
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
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Text, fg);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##BottomConsole", nullptr, flags);
    ImGui::SetWindowFontScale(scale);

    const float footer = inputHeight;
    ImGui::BeginChild("##ConsoleScroll",
                      ImVec2(0, -footer),
                      true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::SetWindowFontScale(scale);

    const float scrollY = ImGui::GetScrollY();
    const float scrollMaxY = ImGui::GetScrollMaxY();
    const float atBottomEpsilon = 2.0f;
    if (scrollMaxY > 0.0f) {
        if (scrollY < scrollMaxY - atBottomEpsilon) {
            autoScroll = false;
        } else {
            autoScroll = true;
        }
    }

    for (const std::string& line : consoleLines) {
        ImGui::TextUnformatted(line.c_str());
    }

    if (autoScroll) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::InvisibleButton("##focus_sink", ImVec2(1, 1));

    if (chatFocus) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::PushItemWidth(-1);
    bool submitted = ImGui::InputTextWithHint(
        "##ChatHint",
        "press T to type",
        chatInputBuffer.data(),
        chatInputBuffer.size(),
        ImGuiInputTextFlags_EnterReturnsTrue
    );

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        chatInputBuffer.fill(0);
        submittedInputBuffer.clear();
        chatFocus = false;
    }
    
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    if (submitted) {
        submittedInputBuffer = std::string(chatInputBuffer.data());
        chatInputBuffer.fill(0);
        chatFocus = false;
    }
}

} // namespace ui

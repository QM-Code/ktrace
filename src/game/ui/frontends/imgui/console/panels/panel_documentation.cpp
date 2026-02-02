#include "ui/frontends/imgui/console/console.hpp"

namespace ui {

void ConsoleView::drawDocumentationPanel(const MessageColors &colors) const {
    ImGui::PushStyleColor(ImGuiCol_Text, colors.notice);
    ImGui::TextWrapped("%s", "This space intentionally left blank.");
    ImGui::PopStyleColor();
}


} // namespace ui

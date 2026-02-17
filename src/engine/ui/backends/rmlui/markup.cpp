#include "ui/backends/rmlui/internal.hpp"

#if defined(KARMA_HAS_RMLUI)

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace karma::ui::rmlui {

std::string EscapeText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string BuildBaseDocument() {
    return R"(<rml>
<head>
<style>
body {
    margin: 0px;
    background-color: rgba(0,0,0,0);
    color: #d7dce8;
    font-family: "Google Sans";
    font-size: 16px;
}
#root {
    position: absolute;
    left: 0px;
    top: 0px;
    width: 100%;
    height: 100%;
}
.panel {
    position: absolute;
    display: inline-block;
    padding: 10px 12px;
    border-width: 1px;
    border-style: solid;
    border-color: #7084a0;
    background-color: rgba(12, 18, 26, 210);
}
.title {
    margin-bottom: 8px;
    font-size: 20px;
    color: #8ec8ff;
}
.line {
    margin-bottom: 2px;
    white-space: pre;
}
</style>
</head>
<body>
<div id="root"></div>
</body>
</rml>)";
}

std::string BuildPanelsMarkup(const std::vector<UiDrawContext::TextPanel>& text_panels) {
    auto estimate_panel_size = [](const UiDrawContext::TextPanel& panel) -> std::pair<int, int> {
        size_t max_chars = panel.title.size();
        for (const auto& line : panel.lines) {
            max_chars = std::max(max_chars, line.size());
        }
        // Conservative layout estimate for the temporary debug overlay.
        const int text_width = static_cast<int>(max_chars * 9u);
        const int width = std::max(280, text_width + 24);
        const int height = std::max(96, 36 + static_cast<int>(panel.lines.size()) * 20 + 12);
        return {width, height};
    };

    std::string markup;
    markup.reserve(1024);
    for (const auto& panel : text_panels) {
        const auto [panel_width, panel_height] = estimate_panel_size(panel);
        const int bg_alpha = static_cast<int>(std::lround(std::clamp(panel.bg_alpha, 0.0f, 1.0f) * 255.0f));
        markup.append("<div class=\"panel\" style=\"left:");
        markup.append(std::to_string(static_cast<int>(std::lround(panel.x))));
        markup.append("px;top:");
        markup.append(std::to_string(static_cast<int>(std::lround(panel.y))));
        markup.append("px;width:");
        markup.append(std::to_string(panel_width));
        markup.append("px;height:");
        markup.append(std::to_string(panel_height));
        markup.append("px;background-color:rgba(12,18,26,");
        markup.append(std::to_string(bg_alpha));
        markup.append(");\">");
        markup.append("<div class=\"title\">");
        markup.append(EscapeText(panel.title));
        markup.append("</div>");
        for (const auto& line : panel.lines) {
            markup.append("<div class=\"line\">");
            markup.append(EscapeText(line));
            markup.append("</div>");
        }
        markup.append("</div>");
    }
    return markup;
}

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)

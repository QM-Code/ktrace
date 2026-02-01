#include "ui/frontends/rmlui/hud/scoreboard.hpp"

#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cstdio>

namespace ui {
namespace {

std::string formatRgba(const std::array<float, 4> &color) {
    const float r = std::clamp(color[0], 0.0f, 1.0f);
    const float g = std::clamp(color[1], 0.0f, 1.0f);
    const float b = std::clamp(color[2], 0.0f, 1.0f);
    const float a = std::clamp(color[3], 0.0f, 1.0f);
    const int ri = static_cast<int>(r * 255.0f + 0.5f);
    const int gi = static_cast<int>(g * 255.0f + 0.5f);
    const int bi = static_cast<int>(b * 255.0f + 0.5f);
    const int ai = static_cast<int>(a * 255.0f + 0.5f);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", ri, gi, bi, ai);
    return buffer;
}

} // namespace

void RmlUiHudScoreboard::bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn) {
    emojiMarkup = std::move(emojiMarkupIn);
    container = nullptr;
    if (!document) {
        return;
    }
    container = document->GetElementById("hud-scoreboard");
    if (container) {
        container->SetClass("hidden", !visible);
        container->SetProperty("background-color", formatRgba(backgroundColor));
        container->SetProperty("color", formatRgba(textColor));
        const float clamped = std::clamp(textScale, 0.5f, 3.0f);
        container->SetProperty("font-size", std::to_string(18.0f * clamped) + "px");
    }
    rebuild(document);
}

void RmlUiHudScoreboard::setEntries(const std::vector<ScoreboardEntry> &entriesIn) {
    entries = entriesIn;
    if (container) {
        rebuild(container->GetOwnerDocument());
    }
}

void RmlUiHudScoreboard::setVisible(bool visibleIn) {
    visible = visibleIn;
    if (container) {
        container->SetClass("hidden", !visible);
    }
}

bool RmlUiHudScoreboard::isVisible() const {
    return visible;
}

void RmlUiHudScoreboard::setBackgroundColor(const std::array<float, 4> &color) {
    backgroundColor = color;
    if (container) {
        container->SetProperty("background-color", formatRgba(backgroundColor));
    }
}

void RmlUiHudScoreboard::setTextColor(const std::array<float, 4> &color) {
    textColor = color;
    if (container) {
        const std::string rgba = formatRgba(textColor);
        container->SetProperty("color", rgba);
        for (int i = 0; i < container->GetNumChildren(); ++i) {
            if (auto *child = container->GetChild(i)) {
                child->SetProperty("color", rgba);
            }
        }
    }
}

void RmlUiHudScoreboard::setTextScale(float scale) {
    textScale = scale;
    const float clamped = std::clamp(textScale, 0.5f, 3.0f);
    const std::string sizeValue = std::to_string(18.0f * clamped) + "px";
    if (container) {
        container->SetProperty("font-size", sizeValue);
        for (int i = 0; i < container->GetNumChildren(); ++i) {
            if (auto *child = container->GetChild(i)) {
                child->SetProperty("font-size", sizeValue);
            }
        }
    }
}

void RmlUiHudScoreboard::rebuild(Rml::ElementDocument *document) {
    if (!container || !document) {
        return;
    }
    while (auto *child = container->GetFirstChild()) {
        container->RemoveChild(child);
    }
    for (const auto &entry : entries) {
        const char *prefix = "  ";
        if (entry.communityAdmin) {
            prefix = "@ ";
        } else if (entry.localAdmin) {
            prefix = "* ";
        } else if (entry.registeredUser) {
            prefix = "+ ";
        }
        std::string line = std::string(prefix) + entry.name + "  (" + std::to_string(entry.score) + ")";
        auto element = document->CreateElement("div");
        element->SetClass("hud-scoreboard-line", true);
        element->SetInnerRML(emojiMarkup ? emojiMarkup(line) : line);
        element->SetProperty("color", formatRgba(textColor));
        const float clamped = std::clamp(textScale, 0.5f, 3.0f);
        element->SetProperty("font-size", std::to_string(18.0f * clamped) + "px");
        container->AppendChild(std::move(element));
    }
}

} // namespace ui

#include "ui/frontends/rmlui/hud/radar.hpp"

#include <RmlUi/Core/ElementDocument.h>
#include <algorithm>
#include <cstdio>

namespace ui {

void RmlUiHudRadar::bind(Rml::ElementDocument *document) {
    panel = nullptr;
    image = nullptr;
    if (!document) {
        return;
    }
    panel = document->GetElementById("hud-radar-panel");
    image = document->GetElementById("hud-radar-image");
    if (panel) {
        panel->SetClass("hidden", !visible);
    }
    if (image) {
        if (!texture.valid()) {
            image->SetAttribute("src", "");
        } else {
            std::string src = "texid:" + std::to_string(texture.id);
            if (texture.width > 0 && texture.height > 0) {
                src += ":" + std::to_string(texture.width) + "x" + std::to_string(texture.height);
            }
            image->SetAttribute("src", src);
        }
    }
}

void RmlUiHudRadar::setTexture(const graphics::TextureHandle& textureIn) {
    texture = textureIn;
    if (image) {
        if (!texture.valid()) {
            image->SetAttribute("src", "");
        } else {
            std::string src = "texid:" + std::to_string(texture.id);
            if (texture.width > 0 && texture.height > 0) {
                src += ":" + std::to_string(texture.width) + "x" + std::to_string(texture.height);
            }
            image->SetAttribute("src", src);
        }
    }
}

void RmlUiHudRadar::setBackgroundColor(const std::array<float, 4> &color) {
    if (!panel) {
        return;
    }
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
    panel->SetProperty("background-color", buffer);
}

void RmlUiHudRadar::setVisible(bool visibleIn) {
    visible = visibleIn;
    if (panel) {
        panel->SetClass("hidden", !visible);
    }
}

bool RmlUiHudRadar::isVisible() const {
    return visible;
}

} // namespace ui

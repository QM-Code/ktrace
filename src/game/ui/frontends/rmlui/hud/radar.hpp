#pragma once

#include <array>
#include <string>

#include <RmlUi/Core/Element.h>

#include "karma/graphics/texture_handle.hpp"

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiHudRadar {
public:
    void bind(Rml::ElementDocument *document);
    void setTexture(const graphics::TextureHandle& texture);
    void setBackgroundColor(const std::array<float, 4> &color);
    void setVisible(bool visible);
    bool isVisible() const;

private:
    Rml::Element *panel = nullptr;
    Rml::Element *image = nullptr;
    graphics::TextureHandle texture{};
    bool visible = true;
};

} // namespace ui

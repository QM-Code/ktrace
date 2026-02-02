#pragma once

#include <functional>
#include <string>

#include <RmlUi/Core/Element.h>

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiHudDialog {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    void bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn);
    void setText(const std::string &text);
    void show(bool visible);
    bool isVisible() const { return visible; }

private:
    Rml::Element *overlay = nullptr;
    Rml::Element *textElement = nullptr;
    std::string currentText;
    bool visible = false;
    EmojiMarkupFn emojiMarkup;
};

} // namespace ui

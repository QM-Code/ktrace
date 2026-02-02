#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>

#include "ui/core/types.hpp"

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiHudScoreboard {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    void bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn);
    void setEntries(const std::vector<ScoreboardEntry> &entries);
    void setVisible(bool visible);
    bool isVisible() const;
    void setBackgroundColor(const std::array<float, 4> &color);
    void setTextColor(const std::array<float, 4> &color);
    void setTextScale(float scale);

private:
    Rml::Element *container = nullptr;
    std::vector<ScoreboardEntry> entries;
    EmojiMarkupFn emojiMarkup;
    bool visible = true;
    std::array<float, 4> backgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> textColor{1.0f, 1.0f, 1.0f, 1.0f};
    float textScale = 1.0f;

    void rebuild(Rml::ElementDocument *document);
};

} // namespace ui

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

namespace Rml {
class ElementDocument;
class EventListener;
}

namespace ui {

class RmlUiHudChat {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    void bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn);
    void update();

    void addLine(const std::string &line);
    void setLines(const std::vector<std::string> &linesIn);
    std::string getSubmittedInput() const;
    void clearSubmittedInput();

    void focusInput();
    bool isFocused() const;
    void setVisible(bool visible);
    bool isVisible() const;
    void setBackgroundColor(const std::array<float, 4> &color);
    void setTextColor(const std::array<float, 4> &color);
    void setTextScale(float scale);
    bool consumeSuppressNextChar();
    void handleInputEvent(Rml::Event &event);

private:
    Rml::Element *panel = nullptr;
    Rml::Element *log = nullptr;
    Rml::Element *logContent = nullptr;
    Rml::Element *input = nullptr;
    std::unique_ptr<Rml::EventListener> inputListener;

    std::vector<std::string> lines;
    std::string submittedInput;
    bool visible = true;
    bool focused = false;
    bool autoScroll = true;
    bool pendingScroll = false;
    bool suppressNextChar = false;
    std::array<float, 4> backgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> textColor{1.0f, 1.0f, 1.0f, 1.0f};
    float textScale = 1.0f;

    EmojiMarkupFn emojiMarkup;

    void rebuildLines(Rml::ElementDocument *document);
};

} // namespace ui

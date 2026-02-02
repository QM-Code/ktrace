#include "ui/frontends/rmlui/hud/chat.hpp"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Input.h>
#include <utility>

#include <algorithm>
#include <cstdio>

namespace ui {
namespace {

class ChatInputListener final : public Rml::EventListener {
public:
    explicit ChatInputListener(RmlUiHudChat *chatIn) : chat(chatIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (chat) {
            chat->handleInputEvent(event);
        }
    }

private:
    RmlUiHudChat *chat = nullptr;
};

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

void RmlUiHudChat::bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn) {
    emojiMarkup = std::move(emojiMarkupIn);
    panel = nullptr;
    log = nullptr;
    logContent = nullptr;
    input = nullptr;
    inputListener.reset();
    if (!document) {
        return;
    }
    panel = document->GetElementById("hud-chat-panel");
    log = document->GetElementById("hud-chat-log");
    logContent = document->GetElementById("hud-chat-log-content");
    input = document->GetElementById("hud-chat-input");

    if (panel) {
        panel->SetClass("hidden", !visible);
        panel->SetProperty("background-color", formatRgba(backgroundColor));
    }

    if (input) {
        inputListener = std::make_unique<ChatInputListener>(this);
        input->AddEventListener("keydown", inputListener.get());
        input->AddEventListener("focus", inputListener.get());
        input->AddEventListener("blur", inputListener.get());
    }

    setTextColor(textColor);
    setTextScale(textScale);
    rebuildLines(document);
}

void RmlUiHudChat::update() {
    if (!log) {
        return;
    }
    const float scrollHeight = log->GetScrollHeight();
    const float viewHeight = log->GetOffsetHeight();
    const float scrollMax = std::max(0.0f, scrollHeight - viewHeight);
    const float scrollTop = log->GetScrollTop();
    const float atBottomEpsilon = 2.0f;
    if (scrollMax > 0.0f) {
        autoScroll = (scrollTop >= scrollMax - atBottomEpsilon);
    } else {
        autoScroll = true;
    }
    if (pendingScroll || autoScroll) {
        log->SetScrollTop(scrollMax);
        pendingScroll = false;
    }
}

void RmlUiHudChat::addLine(const std::string &line) {
    std::size_t start = 0;
    while (start <= line.size()) {
        std::size_t end = line.find('\n', start);
        if (end == std::string::npos) {
            end = line.size();
        }
        std::string segment = line.substr(start, end - start);
        if (!segment.empty() && segment.back() == '\r') {
            segment.pop_back();
        }
        lines.push_back(segment);
        if (logContent && logContent->GetOwnerDocument()) {
            auto lineElement = logContent->GetOwnerDocument()->CreateElement("div");
            lineElement->SetClass("hud-chat-line", true);
            lineElement->SetInnerRML(emojiMarkup ? emojiMarkup(segment) : segment);
            lineElement->SetProperty("color", formatRgba(textColor));
            const float clamped = std::clamp(textScale, 0.5f, 3.0f);
            lineElement->SetProperty("font-size", std::to_string(15.0f * clamped) + "px");
            logContent->AppendChild(std::move(lineElement));
            pendingScroll = true;
        }
        if (end >= line.size()) {
            break;
        }
        start = end + 1;
    }
}

void RmlUiHudChat::setLines(const std::vector<std::string> &linesIn) {
    if (linesIn == lines) {
        return;
    }
    lines = linesIn;
    Rml::ElementDocument *document = logContent ? logContent->GetOwnerDocument() : nullptr;
    rebuildLines(document);
}

std::string RmlUiHudChat::getSubmittedInput() const {
    return submittedInput;
}

void RmlUiHudChat::clearSubmittedInput() {
    submittedInput.clear();
}

void RmlUiHudChat::focusInput() {
    focused = true;
    suppressNextChar = true;
    if (input) {
        input->Focus();
    }
}

bool RmlUiHudChat::isFocused() const {
    return focused;
}

void RmlUiHudChat::setVisible(bool visibleIn) {
    visible = visibleIn;
    if (panel) {
        panel->SetClass("hidden", !visible);
    }
    if (!visible) {
        focused = false;
    }
}

bool RmlUiHudChat::isVisible() const {
    return visible;
}

void RmlUiHudChat::setBackgroundColor(const std::array<float, 4> &color) {
    backgroundColor = color;
    if (panel) {
        panel->SetProperty("background-color", formatRgba(backgroundColor));
    }
}

void RmlUiHudChat::setTextColor(const std::array<float, 4> &color) {
    textColor = color;
    const std::string rgba = formatRgba(textColor);
    if (log) {
        log->SetProperty("color", rgba);
    }
    if (logContent) {
        logContent->SetProperty("color", rgba);
        for (int i = 0; i < logContent->GetNumChildren(); ++i) {
            if (auto *child = logContent->GetChild(i)) {
                child->SetProperty("color", rgba);
            }
        }
    }
    if (input) {
        input->SetProperty("color", rgba);
    }
}

void RmlUiHudChat::setTextScale(float scale) {
    textScale = scale;
    const float clamped = std::clamp(textScale, 0.5f, 3.0f);
    const float logSize = 15.0f * clamped;
    const float inputSize = 15.0f * clamped;
    const std::string logSizeValue = std::to_string(logSize) + "px";
    const std::string inputSizeValue = std::to_string(inputSize) + "px";
    if (log) {
        log->SetProperty("font-size", logSizeValue);
    }
    if (logContent) {
        for (int i = 0; i < logContent->GetNumChildren(); ++i) {
            if (auto *child = logContent->GetChild(i)) {
                child->SetProperty("font-size", logSizeValue);
            }
        }
    }
    if (input) {
        input->SetProperty("font-size", inputSizeValue);
    }
}


bool RmlUiHudChat::consumeSuppressNextChar() {
    if (!suppressNextChar) {
        return false;
    }
    suppressNextChar = false;
    return true;
}

void RmlUiHudChat::handleInputEvent(Rml::Event &event) {
    if (event.GetType() == "focus") {
        focused = true;
        return;
    }
    if (event.GetType() == "blur") {
        focused = false;
        return;
    }
    if (!input) {
        return;
    }
    auto *control = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(input);
    if (!control) {
        return;
    }
    const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
    if (keyIdentifier == Rml::Input::KI_ESCAPE) {
        control->SetValue("");
        submittedInput.clear();
        focused = false;
        return;
    }
    if (keyIdentifier == Rml::Input::KI_RETURN || keyIdentifier == Rml::Input::KI_NUMPADENTER) {
        const Rml::String value = control->GetValue();
        submittedInput = value;
        control->SetValue("");
        focused = true;
        control->Focus();
    }
}

void RmlUiHudChat::rebuildLines(Rml::ElementDocument *document) {
    if (!logContent || !document) {
        return;
    }
    while (auto *child = logContent->GetFirstChild()) {
        logContent->RemoveChild(child);
    }
    for (const auto &line : lines) {
        auto lineElement = document->CreateElement("div");
        lineElement->SetClass("hud-chat-line", true);
        lineElement->SetInnerRML(emojiMarkup ? emojiMarkup(line) : line);
        lineElement->SetProperty("color", formatRgba(textColor));
        const float clamped = std::clamp(textScale, 0.5f, 3.0f);
        lineElement->SetProperty("font-size", std::to_string(15.0f * clamped) + "px");
        logContent->AppendChild(std::move(lineElement));
    }
    pendingScroll = true;
}

} // namespace ui

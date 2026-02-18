#include "ui/frontends/rmlui/hud/hud.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <utility>

#include <algorithm>
#include <cstdio>

#include "karma/common/i18n/i18n.hpp"
#include "ui/frontends/rmlui/translate.hpp"

namespace ui {

RmlUiHud::RmlUiHud() = default;

RmlUiHud::~RmlUiHud() {
    unload();
}

void RmlUiHud::load(Rml::Context *contextIn, const std::string &pathIn, EmojiMarkupFn emojiMarkupIn) {
    unload();
    context = contextIn;
    path = pathIn;
    emojiMarkup = std::move(emojiMarkupIn);
    if (!context || path.empty()) {
        return;
    }
    document = context->LoadDocument(path);
    if (!document) {
        return;
    }
    rmlui::ApplyTranslations(document, karma::common::i18n::Get());
    lastLanguage = karma::common::i18n::Get().language();
    bindElements();
    document->Show();
}

void RmlUiHud::unload() {
    if (document) {
        document->Close();
        document = nullptr;
        if (context) {
            context->Update();
        }
    }
    context = nullptr;
    path.clear();
    emojiMarkup = nullptr;
}

void RmlUiHud::show() {
    if (document && !document->IsVisible()) {
        document->Show();
    }
}

void RmlUiHud::hide() {
    if (document && document->IsVisible()) {
        document->Hide();
    }
}

bool RmlUiHud::isVisible() const {
    return document && document->IsVisible();
}

void RmlUiHud::update() {
    if (document) {
        const std::string currentLanguage = karma::common::i18n::Get().language();
        if (currentLanguage != lastLanguage) {
            rmlui::ApplyTranslations(document, karma::common::i18n::Get());
            lastLanguage = currentLanguage;
            lastFpsInt = -1;
            setFpsValue(lastFps);
        }
    }
    chat.update();
}

void RmlUiHud::setDialogText(const std::string &text) {
    dialog.setText(text);
}

void RmlUiHud::setDialogVisible(bool show) {
    dialog.show(show);
}

void RmlUiHud::setChatLines(const std::vector<std::string> &lines) {
    chat.setLines(lines);
}

void RmlUiHud::addChatLine(const std::string &line) {
    chat.addLine(line);
}

std::string RmlUiHud::getSubmittedChatInput() const {
    return chat.getSubmittedInput();
}

void RmlUiHud::clearSubmittedChatInput() {
    chat.clearSubmittedInput();
}

void RmlUiHud::focusChatInput() {
    chat.focusInput();
}

bool RmlUiHud::isChatFocused() const {
    return chatVisible && chat.isFocused();
}


bool RmlUiHud::consumeSuppressNextChatChar() {
    return chat.consumeSuppressNextChar();
}

void RmlUiHud::handleChatInputEvent(Rml::Event &event) {
    chat.handleInputEvent(event);
}

void RmlUiHud::setRadarTexture(const graphics::TextureHandle& texture) {
    radar.setTexture(texture);
}

void RmlUiHud::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboard.setEntries(entries);
}

void RmlUiHud::setScoreboardVisible(bool visible) {
    scoreboardVisible = visible;
    scoreboard.setVisible(visible);
}

void RmlUiHud::setChatVisible(bool visible) {
    chatVisible = visible;
    chat.setVisible(visible);
}

void RmlUiHud::setRadarVisible(bool visible) {
    radarVisible = visible;
    radar.setVisible(visible);
}

void RmlUiHud::setCrosshairVisible(bool visible) {
    crosshairVisible = visible;
    crosshair.setVisible(visible);
}

void RmlUiHud::setHudBackgroundColor(const std::array<float, 4> &color) {
    hudBackgroundColor = color;
    chat.setBackgroundColor(hudBackgroundColor);
    scoreboard.setBackgroundColor(hudBackgroundColor);
    radar.setBackgroundColor(hudBackgroundColor);
    if (fpsElement) {
        const float r = std::clamp(hudBackgroundColor[0], 0.0f, 1.0f);
        const float g = std::clamp(hudBackgroundColor[1], 0.0f, 1.0f);
        const float b = std::clamp(hudBackgroundColor[2], 0.0f, 1.0f);
        const float a = std::clamp(hudBackgroundColor[3], 0.0f, 1.0f);
        const int ri = static_cast<int>(r * 255.0f + 0.5f);
        const int gi = static_cast<int>(g * 255.0f + 0.5f);
        const int bi = static_cast<int>(b * 255.0f + 0.5f);
        const int ai = static_cast<int>(a * 255.0f + 0.5f);
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", ri, gi, bi, ai);
        fpsElement->SetProperty("background-color", buffer);
    }
}

void RmlUiHud::setHudTextColor(const std::array<float, 4> &color) {
    hudTextColor = color;
    chat.setTextColor(hudTextColor);
    scoreboard.setTextColor(hudTextColor);
    if (fpsElement) {
        const float r = std::clamp(hudTextColor[0], 0.0f, 1.0f);
        const float g = std::clamp(hudTextColor[1], 0.0f, 1.0f);
        const float b = std::clamp(hudTextColor[2], 0.0f, 1.0f);
        const float a = std::clamp(hudTextColor[3], 0.0f, 1.0f);
        const int ri = static_cast<int>(r * 255.0f + 0.5f);
        const int gi = static_cast<int>(g * 255.0f + 0.5f);
        const int bi = static_cast<int>(b * 255.0f + 0.5f);
        const int ai = static_cast<int>(a * 255.0f + 0.5f);
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", ri, gi, bi, ai);
        fpsElement->SetProperty("color", buffer);
    }
}

void RmlUiHud::setHudTextScale(float scale) {
    hudTextScale = scale;
    chat.setTextScale(hudTextScale);
    scoreboard.setTextScale(hudTextScale);
    if (fpsElement) {
        const float clamped = std::clamp(hudTextScale, 0.5f, 3.0f);
        fpsElement->SetProperty("font-size", std::to_string(14.0f * clamped) + "px");
        const float baseWidth = 76.0f;
        fpsElement->SetProperty("min-width", std::to_string(baseWidth * clamped) + "px");
        fpsElement->SetProperty("text-align", "center");
    }
}

void RmlUiHud::setFpsVisible(bool visible) {
    if (visible == fpsVisible) {
        return;
    }
    fpsVisible = visible;
    if (fpsElement) {
        fpsElement->SetClass("hidden", !visible);
    }
}

void RmlUiHud::setFpsValue(float fps) {
    lastFps = fps;
    if (fpsElement) {
        const int fpsInt = static_cast<int>(fps + 0.5f);
        if (fpsInt == lastFpsInt) {
            return;
        }
        lastFpsInt = fpsInt;
        const std::string value = std::to_string(fpsInt);
        const std::string fpsText = karma::common::i18n::Get().format("ui.hud.fps_label", {{"value", value}});
        fpsElement->SetInnerRML(fpsText);
    }
}

void RmlUiHud::setQuickMenuVisible(bool visible) {
    quickMenu.show(visible);
}

std::optional<QuickMenuAction> RmlUiHud::consumeQuickMenuAction() {
    return quickMenu.consumeAction();
}

void RmlUiHud::bindElements() {
    if (!document) {
        return;
    }
    dialog.bind(document, emojiMarkup);
    quickMenu.bind(document);
    chat.bind(document, emojiMarkup);
    crosshair.bind(document);
    radar.bind(document);
    scoreboard.bind(document, emojiMarkup);
    setHudBackgroundColor(hudBackgroundColor);
    setHudTextColor(hudTextColor);
    setHudTextScale(hudTextScale);
    chat.setVisible(chatVisible);
    scoreboard.setVisible(scoreboardVisible);
    radar.setVisible(radarVisible);
    crosshair.setVisible(crosshairVisible);
    fpsElement = document->GetElementById("hud-fps");
    fpsVisible = fpsElement && !fpsElement->IsClassSet("hidden");
    setFpsValue(lastFps);
}

} // namespace ui

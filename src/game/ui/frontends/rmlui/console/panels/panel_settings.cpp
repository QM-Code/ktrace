#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include "karma/common/config/store.hpp"
#include "karma/common/i18n/i18n.hpp"
#include "ui/console/status_banner.hpp"
#include "spdlog/spdlog.h"

namespace ui {
namespace {

const std::vector<std::string> kLanguageCodes = {
    "en", "es", "fr", "de", "pt", "ru", "jp", "zh", "ko", "it", "hi", "ar"
};

std::string escapeRmlText(std::string_view text) {
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

} // namespace

void RmlUiPanelSettings::DebouncedCommit::markChanged() {
    pending = true;
    lastChange = std::chrono::steady_clock::now();
}

class RmlUiPanelSettings::BrightnessListener final : public Rml::EventListener {
public:
    explicit BrightnessListener(RmlUiPanelSettings *panelIn) : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        auto *target = event.GetTargetElement();
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!input) {
            return;
        }
        const std::string value = input->GetValue();
        try {
            const float brightness = std::stof(value);
        if (event.GetType() == "input" || event.GetType() == "change") {
            panel->applyRenderBrightness(brightness, false);
            panel->syncRenderBrightnessLabel();
            panel->setRenderBrightnessDragging(true);
            panel->brightnessCommit.markChanged();
        }
        } catch (...) {
            return;
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::LanguageListener final : public Rml::EventListener {
public:
    explicit LanguageListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (panel->isLanguageSelectionSuppressed()) {
            return;
        }
        auto *element = event.GetTargetElement();
        auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(element);
        if (!select) {
            return;
        }
        panel->applyLanguageSelection(select->GetValue());
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudToggleListener final : public Rml::EventListener {
public:
    explicit HudToggleListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (panel) {
            panel->handleHudToggle(event.GetTargetElement());
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::RenderToggleListener final : public Rml::EventListener {
public:
    explicit RenderToggleListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (panel) {
            panel->handleRenderToggle(event.GetTargetElement());
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudBackgroundListener final : public Rml::EventListener {
public:
    explicit HudBackgroundListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        auto *target = event.GetTargetElement();
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!input) {
            return;
        }
        if (event.GetType() == "input" || event.GetType() == "change") {
            panel->hudBackgroundDragging = true;
            panel->handleHudBackgroundInput(false);
            panel->hudBackgroundCommit.markChanged();
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudBackgroundToggleListener final : public Rml::EventListener {
public:
    explicit HudBackgroundToggleListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (event.GetType() == "click") {
            panel->handleHudBackgroundToggle();
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudTextListener final : public Rml::EventListener {
public:
    explicit HudTextListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        auto *target = event.GetTargetElement();
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!input) {
            return;
        }
        if (event.GetType() == "input" || event.GetType() == "change") {
            panel->hudTextDragging = true;
            panel->handleHudTextInput(false);
            panel->hudTextCommit.markChanged();
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudTextToggleListener final : public Rml::EventListener {
public:
    explicit HudTextToggleListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (event.GetType() == "click") {
            panel->handleHudTextToggle();
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudTextSizeListener final : public Rml::EventListener {
public:
    explicit HudTextSizeListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        auto *target = event.GetTargetElement();
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!input) {
            return;
        }
        if (event.GetType() == "input" || event.GetType() == "change") {
            panel->hudTextSizeDragging = true;
            panel->handleHudTextSizeInput(false);
            panel->hudTextSizeCommit.markChanged();
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

RmlUiPanelSettings::RmlUiPanelSettings()
    : RmlUiPanel("settings", "client/ui/console_panel_settings.rml") {}

void RmlUiPanelSettings::setUserConfigPath(const std::string &path) {
    (void)path;
    settingsModel.loaded = false;
    settingsModel.render.reset();
    settingsModel.hud.reset();
    syncSettingsFromConfig();
    syncRenderBrightnessControls(true);
    syncHudControls();
}

void RmlUiPanelSettings::setLanguageCallback(std::function<void(const std::string &)> callback) {
    languageCallback = std::move(callback);
}

void RmlUiPanelSettings::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    statusLabel = document->GetElementById("settings-status");
    brightnessSlider = document->GetElementById("settings-brightness-slider");
    brightnessValueLabel = document->GetElementById("settings-brightness-value");
    hudBackgroundSwatch = document->GetElementById("settings-hud-background-swatch");
    hudBackgroundPopup = document->GetElementById("settings-hud-background-popup");
    hudBackgroundEditButton = document->GetElementById("settings-hud-background-edit");
    hudBackgroundRSlider = document->GetElementById("settings-hud-background-r");
    hudBackgroundGSlider = document->GetElementById("settings-hud-background-g");
    hudBackgroundBSlider = document->GetElementById("settings-hud-background-b");
    hudBackgroundASlider = document->GetElementById("settings-hud-background-a");
    hudBackgroundRValue = document->GetElementById("settings-hud-background-r-value");
    hudBackgroundGValue = document->GetElementById("settings-hud-background-g-value");
    hudBackgroundBValue = document->GetElementById("settings-hud-background-b-value");
    hudBackgroundAValue = document->GetElementById("settings-hud-background-a-value");
    hudTextSwatch = document->GetElementById("settings-hud-text-swatch");
    hudTextPopup = document->GetElementById("settings-hud-text-popup");
    hudTextEditButton = document->GetElementById("settings-hud-text-edit");
    hudTextRSlider = document->GetElementById("settings-hud-text-r");
    hudTextGSlider = document->GetElementById("settings-hud-text-g");
    hudTextBSlider = document->GetElementById("settings-hud-text-b");
    hudTextRValue = document->GetElementById("settings-hud-text-r-value");
    hudTextGValue = document->GetElementById("settings-hud-text-g-value");
    hudTextBValue = document->GetElementById("settings-hud-text-b-value");
    hudTextSizeSlider = document->GetElementById("settings-hud-text-size-slider");
    hudTextSizeValue = document->GetElementById("settings-hud-text-size-value");
    languageSelect = document->GetElementById("settings-language-select");
    hudScoreboardToggle.on = document->GetElementById("settings-hud-scoreboard-on");
    hudScoreboardToggle.off = document->GetElementById("settings-hud-scoreboard-off");
    hudChatToggle.on = document->GetElementById("settings-hud-chat-on");
    hudChatToggle.off = document->GetElementById("settings-hud-chat-off");
    hudRadarToggle.on = document->GetElementById("settings-hud-radar-on");
    hudRadarToggle.off = document->GetElementById("settings-hud-radar-off");
    hudFpsToggle.on = document->GetElementById("settings-hud-fps-on");
    hudFpsToggle.off = document->GetElementById("settings-hud-fps-off");
    hudCrosshairToggle.on = document->GetElementById("settings-hud-crosshair-on");
    hudCrosshairToggle.off = document->GetElementById("settings-hud-crosshair-off");
    vsyncToggle.on = document->GetElementById("settings-vsync-on");
    vsyncToggle.off = document->GetElementById("settings-vsync-off");

    listeners.clear();
    if (brightnessSlider) {
        auto listener = std::make_unique<BrightnessListener>(this);
        brightnessSlider->AddEventListener("change", listener.get());
        brightnessSlider->AddEventListener("input", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (hudBackgroundEditButton) {
        auto listener = std::make_unique<HudBackgroundToggleListener>(this);
        hudBackgroundEditButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (hudBackgroundRSlider || hudBackgroundGSlider || hudBackgroundBSlider || hudBackgroundASlider) {
        auto listener = std::make_unique<HudBackgroundListener>(this);
        if (hudBackgroundRSlider) {
            hudBackgroundRSlider->AddEventListener("change", listener.get());
            hudBackgroundRSlider->AddEventListener("input", listener.get());
        }
        if (hudBackgroundGSlider) {
            hudBackgroundGSlider->AddEventListener("change", listener.get());
            hudBackgroundGSlider->AddEventListener("input", listener.get());
        }
        if (hudBackgroundBSlider) {
            hudBackgroundBSlider->AddEventListener("change", listener.get());
            hudBackgroundBSlider->AddEventListener("input", listener.get());
        }
        if (hudBackgroundASlider) {
            hudBackgroundASlider->AddEventListener("change", listener.get());
            hudBackgroundASlider->AddEventListener("input", listener.get());
        }
        listeners.emplace_back(std::move(listener));
    }
    if (hudTextEditButton) {
        auto listener = std::make_unique<HudTextToggleListener>(this);
        hudTextEditButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (hudTextRSlider || hudTextGSlider || hudTextBSlider) {
        auto listener = std::make_unique<HudTextListener>(this);
        if (hudTextRSlider) {
            hudTextRSlider->AddEventListener("change", listener.get());
            hudTextRSlider->AddEventListener("input", listener.get());
        }
        if (hudTextGSlider) {
            hudTextGSlider->AddEventListener("change", listener.get());
            hudTextGSlider->AddEventListener("input", listener.get());
        }
        if (hudTextBSlider) {
            hudTextBSlider->AddEventListener("change", listener.get());
            hudTextBSlider->AddEventListener("input", listener.get());
        }
        listeners.emplace_back(std::move(listener));
    }
    if (hudTextSizeSlider) {
        auto listener = std::make_unique<HudTextSizeListener>(this);
        hudTextSizeSlider->AddEventListener("change", listener.get());
        hudTextSizeSlider->AddEventListener("input", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (languageSelect) {
        auto listener = std::make_unique<LanguageListener>(this);
        languageSelect->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
        rebuildLanguageOptions();
    }
    if (hudScoreboardToggle.on || hudScoreboardToggle.off ||
        hudChatToggle.on || hudChatToggle.off ||
        hudRadarToggle.on || hudRadarToggle.off ||
        hudFpsToggle.on || hudFpsToggle.off ||
        hudCrosshairToggle.on || hudCrosshairToggle.off) {
        auto listener = std::make_unique<HudToggleListener>(this);
        if (hudScoreboardToggle.on) {
            hudScoreboardToggle.on->AddEventListener("click", listener.get());
        }
        if (hudScoreboardToggle.off) {
            hudScoreboardToggle.off->AddEventListener("click", listener.get());
        }
        if (hudChatToggle.on) {
            hudChatToggle.on->AddEventListener("click", listener.get());
        }
        if (hudChatToggle.off) {
            hudChatToggle.off->AddEventListener("click", listener.get());
        }
        if (hudRadarToggle.on) {
            hudRadarToggle.on->AddEventListener("click", listener.get());
        }
        if (hudRadarToggle.off) {
            hudRadarToggle.off->AddEventListener("click", listener.get());
        }
        if (hudFpsToggle.on) {
            hudFpsToggle.on->AddEventListener("click", listener.get());
        }
        if (hudFpsToggle.off) {
            hudFpsToggle.off->AddEventListener("click", listener.get());
        }
        if (hudCrosshairToggle.on) {
            hudCrosshairToggle.on->AddEventListener("click", listener.get());
        }
        if (hudCrosshairToggle.off) {
            hudCrosshairToggle.off->AddEventListener("click", listener.get());
        }
        listeners.emplace_back(std::move(listener));
    }
    if (vsyncToggle.on || vsyncToggle.off) {
        auto listener = std::make_unique<RenderToggleListener>(this);
        if (vsyncToggle.on) {
            vsyncToggle.on->AddEventListener("click", listener.get());
        }
        if (vsyncToggle.off) {
            vsyncToggle.off->AddEventListener("click", listener.get());
        }
        listeners.emplace_back(std::move(listener));
    }
    syncSettingsFromConfig();
    syncRenderBrightnessControls(true);
    syncHudBackgroundControls(true);
    syncHudTextControls(true);
    syncHudTextSizeControls(true);
    syncRenderControls();
    syncHudControls();
    updateStatus();
}

void RmlUiPanelSettings::onUpdate() {
    if (!document) {
        return;
    }
    if (!settingsModel.loaded) {
        settingsModel.loaded = true;
        syncSettingsFromConfig();
        syncRenderBrightnessControls(true);
        syncHudBackgroundControls(true);
        syncHudTextControls(true);
        syncHudTextSizeControls(true);
        syncRenderControls();
        syncHudControls();
        updateStatus();
    }
    if (settingsModel.hud.consumeDirty()) {
        std::string error;
        if (!settingsController.saveHudSettings(&error)) {
            showStatus(error, true);
        }
    }

    refreshDebouncedCommits();
}

void RmlUiPanelSettings::refreshDebouncedCommits() {
    const auto debounce = std::chrono::milliseconds(150);

    brightnessCommit.tryCommit(debounce, [&]() {
        setRenderBrightnessDragging(false);
        float value = getRenderBrightness();
        if (brightnessSlider) {
            if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(brightnessSlider)) {
                try {
                    value = std::stof(input->GetValue());
                } catch (...) {
                }
            }
        }
        applyRenderBrightness(value, true);
        commitRenderBrightness();
    });

    hudBackgroundCommit.tryCommit(debounce, [&]() {
        hudBackgroundDragging = false;
        handleHudBackgroundInput(true);
    });

    hudTextCommit.tryCommit(debounce, [&]() {
        hudTextDragging = false;
        handleHudTextInput(true);
    });

    hudTextSizeCommit.tryCommit(debounce, [&]() {
        hudTextSizeDragging = false;
        handleHudTextSizeInput(true);
    });
}

void RmlUiPanelSettings::onShow() {
    settingsModel.loaded = false;
}

void RmlUiPanelSettings::onConfigChanged() {
    settingsModel.loaded = false;
}

void RmlUiPanelSettings::rebuildLanguageOptions() {
    if (!languageSelect) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(languageSelect);
    if (!select) {
        return;
    }
    suppressLanguageSelection = true;
    select->RemoveAll();
    for (const auto &code : kLanguageCodes) {
        const std::string labelKey = "languages." + code;
        const std::string &label = karma::common::i18n::Get().get(labelKey);
        select->Add(label.empty() ? code : label, code);
    }
    const std::string selected = selectedLanguageFromConfig();
    for (std::size_t i = 0; i < kLanguageCodes.size(); ++i) {
        if (kLanguageCodes[i] == selected) {
            select->SetSelection(static_cast<int>(i));
            break;
        }
    }
    suppressLanguageSelection = false;
}

void RmlUiPanelSettings::applyLanguageSelection(const std::string &code) {
    if (code.empty()) {
        return;
    }
    if (code == selectedLanguageFromConfig() && code == karma::common::i18n::Get().language()) {
        return;
    }
    std::string error;
    if (!settingsController.setLanguage(code, &error)) {
        showStatus(error, true);
        return;
    }
    if (languageCallback) {
        languageCallback(code);
    }
}

std::string RmlUiPanelSettings::selectedLanguageFromConfig() const {
    const std::string configured = settingsController.getConfiguredLanguage();
    if (!configured.empty()) {
        return configured;
    }
    return karma::common::i18n::Get().language();
}

float RmlUiPanelSettings::getRenderBrightness() const {
    return settingsModel.render.brightness();
}

bool RmlUiPanelSettings::isRenderBrightnessDragActive() const {
    return renderBrightnessDragging;
}

void RmlUiPanelSettings::clearRenderBrightnessDrag() {
    renderBrightnessDragging = false;
}

void RmlUiPanelSettings::applyRenderBrightness(float value, bool fromUser) {
    if (!settingsModel.render.setBrightness(value, fromUser)) {
        return;
    }
    if (fromUser) {
        syncRenderBrightnessLabel();
    } else {
        syncRenderBrightnessControls(true);
    }
}

bool RmlUiPanelSettings::commitRenderBrightness() {
    std::string error;
    if (!settingsController.saveRenderSettings(&error)) {
        showStatus(error, true);
        return false;
    }
    settingsModel.render.clearDirty();
    return true;
}

void RmlUiPanelSettings::setRenderBrightnessDragging(bool dragging) {
    renderBrightnessDragging = dragging;
}

void RmlUiPanelSettings::syncRenderBrightnessControls(bool syncSlider) {
    if (syncSlider && brightnessSlider) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(brightnessSlider);
        if (input) {
            input->SetValue(std::to_string(settingsModel.render.brightness()));
        }
    }
    syncRenderBrightnessLabel();
}

void RmlUiPanelSettings::syncRenderBrightnessLabel() {
    if (!brightnessValueLabel) {
        return;
    }
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(2);
    oss << settingsModel.render.brightness() << "x";
    brightnessValueLabel->SetInnerRML(oss.str());
}

void RmlUiPanelSettings::syncHudBackgroundControls(bool syncSliders) {
    if (!syncSliders) {
        syncHudBackgroundSwatch();
        syncHudBackgroundValues();
        return;
    }
    const auto color = settingsModel.hud.backgroundColor();
    auto setSlider = [](Rml::Element *element, float value) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element);
        if (input) {
            input->SetValue(std::to_string(value));
        }
    };
    setSlider(hudBackgroundRSlider, color[0]);
    setSlider(hudBackgroundGSlider, color[1]);
    setSlider(hudBackgroundBSlider, color[2]);
    setSlider(hudBackgroundASlider, color[3]);
    syncHudBackgroundSwatch();
    syncHudBackgroundValues();
}

void RmlUiPanelSettings::syncHudBackgroundSwatch() {
    if (!hudBackgroundSwatch) {
        return;
    }
    const auto color = settingsModel.hud.backgroundColor();
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
    hudBackgroundSwatch->SetProperty("background-color", buffer);
}

void RmlUiPanelSettings::syncHudBackgroundValues() {
    const auto color = settingsModel.hud.backgroundColor();
    auto setValue = [](Rml::Element *element, float value) {
        if (!element) {
            return;
        }
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << value;
        element->SetInnerRML(oss.str());
    };
    setValue(hudBackgroundRValue, color[0]);
    setValue(hudBackgroundGValue, color[1]);
    setValue(hudBackgroundBValue, color[2]);
    setValue(hudBackgroundAValue, color[3]);
}

void RmlUiPanelSettings::syncHudTextControls(bool syncSliders) {
    if (!syncSliders) {
        syncHudTextSwatch();
        syncHudTextValues();
        return;
    }
    auto color = settingsModel.hud.textColor();
    color[3] = 1.0f;
    auto setSlider = [](Rml::Element *element, float value) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element);
        if (input) {
            input->SetValue(std::to_string(value));
        }
    };
    setSlider(hudTextRSlider, color[0]);
    setSlider(hudTextGSlider, color[1]);
    setSlider(hudTextBSlider, color[2]);
    syncHudTextSwatch();
    syncHudTextValues();
}

void RmlUiPanelSettings::syncHudTextSwatch() {
    if (!hudTextSwatch) {
        return;
    }
    const auto color = settingsModel.hud.textColor();
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
    hudTextSwatch->SetProperty("background-color", buffer);
}

void RmlUiPanelSettings::syncHudTextValues() {
    const auto color = settingsModel.hud.textColor();
    auto setValue = [](Rml::Element *element, float value) {
        if (!element) {
            return;
        }
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << value;
        element->SetInnerRML(oss.str());
    };
    setValue(hudTextRValue, color[0]);
    setValue(hudTextGValue, color[1]);
    setValue(hudTextBValue, color[2]);
}

void RmlUiPanelSettings::handleHudTextInput(bool commit) {
    auto getSliderValue = [](Rml::Element *element, float fallback) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element);
        if (!input) {
            return fallback;
        }
        try {
            return std::stof(input->GetValue());
        } catch (...) {
            return fallback;
        }
    };
    const auto current = settingsModel.hud.textColor();
    const float r = getSliderValue(hudTextRSlider, current[0]);
    const float g = getSliderValue(hudTextGSlider, current[1]);
    const float b = getSliderValue(hudTextBSlider, current[2]);
    settingsModel.hud.setTextColor({r, g, b, 1.0f}, commit);
    syncHudTextSwatch();
    syncHudTextValues();
    if (commit) {
        std::string error;
        if (!settingsController.saveHudSettings(&error)) {
            showStatus(error, true);
        }
        settingsModel.hud.clearDirty();
    }
}

void RmlUiPanelSettings::handleHudTextToggle() {
    if (!hudTextPopup) {
        return;
    }
    const bool hidden = hudTextPopup->IsClassSet("hidden");
    hudTextPopup->SetClass("hidden", !hidden);
}

void RmlUiPanelSettings::syncHudTextSizeControls(bool syncSlider) {
    if (syncSlider && hudTextSizeSlider) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(hudTextSizeSlider);
        if (input) {
            input->SetValue(std::to_string(settingsModel.hud.textScale()));
        }
    }
    syncHudTextSizeLabel();
}

void RmlUiPanelSettings::syncHudTextSizeLabel() {
    if (!hudTextSizeValue) {
        return;
    }
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(2);
    oss << settingsModel.hud.textScale() << "x";
    hudTextSizeValue->SetInnerRML(oss.str());
}

void RmlUiPanelSettings::handleHudTextSizeInput(bool commit) {
    float value = settingsModel.hud.textScale();
    if (hudTextSizeSlider) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(hudTextSizeSlider)) {
            try {
                value = std::stof(input->GetValue());
            } catch (...) {
            }
        }
    }
    settingsModel.hud.setTextScale(value, commit);
    syncHudTextSizeLabel();
    if (commit) {
        std::string error;
        if (!settingsController.saveHudSettings(&error)) {
            showStatus(error, true);
        }
        settingsModel.hud.clearDirty();
    }
}

void RmlUiPanelSettings::handleHudBackgroundInput(bool commit) {
    auto getSliderValue = [](Rml::Element *element, float fallback) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element);
        if (!input) {
            return fallback;
        }
        try {
            return std::stof(input->GetValue());
        } catch (...) {
            return fallback;
        }
    };
    const auto current = settingsModel.hud.backgroundColor();
    const float r = getSliderValue(hudBackgroundRSlider, current[0]);
    const float g = getSliderValue(hudBackgroundGSlider, current[1]);
    const float b = getSliderValue(hudBackgroundBSlider, current[2]);
    const float a = getSliderValue(hudBackgroundASlider, current[3]);
    settingsModel.hud.setBackgroundColor({r, g, b, a}, commit);
    syncHudBackgroundSwatch();
    syncHudBackgroundValues();
    if (commit) {
        std::string error;
        if (!settingsController.saveHudSettings(&error)) {
            showStatus(error, true);
        }
        settingsModel.hud.clearDirty();
    }
}

void RmlUiPanelSettings::handleHudBackgroundToggle() {
    if (!hudBackgroundPopup) {
        return;
    }
    const bool hidden = hudBackgroundPopup->IsClassSet("hidden");
    hudBackgroundPopup->SetClass("hidden", !hidden);
}

void RmlUiPanelSettings::syncHudControls() {
    auto applyToggle = [](const HudToggleButtons &toggle, bool value) {
        if (toggle.on) {
            toggle.on->SetClass("active", value);
        }
        if (toggle.off) {
            toggle.off->SetClass("active", !value);
        }
    };
    applyToggle(hudScoreboardToggle, settingsModel.hud.scoreboardVisible());
    applyToggle(hudChatToggle, settingsModel.hud.chatVisible());
    applyToggle(hudRadarToggle, settingsModel.hud.radarVisible());
    applyToggle(hudFpsToggle, settingsModel.hud.fpsVisible());
    applyToggle(hudCrosshairToggle, settingsModel.hud.crosshairVisible());
    syncHudBackgroundControls(false);
    syncHudTextControls(false);
    syncHudTextSizeControls(false);
}

void RmlUiPanelSettings::syncRenderControls() {
    auto applyToggle = [](const HudToggleButtons &toggle, bool value) {
        if (toggle.on) {
            toggle.on->SetClass("active", value);
        }
        if (toggle.off) {
            toggle.off->SetClass("active", !value);
        }
    };
    applyToggle(vsyncToggle, settingsModel.render.vsync());
}

void RmlUiPanelSettings::handleHudToggle(Rml::Element *target) {
    if (!target) {
        return;
    }
    if (target == hudScoreboardToggle.on) {
        settingsModel.hud.setScoreboardVisible(true, true);
    } else if (target == hudScoreboardToggle.off) {
        settingsModel.hud.setScoreboardVisible(false, true);
    } else if (target == hudChatToggle.on) {
        settingsModel.hud.setChatVisible(true, true);
    } else if (target == hudChatToggle.off) {
        settingsModel.hud.setChatVisible(false, true);
    } else if (target == hudRadarToggle.on) {
        settingsModel.hud.setRadarVisible(true, true);
    } else if (target == hudRadarToggle.off) {
        settingsModel.hud.setRadarVisible(false, true);
    } else if (target == hudFpsToggle.on) {
        settingsModel.hud.setFpsVisible(true, true);
    } else if (target == hudFpsToggle.off) {
        settingsModel.hud.setFpsVisible(false, true);
    } else if (target == hudCrosshairToggle.on) {
        settingsModel.hud.setCrosshairVisible(true, true);
    } else if (target == hudCrosshairToggle.off) {
        settingsModel.hud.setCrosshairVisible(false, true);
    } else {
        return;
    }
    syncHudControls();
}

void RmlUiPanelSettings::handleRenderToggle(Rml::Element *target) {
    if (!target) {
        return;
    }
    if (target == vsyncToggle.on) {
        settingsModel.render.setVsync(true, true);
    } else if (target == vsyncToggle.off) {
        settingsModel.render.setVsync(false, true);
    } else {
        return;
    }
    syncRenderControls();
    std::string error;
    if (!settingsController.saveRenderSettings(&error)) {
        showStatus(error, true);
    }
}

void RmlUiPanelSettings::showStatus(const std::string &message, bool isError) {
    settingsModel.statusText = message;
    settingsModel.statusIsError = isError;
    updateStatus();
}

void RmlUiPanelSettings::updateStatus() {
    if (!statusLabel) {
        return;
    }
    const auto banner = ui::status_banner::MakeStatusBanner(settingsModel.statusText,
                                                            settingsModel.statusIsError);
    if (!banner.visible) {
        statusLabel->SetClass("hidden", true);
        return;
    }
    statusLabel->SetClass("hidden", false);
    statusLabel->SetClass("status-error", banner.tone == ui::MessageTone::Error);
    statusLabel->SetClass("status-pending", banner.tone == ui::MessageTone::Pending);
    const std::string text = ui::status_banner::FormatStatusText(banner);
    statusLabel->SetInnerRML(escapeRmlText(text));
}

void RmlUiPanelSettings::syncSettingsFromConfig() {
    if (!karma::common::config::ConfigStore::Initialized()) {
        return;
    }

    settingsModel.render.loadFromConfig();
    settingsModel.hud.loadFromConfig();
}

} // namespace ui

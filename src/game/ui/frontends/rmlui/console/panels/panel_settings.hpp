#pragma once

#include <optional>
#include <string>
#include <functional>
#include <chrono>
#include <vector>

#include "ui/models/settings_model.hpp"
#include "ui/controllers/settings_controller.hpp"

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "ui/frontends/rmlui/console/panels/panel.hpp"

#include <memory>

namespace ui {

class RmlUiPanelSettings final : public RmlUiPanel {
public:
    RmlUiPanelSettings();
    void setUserConfigPath(const std::string &path);
    bool consumeKeybindingsReloadRequest();
    float getRenderBrightness() const;
    bool isRenderBrightnessDragActive() const;
    void clearRenderBrightnessDrag();
    void setLanguageCallback(std::function<void(const std::string &)> callback);

protected:
    void onLoaded(Rml::ElementDocument *document) override;
    void onUpdate() override;
    void onShow() override;
    void onConfigChanged() override;

private:
    class DebouncedCommit {
    public:
        void markChanged();
        template <typename Fn>
        void tryCommit(std::chrono::milliseconds delay, Fn &&commitFn) {
            if (!pending) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - lastChange < delay) {
                return;
            }
            pending = false;
            commitFn();
        }

    private:
        bool pending = false;
        std::chrono::steady_clock::time_point lastChange{};
    };
    class BrightnessListener;
    class HudBackgroundListener;
    class HudBackgroundToggleListener;
    class HudTextListener;
    class HudTextToggleListener;
    class HudTextSizeListener;
    class LanguageListener;
    class HudToggleListener;
    class RenderToggleListener;

    void updateStatus();
    void refreshDebouncedCommits();
    void applyRenderBrightness(float value, bool fromUser);
    bool commitRenderBrightness();
    void syncRenderBrightnessControls(bool syncSlider);
    void syncRenderBrightnessLabel();
    void syncHudControls();
    void syncHudBackgroundControls(bool syncSliders);
    void syncHudBackgroundSwatch();
    void syncHudBackgroundValues();
    void handleHudBackgroundInput(bool commit);
    void handleHudBackgroundToggle();
    void syncHudTextControls(bool syncSliders);
    void syncHudTextSwatch();
    void syncHudTextValues();
    void handleHudTextInput(bool commit);
    void handleHudTextToggle();
    void syncHudTextSizeControls(bool syncSlider);
    void syncHudTextSizeLabel();
    void handleHudTextSizeInput(bool commit);
    void handleHudToggle(Rml::Element *target);
    void syncRenderControls();
    void handleRenderToggle(Rml::Element *target);
    void showStatus(const std::string &message, bool isError);
    void rebuildLanguageOptions();
    void applyLanguageSelection(const std::string &code);
    std::string selectedLanguageFromConfig() const;
    bool isLanguageSelectionSuppressed() const { return suppressLanguageSelection; }
    void syncSettingsFromConfig();
    void setRenderBrightnessDragging(bool dragging);

    Rml::ElementDocument *document = nullptr;
    Rml::Element *statusLabel = nullptr;
    Rml::Element *languageSelect = nullptr;
    ui::SettingsModel settingsModel;
    ui::SettingsController settingsController{settingsModel};
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;

    Rml::Element *brightnessSlider = nullptr;
    Rml::Element *brightnessValueLabel = nullptr;
    Rml::Element *hudBackgroundSwatch = nullptr;
    Rml::Element *hudBackgroundPopup = nullptr;
    Rml::Element *hudBackgroundEditButton = nullptr;
    Rml::Element *hudBackgroundRSlider = nullptr;
    Rml::Element *hudBackgroundGSlider = nullptr;
    Rml::Element *hudBackgroundBSlider = nullptr;
    Rml::Element *hudBackgroundASlider = nullptr;
    Rml::Element *hudBackgroundRValue = nullptr;
    Rml::Element *hudBackgroundGValue = nullptr;
    Rml::Element *hudBackgroundBValue = nullptr;
    Rml::Element *hudBackgroundAValue = nullptr;
    Rml::Element *hudTextSwatch = nullptr;
    Rml::Element *hudTextPopup = nullptr;
    Rml::Element *hudTextEditButton = nullptr;
    Rml::Element *hudTextRSlider = nullptr;
    Rml::Element *hudTextGSlider = nullptr;
    Rml::Element *hudTextBSlider = nullptr;
    Rml::Element *hudTextRValue = nullptr;
    Rml::Element *hudTextGValue = nullptr;
    Rml::Element *hudTextBValue = nullptr;
    Rml::Element *hudTextSizeSlider = nullptr;
    Rml::Element *hudTextSizeValue = nullptr;
    bool renderBrightnessDragging = false;
    bool hudBackgroundDragging = false;
    bool hudTextDragging = false;
    bool hudTextSizeDragging = false;
    DebouncedCommit brightnessCommit{};
    DebouncedCommit hudBackgroundCommit{};
    DebouncedCommit hudTextCommit{};
    DebouncedCommit hudTextSizeCommit{};
    struct HudToggleButtons {
        Rml::Element *on = nullptr;
        Rml::Element *off = nullptr;
    };

    HudToggleButtons hudScoreboardToggle{};
    HudToggleButtons hudChatToggle{};
    HudToggleButtons hudRadarToggle{};
    HudToggleButtons hudFpsToggle{};
    HudToggleButtons hudCrosshairToggle{};
    HudToggleButtons vsyncToggle{};
    std::function<void(const std::string &)> languageCallback;
    bool suppressLanguageSelection = false;
};

} // namespace ui

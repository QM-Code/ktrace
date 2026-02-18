#include "ui/controllers/settings_controller.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/common/config/store.hpp"
#include "ui/config/ui_config.hpp"

namespace ui {

SettingsController::SettingsController(SettingsModel &modelIn)
    : model(modelIn) {}

std::string SettingsController::getConfiguredLanguage() const {
    return ui::UiConfig::GetLanguage();
}

bool SettingsController::setLanguage(const std::string &code, std::string *error) {
    const std::string previousLanguage =
        model.language.empty() ? ui::UiConfig::GetLanguage() : model.language;
    if (!ui::UiConfig::SetLanguage(code)) {
        if (error) {
            *error = "Failed to save language.";
        }
        return false;
    }
    model.language = code;
    KARMA_TRACE("ui.rmlui", "UiSettings: language changed {} -> {}", previousLanguage, code);
    return true;
}

bool SettingsController::saveHudSettings(std::string *error) {
    if (!model.hud.saveToConfig()) {
        if (error) {
            *error = "Failed to save HUD settings.";
        }
        return false;
    }
    return true;
}

bool SettingsController::saveRenderSettings(std::string *error) {
    if (!model.render.saveToConfig()) {
        if (error) {
            *error = "Failed to save render settings.";
        }
        return false;
    }
    return true;
}

} // namespace ui

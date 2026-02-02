#include "ui/config/hud_settings.hpp"

#include "ui/config/ui_config.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

void HudSettings::loadFromConfig() {
    reset();
    scoreboardVisibleValue = UiConfig::GetHudScoreboard();
    chatVisibleValue = UiConfig::GetHudChat();
    radarVisibleValue = UiConfig::GetHudRadar();
    fpsVisibleValue = UiConfig::GetHudFps();
    crosshairVisibleValue = UiConfig::GetHudCrosshair();
    backgroundColorValue = UiConfig::GetHudBackgroundColor();
    textColorValue = UiConfig::GetHudTextColor();
    textScaleValue = UiConfig::GetHudTextScale();
}

bool HudSettings::saveToConfig() const {
    return UiConfig::SetHudScoreboard(scoreboardVisibleValue) &&
        UiConfig::SetHudChat(chatVisibleValue) &&
        UiConfig::SetHudRadar(radarVisibleValue) &&
        UiConfig::SetHudFps(fpsVisibleValue) &&
        UiConfig::SetHudCrosshair(crosshairVisibleValue) &&
        UiConfig::SetHudBackgroundColor(backgroundColorValue) &&
        UiConfig::SetHudTextColor(textColorValue) &&
        UiConfig::SetHudTextScale(textScaleValue);
}

void HudSettings::reset() {
    scoreboardVisibleValue = UiConfig::GetHudScoreboard();
    chatVisibleValue = UiConfig::GetHudChat();
    radarVisibleValue = UiConfig::GetHudRadar();
    fpsVisibleValue = UiConfig::GetHudFps();
    crosshairVisibleValue = UiConfig::GetHudCrosshair();
    backgroundColorValue = UiConfig::GetHudBackgroundColor();
    textColorValue = UiConfig::GetHudTextColor();
    textScaleValue = UiConfig::GetHudTextScale();
    dirty = false;
}

bool HudSettings::setScoreboardVisible(bool value, bool fromUser) {
    if (value == scoreboardVisibleValue) {
        return false;
    }
    scoreboardVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setChatVisible(bool value, bool fromUser) {
    if (value == chatVisibleValue) {
        return false;
    }
    chatVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setRadarVisible(bool value, bool fromUser) {
    if (value == radarVisibleValue) {
        return false;
    }
    radarVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setFpsVisible(bool value, bool fromUser) {
    if (value == fpsVisibleValue) {
        return false;
    }
    fpsVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setCrosshairVisible(bool value, bool fromUser) {
    if (value == crosshairVisibleValue) {
        return false;
    }
    crosshairVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setBackgroundColor(const std::array<float, 4> &value, bool fromUser) {
    std::array<float, 4> clamped = value;
    for (float &component : clamped) {
        component = std::clamp(component, 0.0f, 1.0f);
    }
    if (clamped == backgroundColorValue) {
        return false;
    }
    backgroundColorValue = clamped;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setTextColor(const std::array<float, 4> &value, bool fromUser) {
    std::array<float, 4> clamped = value;
    for (float &component : clamped) {
        component = std::clamp(component, 0.0f, 1.0f);
    }
    clamped[3] = 1.0f;
    if (clamped == textColorValue) {
        return false;
    }
    textColorValue = clamped;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setTextScale(float value, bool fromUser) {
    if (std::abs(value - textScaleValue) < 0.0001f) {
        return false;
    }
    textScaleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::consumeDirty() {
    const bool wasDirty = dirty;
    dirty = false;
    return wasDirty;
}

void HudSettings::clearDirty() {
    dirty = false;
}

} // namespace ui

#include "ui/config/render_settings.hpp"

#include "ui/config/ui_config.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

float RenderSettings::clampBrightness(float value) {
    return std::clamp(value, kMinBrightness, kMaxBrightness);
}

void RenderSettings::loadFromConfig() {
    reset();
    setBrightness(UiConfig::GetRenderBrightness(), false);
    setVsync(UiConfig::GetVsync(), false);
}

bool RenderSettings::saveToConfig() const {
    return UiConfig::SetRenderBrightness(brightnessValue) &&
           UiConfig::SetVsync(vsyncValue);
}

void RenderSettings::reset() {
    brightnessValue = UiConfig::GetRenderBrightness();
    vsyncValue = UiConfig::GetVsync();
    dirty = false;
}

bool RenderSettings::setBrightness(float value, bool fromUser) {
    const float clamped = clampBrightness(value);
    if (std::abs(clamped - brightnessValue) < 0.0001f) {
        return false;
    }
    brightnessValue = clamped;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool RenderSettings::setVsync(bool value, bool fromUser) {
    if (value == vsyncValue) {
        return false;
    }
    vsyncValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

float RenderSettings::brightness() const {
    return brightnessValue;
}

bool RenderSettings::vsync() const {
    return vsyncValue;
}

bool RenderSettings::consumeDirty() {
    const bool wasDirty = dirty;
    dirty = false;
    return wasDirty;
}

void RenderSettings::clearDirty() {
    dirty = false;
}

bool RenderSettings::eraseFromConfig() {
    return UiConfig::EraseRenderBrightness();
}

} // namespace ui

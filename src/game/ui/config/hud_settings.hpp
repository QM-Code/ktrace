#pragma once

#include <array>

namespace ui {

class HudSettings {
public:
    void loadFromConfig();
    bool saveToConfig() const;
    void reset();

    bool scoreboardVisible() const { return scoreboardVisibleValue; }
    bool chatVisible() const { return chatVisibleValue; }
    bool radarVisible() const { return radarVisibleValue; }
    bool fpsVisible() const { return fpsVisibleValue; }
    bool crosshairVisible() const { return crosshairVisibleValue; }
    std::array<float, 4> backgroundColor() const { return backgroundColorValue; }
    std::array<float, 4> textColor() const { return textColorValue; }
    float textScale() const { return textScaleValue; }

    bool setScoreboardVisible(bool value, bool fromUser);
    bool setChatVisible(bool value, bool fromUser);
    bool setRadarVisible(bool value, bool fromUser);
    bool setFpsVisible(bool value, bool fromUser);
    bool setCrosshairVisible(bool value, bool fromUser);
    bool setBackgroundColor(const std::array<float, 4> &value, bool fromUser);
    bool setTextColor(const std::array<float, 4> &value, bool fromUser);
    bool setTextScale(float value, bool fromUser);

    bool consumeDirty();
    void clearDirty();

private:
    bool scoreboardVisibleValue = true;
    bool chatVisibleValue = true;
    bool radarVisibleValue = true;
    bool fpsVisibleValue = false;
    bool crosshairVisibleValue = true;
    std::array<float, 4> backgroundColorValue{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> textColorValue{1.0f, 1.0f, 1.0f, 1.0f};
    float textScaleValue = 1.0f;
    bool dirty = false;
};

} // namespace ui

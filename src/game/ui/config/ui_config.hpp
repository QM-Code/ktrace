#pragma once

#include <optional>
#include <array>
#include <string>

#include "karma/common/json.hpp"

namespace ui {

class UiConfig {
public:
    static constexpr float kMinRenderScale = 0.5f;
    static constexpr float kMaxRenderScale = 1.0f;

    static float GetRenderBrightness();
    static bool SetRenderBrightness(float value);
    static bool EraseRenderBrightness();
    static bool GetVsync();
    static bool SetVsync(bool value);

    static float GetRenderScale();
    static bool SetRenderScale(float value);
    static bool EraseRenderScale();

    static std::string GetLanguage();
    static bool SetLanguage(const std::string &value);

    static const karma::json::Value *GetCommunityCredentials();
    static bool SetCommunityCredentials(const karma::json::Value &value);
    static bool EraseCommunityCredentials();

    static std::optional<karma::json::Value> GetKeybindings();
    static bool SetKeybindings(const karma::json::Value &value);
    static bool EraseKeybindings();

    static std::optional<karma::json::Value> GetControllerKeybindings();
    static bool SetControllerKeybindings(const karma::json::Value &value);
    static bool EraseControllerKeybindings();

    static bool GetHudScoreboard();
    static bool GetHudChat();
    static bool GetHudRadar();
    static bool GetHudFps();
    static bool GetHudCrosshair();
    static std::array<float, 4> GetHudBackgroundColor();
    static std::array<float, 4> GetHudTextColor();
    static float GetHudTextScale();
    static bool GetValidateUi();

    static bool SetHudScoreboard(bool value);
    static bool SetHudChat(bool value);
    static bool SetHudRadar(bool value);
    static bool SetHudFps(bool value);
    static bool SetHudCrosshair(bool value);
    static bool SetHudBackgroundColor(const std::array<float, 4> &value);
    static bool SetHudTextColor(const std::array<float, 4> &value);
    static bool SetHudTextScale(float value);
};

} // namespace ui

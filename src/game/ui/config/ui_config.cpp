#include "ui/config/ui_config.hpp"

#include "karma/common/config/store.hpp"
#include "karma/common/serialization/json.hpp"
#include "ui/config/config.hpp"

#include <string>

namespace ui {

float UiConfig::GetRenderBrightness() {
    return ui::config::GetRequiredFloat("render.brightness");
}

bool UiConfig::SetRenderBrightness(float value) {
    return karma::common::config::ConfigStore::Set("render.brightness", value);
}

bool UiConfig::EraseRenderBrightness() {
    return karma::common::config::ConfigStore::Erase("render.brightness");
}

bool UiConfig::GetVsync() {
    return ui::config::GetRequiredBool("graphics.VSync");
}

bool UiConfig::SetVsync(bool value) {
    return karma::common::config::ConfigStore::Set("graphics.VSync", value);
}

float UiConfig::GetRenderScale() {
    return ui::config::GetRequiredFloat("ui.RenderScale");
}

bool UiConfig::SetRenderScale(float value) {
    return karma::common::config::ConfigStore::Set("ui.RenderScale", value);
}

bool UiConfig::EraseRenderScale() {
    return karma::common::config::ConfigStore::Erase("ui.RenderScale");
}

std::string UiConfig::GetLanguage() {
    if (const auto *value = karma::common::config::ConfigStore::Get("language")) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    return {};
}

bool UiConfig::SetLanguage(const std::string &value) {
    return karma::common::config::ConfigStore::Set("language", value);
}

const karma::common::serialization::Value *UiConfig::GetCommunityCredentials() {
    return karma::common::config::ConfigStore::Get("gui.communityCredentials");
}

bool UiConfig::SetCommunityCredentials(const karma::common::serialization::Value &value) {
    return karma::common::config::ConfigStore::Set("gui.communityCredentials", value);
}

bool UiConfig::EraseCommunityCredentials() {
    return karma::common::config::ConfigStore::Erase("gui.communityCredentials");
}

std::optional<karma::common::serialization::Value> UiConfig::GetKeybindings() {
    return karma::common::config::ConfigStore::GetCopy("keybindings");
}

bool UiConfig::SetKeybindings(const karma::common::serialization::Value &value) {
    return karma::common::config::ConfigStore::Set("keybindings", value);
}

bool UiConfig::EraseKeybindings() {
    return karma::common::config::ConfigStore::Erase("keybindings");
}

std::optional<karma::common::serialization::Value> UiConfig::GetControllerKeybindings() {
    return karma::common::config::ConfigStore::GetCopy("gui.keybindings.controller");
}

bool UiConfig::SetControllerKeybindings(const karma::common::serialization::Value &value) {
    return karma::common::config::ConfigStore::Set("gui.keybindings.controller", value);
}

bool UiConfig::EraseControllerKeybindings() {
    return karma::common::config::ConfigStore::Erase("gui.keybindings.controller");
}

bool UiConfig::GetHudScoreboard() {
    return ui::config::GetRequiredBool("ui.hud.scoreboard");
}

bool UiConfig::GetHudChat() {
    return ui::config::GetRequiredBool("ui.hud.chat");
}

bool UiConfig::GetHudRadar() {
    return ui::config::GetRequiredBool("ui.hud.radar");
}

bool UiConfig::GetHudFps() {
    return ui::config::GetRequiredBool("ui.hud.fps");
}

bool UiConfig::GetHudCrosshair() {
    return ui::config::GetRequiredBool("ui.hud.crosshair");
}

std::array<float, 4> UiConfig::GetHudBackgroundColor() {
    return ui::config::GetRequiredColor("ui.hud.backgroundColor");
}

std::array<float, 4> UiConfig::GetHudTextColor() {
    return ui::config::GetRequiredColor("ui.hud.textColor");
}

float UiConfig::GetHudTextScale() {
    return ui::config::GetRequiredFloat("ui.hud.textScale");
}

bool UiConfig::GetValidateUi() {
    return ui::config::GetRequiredBool("ui.Validate");
}

bool UiConfig::SetHudScoreboard(bool value) {
    return karma::common::config::ConfigStore::Set("ui.hud.scoreboard", value);
}

bool UiConfig::SetHudChat(bool value) {
    return karma::common::config::ConfigStore::Set("ui.hud.chat", value);
}

bool UiConfig::SetHudRadar(bool value) {
    return karma::common::config::ConfigStore::Set("ui.hud.radar", value);
}

bool UiConfig::SetHudFps(bool value) {
    return karma::common::config::ConfigStore::Set("ui.hud.fps", value);
}

bool UiConfig::SetHudCrosshair(bool value) {
    return karma::common::config::ConfigStore::Set("ui.hud.crosshair", value);
}

bool UiConfig::SetHudBackgroundColor(const std::array<float, 4> &value) {
    const karma::common::serialization::Value array = karma::common::serialization::Array({value[0], value[1], value[2], value[3]});
    return karma::common::config::ConfigStore::Set("ui.hud.backgroundColor", array);
}

bool UiConfig::SetHudTextColor(const std::array<float, 4> &value) {
    const karma::common::serialization::Value array = karma::common::serialization::Array({value[0], value[1], value[2], value[3]});
    return karma::common::config::ConfigStore::Set("ui.hud.textColor", array);
}

bool UiConfig::SetHudTextScale(float value) {
    return karma::common::config::ConfigStore::Set("ui.hud.textScale", value);
}

} // namespace ui

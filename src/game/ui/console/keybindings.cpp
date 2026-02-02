#include "ui/console/keybindings.hpp"

#include <cctype>
#include <string>

namespace ui::bindings {
namespace {

constexpr BindingDefinition kDefinitions[] = {
    {nullptr, "Gameplay", true},
    {"moveForward", "Move Forward"},
    {"moveBackward", "Move Backward"},
    {"moveLeft", "Move Left"},
    {"moveRight", "Move Right"},
    {"jump", "Jump"},
    {"fire", "Fire"},
    {"spawn", "Spawn"},
    {"chat", "Chat"},
    {"toggleFullscreen", "Toggle Fullscreen"},
    {"quickQuit", "Quick Quit"},
    {nullptr, "Roaming", true},
    {"roamMoveForward", "Roam Camera Forward"},
    {"roamMoveBackward", "Roam Camera Backward"},
    {"roamMoveLeft", "Roam Camera Left"},
    {"roamMoveRight", "Roam Camera Right"},
    {"roamMoveUp", "Roam Camera Up"},
    {"roamMoveDown", "Roam Camera Down"},
    {"roamLook", "Roam Camera Look (Hold)"},
    {"roamMoveFast", "Roam Camera Fast"}
};

} // namespace

std::span<const BindingDefinition> Definitions() {
    return std::span<const BindingDefinition>(kDefinitions);
}

bool IsReservedBindingName(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    std::string normalized;
    normalized.reserve(name.size());
    for (const char ch : name) {
        if (ch == ' ' || ch == '-') {
            normalized.push_back('_');
        } else {
            normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
    }
    return normalized == "ESCAPE" || normalized == "GRAVE_ACCENT";
}

} // namespace ui::bindings

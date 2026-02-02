#include "ui/controllers/bindings_controller.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

#include "karma/common/config_store.hpp"
#include "game/input/bindings.hpp"
#include "ui/console/keybindings.hpp"
#include "ui/config/ui_config.hpp"

namespace ui {
namespace {

bool IsBindingDefinition(const ui::bindings::BindingDefinition &def) {
    return !def.isHeader && def.action && def.action[0] != '\0';
}

const std::vector<std::string> &defaultBindingsForAction(std::string_view action) {
    static const game_input::DefaultBindingsMap kDefaults = game_input::DefaultKeybindings();
    static const std::vector<std::string> kEmpty;
    auto it = kDefaults.find(std::string(action));
    if (it == kDefaults.end()) {
        return kEmpty;
    }
    return it->second;
}

void WriteBuffer(std::array<char, 128> &buffer, const std::string &value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

} // namespace

BindingsController::BindingsController(BindingsModel &modelIn)
    : model(modelIn) {}

BindingsController::Result BindingsController::loadFromConfig() {
    Result result;
    model.loaded = true;
    for (auto &buffer : model.keyboard) {
        buffer[0] = '\0';
    }
    for (auto &buffer : model.mouse) {
        buffer[0] = '\0';
    }
    for (auto &buffer : model.controller) {
        buffer[0] = '\0';
    }

    if (!karma::config::ConfigStore::Initialized()) {
        result.ok = false;
        result.status = "Failed to load config; showing defaults.";
        result.statusIsError = true;
    }

    const karma::json::Value *bindingsNode = nullptr;
    auto keybindingsNode = ui::UiConfig::GetKeybindings();
    if (keybindingsNode && keybindingsNode->is_object()) {
        bindingsNode = &(*keybindingsNode);
    }
    const karma::json::Value *controllerNode = nullptr;
    auto controllerBindingsNode = ui::UiConfig::GetControllerKeybindings();
    if (controllerBindingsNode && controllerBindingsNode->is_object()) {
        controllerNode = &(*controllerBindingsNode);
    }

    const auto defs = ui::bindings::Definitions();
    const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
    for (std::size_t i = 0; i < count; ++i) {
        if (!IsBindingDefinition(defs[i])) {
            model.keyboard[i][0] = '\0';
            model.mouse[i][0] = '\0';
            model.controller[i][0] = '\0';
            continue;
        }
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        std::vector<std::string> controllerEntries;
        const bool allowConfigBindings = std::string_view(defs[i].action) != "escape";

        if (bindingsNode && allowConfigBindings) {
            auto it = bindingsNode->find(defs[i].action);
            if (it != bindingsNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (!entry.is_string()) {
                        continue;
                    }
                    const auto value = entry.get<std::string>();
                    if (ui::bindings::IsReservedBindingName(value)) {
                        continue;
                    }
                    if (ui::bindings::IsMouseBindingName(value)) {
                        mouseEntries.push_back(value);
                    } else {
                        keyboardEntries.push_back(value);
                    }
                }
            }
        }

        if (keyboardEntries.empty() && mouseEntries.empty()) {
            const auto &defaults = defaultBindingsForAction(defs[i].action);
            for (const auto &value : defaults) {
                if (ui::bindings::IsMouseBindingName(value)) {
                    mouseEntries.push_back(value);
                } else {
                    keyboardEntries.push_back(value);
                }
            }
        }

        if (controllerNode) {
            auto it = controllerNode->find(defs[i].action);
            if (it != controllerNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (entry.is_string()) {
                        controllerEntries.push_back(entry.get<std::string>());
                    }
                }
            }
        }

        WriteBuffer(model.keyboard[i], ui::bindings::JoinBindings(keyboardEntries));
        WriteBuffer(model.mouse[i], ui::bindings::JoinBindings(mouseEntries));
        WriteBuffer(model.controller[i], ui::bindings::JoinBindings(controllerEntries));
    }

    return result;
}

BindingsController::Result BindingsController::saveToConfig() {
    Result result;
    karma::json::Value keybindings = karma::json::Object();
    karma::json::Value controllerBindings = karma::json::Object();
    bool hasBindings = false;
    bool hasControllerBindings = false;

    const auto defs = ui::bindings::Definitions();
    const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
    for (std::size_t i = 0; i < count; ++i) {
        if (!IsBindingDefinition(defs[i])) {
            continue;
        }
        std::vector<std::string> keyboardValues = ui::bindings::SplitBindings(model.keyboard[i].data());
        std::vector<std::string> mouseValues = ui::bindings::SplitBindings(model.mouse[i].data());
        std::vector<std::string> controllerValues = ui::bindings::SplitBindings(model.controller[i].data());

        std::vector<std::string> combined;
        combined.reserve(keyboardValues.size() + mouseValues.size());
        for (const auto &value : keyboardValues) {
            if (!value.empty()) {
                combined.push_back(value);
            }
        }
        for (const auto &value : mouseValues) {
            if (!value.empty()) {
                combined.push_back(value);
            }
        }

        if (!combined.empty()) {
            keybindings[defs[i].action] = combined;
            hasBindings = true;
        }

        if (!controllerValues.empty()) {
            controllerBindings[defs[i].action] = controllerValues;
            hasControllerBindings = true;
        }
    }

    if (!karma::config::ConfigStore::Initialized()) {
        result.ok = false;
        result.status = "Failed to save bindings.";
        result.statusIsError = true;
        return result;
    }

    if (hasBindings) {
        if (!ui::UiConfig::SetKeybindings(keybindings)) {
            result.ok = false;
            result.status = "Failed to save bindings.";
            result.statusIsError = true;
            return result;
        }
    } else {
        ui::UiConfig::EraseKeybindings();
    }

    if (hasControllerBindings) {
        if (!ui::UiConfig::SetControllerKeybindings(controllerBindings)) {
            result.ok = false;
            result.status = "Failed to save bindings.";
            result.statusIsError = true;
            return result;
        }
    } else {
        ui::UiConfig::EraseControllerKeybindings();
    }

    result.ok = true;
    result.status = "Bindings saved.";
    result.statusIsError = false;
    return result;
}

BindingsController::Result BindingsController::resetToDefaults() {
    Result result;
    const auto defs = ui::bindings::Definitions();
    const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
    for (std::size_t i = 0; i < count; ++i) {
        if (!IsBindingDefinition(defs[i])) {
            model.keyboard[i][0] = '\0';
            model.mouse[i][0] = '\0';
            model.controller[i][0] = '\0';
            continue;
        }
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        const auto &defaults = defaultBindingsForAction(defs[i].action);
        for (const auto &value : defaults) {
            if (ui::bindings::IsMouseBindingName(value)) {
                mouseEntries.push_back(value);
            } else {
                keyboardEntries.push_back(value);
            }
        }
        WriteBuffer(model.keyboard[i], ui::bindings::JoinBindings(keyboardEntries));
        WriteBuffer(model.mouse[i], ui::bindings::JoinBindings(mouseEntries));
        model.controller[i][0] = '\0';
    }

    ui::UiConfig::EraseKeybindings();
    ui::UiConfig::EraseControllerKeybindings();
    result.status = "Bindings reset to defaults.";
    result.statusIsError = false;
    return result;
}

} // namespace ui

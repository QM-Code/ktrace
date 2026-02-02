#include "input/input.hpp"

#include "platform/window.hpp"
#include "common/config_store.hpp"

#include <stdexcept>
#include <spdlog/spdlog.h>

Input::Input(platform::Window &window, input::InputMap::DefaultBindingsMap defaultBindings) {
    this->window = &window;
    defaultBindings_ = std::move(defaultBindings);
    loadKeyBindings();
}

void Input::loadKeyBindings() {
    auto keybindingsOpt = karma::config::ConfigStore::GetCopy("keybindings");
    if (!keybindingsOpt || !keybindingsOpt->is_object()) {
        throw std::runtime_error("Input: required config 'keybindings' is missing or not a JSON object");
    }
    mapper_.loadBindings(&(*keybindingsOpt), defaultBindings_);
}

void Input::update(const std::vector<platform::Event> &events) {
    lastEvents_ = events;
}

void Input::pumpEvents(const std::vector<platform::Event> &events) {
    update(events);
}

bool Input::actionTriggered(std::string_view actionId) const {
    return mapper_.actionTriggered(actionId, lastEvents_);
}

bool Input::actionDown(std::string_view actionId) const {
    return mapper_.actionDown(actionId, window);
}

void Input::reloadKeyBindings() {
    loadKeyBindings();
}

std::string Input::bindingListDisplay(std::string_view actionId) const {
    return mapper_.bindingListDisplay(actionId);
}

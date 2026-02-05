#include "karma/input/input_system.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "karma/platform/window.hpp"

#include <cctype>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace karma::input {
namespace {

std::string toLowerKeyName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

platform::Key parseKeyName(const std::string& name) {
    const std::string token = toLowerKeyName(name);
    if (token.size() == 1) {
        const char ch = token[0];
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<platform::Key>(
                static_cast<int>(platform::Key::A) + (ch - 'a'));
        }
        if (ch >= '0' && ch <= '9') {
            return static_cast<platform::Key>(
                static_cast<int>(platform::Key::Num0) + (ch - '0'));
        }
    }
    if (token.size() > 1 && token[0] == 'f') {
        try {
            const int num = std::stoi(token.substr(1));
            if (num >= 1 && num <= 12) {
                return static_cast<platform::Key>(
                    static_cast<int>(platform::Key::F1) + (num - 1));
            }
        } catch (...) {
        }
    }
    static const std::unordered_map<std::string, platform::Key> kNamedKeys = {
        {"left", platform::Key::Left},
        {"right", platform::Key::Right},
        {"up", platform::Key::Up},
        {"down", platform::Key::Down},
        {"minus", platform::Key::Minus},
        {"equals", platform::Key::Equals},
        {"left_bracket", platform::Key::LeftBracket},
        {"right_bracket", platform::Key::RightBracket},
        {"backslash", platform::Key::Backslash},
        {"semicolon", platform::Key::Semicolon},
        {"apostrophe", platform::Key::Apostrophe},
        {"comma", platform::Key::Comma},
        {"slash", platform::Key::Slash},
        {"grave", platform::Key::Grave},
        {"left_shift", platform::Key::LeftShift},
        {"right_shift", platform::Key::RightShift},
        {"left_ctrl", platform::Key::LeftControl},
        {"right_ctrl", platform::Key::RightControl},
        {"left_alt", platform::Key::LeftAlt},
        {"right_alt", platform::Key::RightAlt},
        {"left_super", platform::Key::LeftSuper},
        {"right_super", platform::Key::RightSuper},
        {"menu", platform::Key::Menu},
        {"home", platform::Key::Home},
        {"end", platform::Key::End},
        {"page_up", platform::Key::PageUp},
        {"page_down", platform::Key::PageDown},
        {"insert", platform::Key::Insert},
        {"delete", platform::Key::Delete},
        {"caps_lock", platform::Key::CapsLock},
        {"num_lock", platform::Key::NumLock},
        {"scroll_lock", platform::Key::ScrollLock},
        {"enter", platform::Key::Enter},
        {"space", platform::Key::Space},
        {"tab", platform::Key::Tab},
        {"period", platform::Key::Period},
        {"backspace", platform::Key::Backspace},
        {"escape", platform::Key::Escape}
    };
    if (const auto it = kNamedKeys.find(token); it != kNamedKeys.end()) {
        return it->second;
    }
    return platform::Key::Unknown;
}

std::optional<platform::MouseButton> parseMouseButtonName(const std::string& name) {
    const std::string token = toLowerKeyName(name);
    if (token == "left_mouse") return platform::MouseButton::Left;
    if (token == "right_mouse") return platform::MouseButton::Right;
    if (token == "middle_mouse") return platform::MouseButton::Middle;
    return std::nullopt;
}

bool isKeyDown(const platform::Window& window, platform::Key key) {
    return window.isKeyDown(key);
}

bool isMouseDown(const platform::Window& window, platform::MouseButton button) {
    return window.isMouseDown(button);
}

bool isKeyEvent(const platform::Event& event, platform::Key key, platform::EventType type) {
    return event.type == type && event.key == key;
}

bool isMouseEvent(const platform::Event& event, platform::MouseButton button,
                  platform::EventType type) {
    return event.type == type && event.mouse_button == button;
}

} // namespace

void InputSystem::bindKey(const std::string& action, platform::Key key, Trigger trigger) {
    bindings_[action].push_back(Binding{.trigger = trigger, .key = key, .use_key = true});
}

void InputSystem::bindMouse(const std::string& action, platform::MouseButton button,
                            Trigger trigger) {
    bindings_[action].push_back(Binding{.trigger = trigger, .mouse = button, .use_key = false});
}

void InputSystem::setRequiredModifiers(const std::string& action, platform::Modifiers mods) {
    for (auto& binding : bindings_[action]) {
        binding.mods = mods;
    }
}

bool InputSystem::matchesModifiers(const platform::Modifiers& event_mods,
                                   const platform::Modifiers& required_mods) const {
    if (required_mods.shift && !event_mods.shift) {
        return false;
    }
    if (required_mods.ctrl && !event_mods.ctrl) {
        return false;
    }
    if (required_mods.alt && !event_mods.alt) {
        return false;
    }
    if (required_mods.super && !event_mods.super) {
        return false;
    }
    return true;
}

void InputSystem::update(const std::vector<platform::Event>& events) {
    pressed_this_frame_.clear();
    down_this_frame_.clear();
    mouse_delta_x_ = 0.0f;
    mouse_delta_y_ = 0.0f;

    if (window_) {
        for (const auto& [action, bindings] : bindings_) {
            for (const auto& binding : bindings) {
                if (binding.trigger != Trigger::Down) {
                    continue;
                }
                if (binding.use_key && isKeyDown(*window_, binding.key)) {
                    down_this_frame_.insert(action);
                } else if (!binding.use_key && isMouseDown(*window_, binding.mouse)) {
                    down_this_frame_.insert(action);
                }
            }
        }
    }

    for (const auto& event : events) {
        if (event.type == platform::EventType::MouseMove) {
            if (has_mouse_pos_) {
                mouse_delta_x_ += static_cast<float>(event.mouse_x - last_mouse_x_);
                mouse_delta_y_ += static_cast<float>(event.mouse_y - last_mouse_y_);
            }
            last_mouse_x_ = event.mouse_x;
            last_mouse_y_ = event.mouse_y;
            has_mouse_pos_ = true;
        }
        for (const auto& [action, bindings] : bindings_) {
            for (const auto& binding : bindings) {
                if (binding.trigger != Trigger::Pressed) {
                    continue;
                }
                if (!matchesModifiers(event.mods, binding.mods)) {
                    continue;
                }
                if (binding.use_key &&
                    isKeyEvent(event, binding.key, platform::EventType::KeyDown)) {
                    pressed_this_frame_.insert(action);
                }
                if (!binding.use_key &&
                    isMouseEvent(event, binding.mouse, platform::EventType::MouseButtonDown)) {
                    pressed_this_frame_.insert(action);
                }
            }
        }
    }
}

bool InputSystem::actionDown(const std::string& action) const {
    return down_this_frame_.find(action) != down_this_frame_.end();
}

bool InputSystem::actionPressed(const std::string& action) const {
    return pressed_this_frame_.find(action) != pressed_this_frame_.end();
}

void InputSystem::clear() {
    pressed_this_frame_.clear();
    down_this_frame_.clear();
    mouse_delta_x_ = 0.0f;
    mouse_delta_y_ = 0.0f;
}

void InputContext::setWindow(const platform::Window* window) {
    global_.setWindow(window);
    game_.setWindow(window);
    roaming_.setWindow(window);
}

void InputContext::update(const std::vector<platform::Event>& events) {
    global_.update(events);
    game_.update(events);
    roaming_.update(events);
}

void InputContext::clear() {
    global_.clear();
    game_.clear();
    roaming_.clear();
}

const InputSystem& InputContext::activeSystem() const {
    return mode_ == InputMode::Roaming ? roaming_ : game_;
}

InputSystem& InputContext::activeSystem() {
    return mode_ == InputMode::Roaming ? roaming_ : game_;
}

bool InputContext::actionDown(const std::string& action) const {
    if (global_.actionDown(action)) {
        return true;
    }
    return activeSystem().actionDown(action);
}

bool InputContext::actionPressed(const std::string& action) const {
    if (global_.actionPressed(action)) {
        return true;
    }
    return activeSystem().actionPressed(action);
}

namespace {
void LoadBindingSection(const karma::json::Value& section,
                        const std::string& label,
                        InputSystem& system,
                        bool allow_reserved) {
    if (!section.is_object()) {
        throw std::runtime_error("Bindings section '" + label + "' must be an object");
    }
    static const std::unordered_set<std::string> kReserved = {
        "escape",
        "grave"
    };
    for (auto it = section.begin(); it != section.end(); ++it) {
        const std::string action = it.key();
        const auto& value = it.value();
        if (!value.is_array()) {
            throw std::runtime_error("Binding '" + label + "." + action + "' must be an array");
        }
        for (const auto& entry : value) {
            if (!entry.is_string()) {
                throw std::runtime_error("Binding '" + label + "." + action + "' contains a non-string value");
            }
            const std::string keyName = entry.get<std::string>();
            const std::string token = toLowerKeyName(keyName);
            if (!allow_reserved && kReserved.find(token) != kReserved.end()) {
                throw std::runtime_error("Binding '" + label + "." + action + "' uses reserved key '" + keyName + "'");
            }
            const platform::Key key = parseKeyName(keyName);
            if (key != platform::Key::Unknown) {
                system.bindKey(action, key, Trigger::Down);
                KARMA_TRACE("config.bindings", "Binding {}.{} -> {}", label, action, keyName);
                continue;
            }
            const auto button = parseMouseButtonName(keyName);
            if (button) {
                system.bindMouse(action, *button, Trigger::Down);
                KARMA_TRACE("config.bindings", "Binding {}.{} -> {}", label, action, keyName);
                continue;
            }
            throw std::runtime_error("Binding '" + label + "." + action + "' uses unknown key '" + keyName + "'");
        }
    }
}
} // namespace

void LoadBindingsFromConfig(InputContext& context) {
    const auto& root = karma::config::ReadRequiredObjectConfig("bindings");
    const auto* global = root.contains("global") ? &root["global"] : nullptr;
    const auto* game = root.contains("game") ? &root["game"] : nullptr;
    const auto* roaming = root.contains("roaming") ? &root["roaming"] : nullptr;
    if (!global || !game || !roaming) {
        throw std::runtime_error("Bindings config must define 'global', 'game', and 'roaming'");
    }
    LoadBindingSection(*global, "global", context.global(), true);
    LoadBindingSection(*game, "game", context.game(), false);
    LoadBindingSection(*roaming, "roaming", context.roaming(), false);
    KARMA_TRACE("config.bindings", "Bindings applied: global={}, game={}, roaming={}",
                global->size(), game->size(), roaming->size());
}

} // namespace karma::input

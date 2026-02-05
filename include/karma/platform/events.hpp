#pragma once

#include <string>

namespace karma::platform {

enum class Key {
    Unknown,
    W,
    A,
    S,
    D,
    Escape
};

enum class MouseButton {
    Left,
    Right,
    Middle
};

struct Modifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

enum class EventType {
    None,
    Quit,
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    TextInput,
    WindowResized
};

struct Event {
    EventType type = EventType::None;
    Key key = Key::Unknown;
    MouseButton mouse_button = MouseButton::Left;
    Modifiers mods{};
    int mouse_x = 0;
    int mouse_y = 0;
    int window_w = 0;
    int window_h = 0;
    unsigned int codepoint = 0;
    std::string text;
};

struct WindowConfig {
    std::string title = "BZ3";
    int width = 1280;
    int height = 720;
    bool resizable = true;
    std::string preferredVideoDriver;
    bool fullscreen = false;
    bool wayland_libdecor = true;
};

} // namespace karma::platform

#pragma once

#include <string>

namespace karma::platform {

enum class Key {
    Unknown,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Comma,
    Slash,
    Grave,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    LeftSuper,
    RightSuper,
    Menu,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    CapsLock,
    NumLock,
    ScrollLock,
    Left,
    Right,
    Up,
    Down,
    LeftShift,
    RightShift,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Enter,
    Space,
    Tab,
    Period,
    Backspace,
    Escape
};

enum class MouseButton {
    Left,
    Right,
    Middle,
    Button4,
    Button5,
    Button6,
    Button7,
    Button8
};

struct Modifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;
};

enum class EventType {
    None,
    Quit,
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseScroll,
    TextInput,
    WindowResized,
    WindowFocus,
    WindowClose
};

struct Event {
    EventType type = EventType::None;
    Key key = Key::Unknown;
    MouseButton mouse_button = MouseButton::Left;
    Modifiers mods{};
    int mouse_x = 0;
    int mouse_y = 0;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    int window_w = 0;
    int window_h = 0;
    unsigned int codepoint = 0;
    std::string text;
    bool focused = true;
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

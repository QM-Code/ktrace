#include "platform/backends/window_sdl3.hpp"

#include "karma/common/logging.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_hints.h>

#include <algorithm>
#include <cstdlib>

#include <spdlog/spdlog.h>

namespace karma::platform {

namespace {
Key toKey(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_0: return Key::Num0;
        case SDL_SCANCODE_1: return Key::Num1;
        case SDL_SCANCODE_2: return Key::Num2;
        case SDL_SCANCODE_3: return Key::Num3;
        case SDL_SCANCODE_4: return Key::Num4;
        case SDL_SCANCODE_5: return Key::Num5;
        case SDL_SCANCODE_6: return Key::Num6;
        case SDL_SCANCODE_7: return Key::Num7;
        case SDL_SCANCODE_8: return Key::Num8;
        case SDL_SCANCODE_9: return Key::Num9;
        case SDL_SCANCODE_MINUS: return Key::Minus;
        case SDL_SCANCODE_EQUALS: return Key::Equals;
        case SDL_SCANCODE_LEFTBRACKET: return Key::LeftBracket;
        case SDL_SCANCODE_RIGHTBRACKET: return Key::RightBracket;
        case SDL_SCANCODE_BACKSLASH: return Key::Backslash;
        case SDL_SCANCODE_SEMICOLON: return Key::Semicolon;
        case SDL_SCANCODE_APOSTROPHE: return Key::Apostrophe;
        case SDL_SCANCODE_COMMA: return Key::Comma;
        case SDL_SCANCODE_SLASH: return Key::Slash;
        case SDL_SCANCODE_GRAVE: return Key::Grave;
        case SDL_SCANCODE_LCTRL: return Key::LeftControl;
        case SDL_SCANCODE_RCTRL: return Key::RightControl;
        case SDL_SCANCODE_LALT: return Key::LeftAlt;
        case SDL_SCANCODE_RALT: return Key::RightAlt;
        case SDL_SCANCODE_LGUI: return Key::LeftSuper;
        case SDL_SCANCODE_RGUI: return Key::RightSuper;
        case SDL_SCANCODE_MENU: return Key::Menu;
        case SDL_SCANCODE_HOME: return Key::Home;
        case SDL_SCANCODE_END: return Key::End;
        case SDL_SCANCODE_PAGEUP: return Key::PageUp;
        case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
        case SDL_SCANCODE_INSERT: return Key::Insert;
        case SDL_SCANCODE_DELETE: return Key::Delete;
        case SDL_SCANCODE_CAPSLOCK: return Key::CapsLock;
        case SDL_SCANCODE_NUMLOCKCLEAR: return Key::NumLock;
        case SDL_SCANCODE_SCROLLLOCK: return Key::ScrollLock;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
        case SDL_SCANCODE_RSHIFT: return Key::RightShift;
        case SDL_SCANCODE_F1: return Key::F1;
        case SDL_SCANCODE_F2: return Key::F2;
        case SDL_SCANCODE_F3: return Key::F3;
        case SDL_SCANCODE_F4: return Key::F4;
        case SDL_SCANCODE_F5: return Key::F5;
        case SDL_SCANCODE_F6: return Key::F6;
        case SDL_SCANCODE_F7: return Key::F7;
        case SDL_SCANCODE_F8: return Key::F8;
        case SDL_SCANCODE_F9: return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_PERIOD: return Key::Period;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        default: return Key::Unknown;
    }
}

SDL_Scancode toScancode(Key key) {
    switch (key) {
        case Key::A: return SDL_SCANCODE_A;
        case Key::B: return SDL_SCANCODE_B;
        case Key::C: return SDL_SCANCODE_C;
        case Key::D: return SDL_SCANCODE_D;
        case Key::E: return SDL_SCANCODE_E;
        case Key::F: return SDL_SCANCODE_F;
        case Key::G: return SDL_SCANCODE_G;
        case Key::H: return SDL_SCANCODE_H;
        case Key::I: return SDL_SCANCODE_I;
        case Key::J: return SDL_SCANCODE_J;
        case Key::K: return SDL_SCANCODE_K;
        case Key::L: return SDL_SCANCODE_L;
        case Key::M: return SDL_SCANCODE_M;
        case Key::N: return SDL_SCANCODE_N;
        case Key::O: return SDL_SCANCODE_O;
        case Key::P: return SDL_SCANCODE_P;
        case Key::Q: return SDL_SCANCODE_Q;
        case Key::R: return SDL_SCANCODE_R;
        case Key::S: return SDL_SCANCODE_S;
        case Key::T: return SDL_SCANCODE_T;
        case Key::U: return SDL_SCANCODE_U;
        case Key::V: return SDL_SCANCODE_V;
        case Key::W: return SDL_SCANCODE_W;
        case Key::X: return SDL_SCANCODE_X;
        case Key::Y: return SDL_SCANCODE_Y;
        case Key::Z: return SDL_SCANCODE_Z;
        case Key::Num0: return SDL_SCANCODE_0;
        case Key::Num1: return SDL_SCANCODE_1;
        case Key::Num2: return SDL_SCANCODE_2;
        case Key::Num3: return SDL_SCANCODE_3;
        case Key::Num4: return SDL_SCANCODE_4;
        case Key::Num5: return SDL_SCANCODE_5;
        case Key::Num6: return SDL_SCANCODE_6;
        case Key::Num7: return SDL_SCANCODE_7;
        case Key::Num8: return SDL_SCANCODE_8;
        case Key::Num9: return SDL_SCANCODE_9;
        case Key::Minus: return SDL_SCANCODE_MINUS;
        case Key::Equals: return SDL_SCANCODE_EQUALS;
        case Key::LeftBracket: return SDL_SCANCODE_LEFTBRACKET;
        case Key::RightBracket: return SDL_SCANCODE_RIGHTBRACKET;
        case Key::Backslash: return SDL_SCANCODE_BACKSLASH;
        case Key::Semicolon: return SDL_SCANCODE_SEMICOLON;
        case Key::Apostrophe: return SDL_SCANCODE_APOSTROPHE;
        case Key::Comma: return SDL_SCANCODE_COMMA;
        case Key::Slash: return SDL_SCANCODE_SLASH;
        case Key::Grave: return SDL_SCANCODE_GRAVE;
        case Key::LeftControl: return SDL_SCANCODE_LCTRL;
        case Key::RightControl: return SDL_SCANCODE_RCTRL;
        case Key::LeftAlt: return SDL_SCANCODE_LALT;
        case Key::RightAlt: return SDL_SCANCODE_RALT;
        case Key::LeftSuper: return SDL_SCANCODE_LGUI;
        case Key::RightSuper: return SDL_SCANCODE_RGUI;
        case Key::Menu: return SDL_SCANCODE_MENU;
        case Key::Home: return SDL_SCANCODE_HOME;
        case Key::End: return SDL_SCANCODE_END;
        case Key::PageUp: return SDL_SCANCODE_PAGEUP;
        case Key::PageDown: return SDL_SCANCODE_PAGEDOWN;
        case Key::Insert: return SDL_SCANCODE_INSERT;
        case Key::Delete: return SDL_SCANCODE_DELETE;
        case Key::CapsLock: return SDL_SCANCODE_CAPSLOCK;
        case Key::NumLock: return SDL_SCANCODE_NUMLOCKCLEAR;
        case Key::ScrollLock: return SDL_SCANCODE_SCROLLLOCK;
        case Key::Left: return SDL_SCANCODE_LEFT;
        case Key::Right: return SDL_SCANCODE_RIGHT;
        case Key::Up: return SDL_SCANCODE_UP;
        case Key::Down: return SDL_SCANCODE_DOWN;
        case Key::LeftShift: return SDL_SCANCODE_LSHIFT;
        case Key::RightShift: return SDL_SCANCODE_RSHIFT;
        case Key::F1: return SDL_SCANCODE_F1;
        case Key::F2: return SDL_SCANCODE_F2;
        case Key::F3: return SDL_SCANCODE_F3;
        case Key::F4: return SDL_SCANCODE_F4;
        case Key::F5: return SDL_SCANCODE_F5;
        case Key::F6: return SDL_SCANCODE_F6;
        case Key::F7: return SDL_SCANCODE_F7;
        case Key::F8: return SDL_SCANCODE_F8;
        case Key::F9: return SDL_SCANCODE_F9;
        case Key::F10: return SDL_SCANCODE_F10;
        case Key::F11: return SDL_SCANCODE_F11;
        case Key::F12: return SDL_SCANCODE_F12;
        case Key::Enter: return SDL_SCANCODE_RETURN;
        case Key::Backspace: return SDL_SCANCODE_BACKSPACE;
        case Key::Space: return SDL_SCANCODE_SPACE;
        case Key::Tab: return SDL_SCANCODE_TAB;
        case Key::Period: return SDL_SCANCODE_PERIOD;
        case Key::Escape: return SDL_SCANCODE_ESCAPE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

MouseButton toMouseButton(uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_X1: return MouseButton::Button4;
        case SDL_BUTTON_X2: return MouseButton::Button5;
        default: return MouseButton::Left;
    }
}

Modifiers toModifiers(SDL_Keymod mod) {
    Modifiers out{};
    out.shift = (mod & SDL_KMOD_SHIFT) != 0;
    out.ctrl = (mod & SDL_KMOD_CTRL) != 0;
    out.alt = (mod & SDL_KMOD_ALT) != 0;
    out.super = (mod & SDL_KMOD_GUI) != 0;
    return out;
}
}

namespace {
void SetEnvVar(const char* name, const char* value) {
#if defined(_WIN32)
    if (value) {
        _putenv_s(name, value);
    } else {
        _putenv_s(name, "");
    }
#else
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}
}

WindowSdl3::WindowSdl3(const WindowConfig& config) {
    preferred_video_driver_ = config.preferredVideoDriver;
    if (!config.preferredVideoDriver.empty()) {
        SetEnvVar("SDL_VIDEODRIVER", config.preferredVideoDriver.c_str());
    }
    if (config.wayland_libdecor) {
        SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "1");
        SetEnvVar("SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR", "1");
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
        SetEnvVar("SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR", "0");
    }
    bool sdl_init_ok = SDL_Init(SDL_INIT_VIDEO);
    if (!sdl_init_ok) {
        if (!config.preferredVideoDriver.empty()) {
            spdlog::warn("SDL_Init failed with preferred driver '{}': {}", config.preferredVideoDriver, SDL_GetError());
            SetEnvVar("SDL_VIDEODRIVER", nullptr);
            sdl_init_ok = SDL_Init(SDL_INIT_VIDEO);
            if (!sdl_init_ok) {
                spdlog::error("SDL_Init failed: {}", SDL_GetError());
                should_close_ = true;
                return;
            }
        } else {
            spdlog::error("SDL_Init failed: {}", SDL_GetError());
            should_close_ = true;
            return;
        }
    }
    KARMA_TRACE("platform.sdl", "SDL_Init(SDL_INIT_VIDEO) returned {}", sdl_init_ok ? 1 : 0);

    KARMA_TRACE("platform.sdl",
                "SDL video driver: {}",
                SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(null)");

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
    window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (!window_) {
        spdlog::error("SDL window failed to create: {}", SDL_GetError());
        SDL_Quit();
        should_close_ = true;
        return;
    }

    SDL_StartTextInput(window_);

    const float scale = SDL_GetWindowDisplayScale(window_);
    if (scale > 0.0f) {
        content_scale_ = std::max(1.0f, scale);
    }
}

WindowSdl3::~WindowSdl3() {
    if (window_) {
        SDL_StopTextInput(window_);
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

void WindowSdl3::pollEvents() {
    events_.clear();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        Event out{};
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                out.type = EventType::Quit;
                should_close_ = true;
                events_.push_back(out);
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                out.type = EventType::WindowResized;
                out.window_w = ev.window.data1;
                out.window_h = ev.window.data2;
                events_.push_back(out);
                break;
            case SDL_EVENT_KEY_DOWN:
                out.type = EventType::KeyDown;
                out.key = toKey(ev.key.scancode);
                out.mods = toModifiers(static_cast<SDL_Keymod>(ev.key.mod));
                events_.push_back(out);
                break;
            case SDL_EVENT_KEY_UP:
                out.type = EventType::KeyUp;
                out.key = toKey(ev.key.scancode);
                out.mods = toModifiers(static_cast<SDL_Keymod>(ev.key.mod));
                events_.push_back(out);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                out.type = EventType::MouseMove;
                out.mouse_x = ev.motion.x;
                out.mouse_y = ev.motion.y;
                events_.push_back(out);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                out.type = EventType::MouseScroll;
                out.scroll_x = static_cast<float>(ev.wheel.x);
                out.scroll_y = static_cast<float>(ev.wheel.y);
                events_.push_back(out);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                out.type = EventType::MouseButtonDown;
                out.mouse_button = toMouseButton(ev.button.button);
                out.mouse_x = ev.button.x;
                out.mouse_y = ev.button.y;
                events_.push_back(out);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                out.type = EventType::MouseButtonUp;
                out.mouse_button = toMouseButton(ev.button.button);
                out.mouse_x = ev.button.x;
                out.mouse_y = ev.button.y;
                events_.push_back(out);
                break;
            case SDL_EVENT_TEXT_INPUT:
                out.type = EventType::TextInput;
                out.text = ev.text.text ? ev.text.text : "";
                if (!out.text.empty()) {
                    out.codepoint = static_cast<unsigned int>(out.text[0]);
                }
                events_.push_back(out);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                out.type = EventType::WindowFocus;
                out.focused = true;
                events_.push_back(out);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                out.type = EventType::WindowFocus;
                out.focused = false;
                events_.push_back(out);
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                out.type = EventType::WindowClose;
                should_close_ = true;
                events_.push_back(out);
                break;
            default:
                break;
        }
    }
}

bool WindowSdl3::isKeyDown(Key key) const {
    const SDL_Scancode sc = toScancode(key);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        return false;
    }
    int count = 0;
    const bool* state = SDL_GetKeyboardState(&count);
    if (!state || sc >= count) {
        return false;
    }
    return state[sc] != 0;
}

bool WindowSdl3::isMouseDown(MouseButton button) const {
    Uint32 mask = SDL_GetMouseState(nullptr, nullptr);
    switch (button) {
        case MouseButton::Left: return (mask & SDL_BUTTON_LMASK) != 0;
        case MouseButton::Right: return (mask & SDL_BUTTON_RMASK) != 0;
        case MouseButton::Middle: return (mask & SDL_BUTTON_MMASK) != 0;
        case MouseButton::Button4: return (mask & SDL_BUTTON_X1MASK) != 0;
        case MouseButton::Button5: return (mask & SDL_BUTTON_X2MASK) != 0;
        default: return false;
    }
}

void WindowSdl3::getFramebufferSize(int& w, int& h) const {
    if (!window_) {
        w = 0;
        h = 0;
        return;
    }
    SDL_GetWindowSizeInPixels(window_, &w, &h);
}

void WindowSdl3::setVsync(bool enabled) {
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void WindowSdl3::setFullscreen(bool enabled) {
    if (!window_) return;
    SDL_SetWindowFullscreen(window_, enabled ? SDL_WINDOW_FULLSCREEN : 0);
}

void WindowSdl3::setCursorVisible(bool visible) {
    if (visible) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

NativeWindowHandle WindowSdl3::nativeHandle() const {
    NativeWindowHandle handle{};
    if (!window_) {
        return handle;
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window_);
    if (props == 0) {
        return handle;
    }

    void* wl_display = const_cast<void*>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
    void* wl_surface = const_cast<void*>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    const Sint64 x11_window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    void* x11_display = const_cast<void*>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
    KARMA_TRACE("platform.sdl", "SDL3 native props: wl_display={} wl_surface={} x11_window={} x11_display={}",
                wl_display, wl_surface,
                static_cast<long long>(x11_window), x11_display);

    if (wl_display && wl_surface) {
        handle.display = wl_display;
        handle.wayland_surface = wl_surface;
        handle.is_wayland = true;
        return handle;
    }

    if (preferred_video_driver_ == "wayland") {
        return handle;
    }

    if (x11_window && x11_display) {
        handle.window = reinterpret_cast<void*>(static_cast<intptr_t>(x11_window));
        handle.display = x11_display;
        handle.is_x11 = true;
        return handle;
    }

    return handle;
}

} // namespace karma::platform

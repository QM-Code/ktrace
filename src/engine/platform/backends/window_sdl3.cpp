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
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        default: return Key::Unknown;
    }
}

MouseButton toMouseButton(uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        default: return MouseButton::Left;
    }
}

Modifiers toModifiers(SDL_Keymod mod) {
    Modifiers out{};
    out.shift = (mod & SDL_KMOD_SHIFT) != 0;
    out.ctrl = (mod & SDL_KMOD_CTRL) != 0;
    out.alt = (mod & SDL_KMOD_ALT) != 0;
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
            default:
                break;
        }
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

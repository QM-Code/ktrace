#pragma once

#include <memory>
#include <string>
#include <vector>

#include "karma/platform/events.hpp"

namespace karma::platform {

struct NativeWindowHandle {
    void* window = nullptr;
    void* display = nullptr;
    void* wayland_surface = nullptr;
    bool is_wayland = false;
    bool is_x11 = false;
};

class Window {
 public:
    virtual ~Window() = default;

    virtual void pollEvents() = 0;
    virtual const std::vector<Event>& events() const = 0;
    virtual void clearEvents() = 0;

    virtual bool shouldClose() const = 0;
    virtual void getFramebufferSize(int& w, int& h) const = 0;
    virtual float getContentScale() const = 0;

    virtual void setVsync(bool enabled) = 0;
    virtual void setFullscreen(bool enabled) = 0;
    virtual void setCursorVisible(bool visible) = 0;
    virtual void setIcon(const std::string& /*path*/) {}

    virtual NativeWindowHandle nativeHandle() const = 0;
};

std::unique_ptr<Window> CreateWindow(const WindowConfig& config);

} // namespace karma::platform

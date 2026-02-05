#pragma once

#include "karma/platform/window.hpp"

namespace karma::platform {

class WindowSdl2Stub final : public Window {
 public:
    explicit WindowSdl2Stub(const WindowConfig&) {}
    ~WindowSdl2Stub() override = default;

    void pollEvents() override {}
    const std::vector<Event>& events() const override { return events_; }
    void clearEvents() override { events_.clear(); }

    bool shouldClose() const override { return true; }
    void getFramebufferSize(int& w, int& h) const override { w = 0; h = 0; }
    float getContentScale() const override { return 1.0f; }

    void setVsync(bool) override {}
    void setFullscreen(bool) override {}
    void setCursorVisible(bool) override {}

    NativeWindowHandle nativeHandle() const override { return {}; }

 private:
    std::vector<Event> events_{};
};

} // namespace karma::platform

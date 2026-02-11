#pragma once

#include "karma/platform/window.hpp"

#include <vector>

namespace karma::platform {

class WindowSdl3 final : public Window {
 public:
    explicit WindowSdl3(const WindowConfig& config);
    ~WindowSdl3() override;

    void pollEvents() override;
    const std::vector<Event>& events() const override { return events_; }
    void clearEvents() override { events_.clear(); }

    bool isKeyDown(Key key) const override;
    bool isMouseDown(MouseButton button) const override;

    bool shouldClose() const override { return should_close_; }
    void getFramebufferSize(int& w, int& h) const override;
    float getContentScale() const override { return content_scale_; }

    void setVsync(bool enabled) override;
    void setFullscreen(bool enabled) override;
    void setCursorVisible(bool visible) override;

    NativeWindowHandle nativeHandle() const override;

 private:
    void* window_ = nullptr;
    std::string preferred_video_driver_;
    bool should_close_ = false;
    float content_scale_ = 1.0f;
    std::vector<Event> events_;
};

} // namespace karma::platform

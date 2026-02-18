#pragma once

#include "ui/backends/driver.hpp"

#include <memory>
#include <vector>

namespace karma::ui::backend::rmlui {

class Adapter {
 public:
    virtual ~Adapter() = default;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt, const std::vector<window::Event>& events) = 0;
    virtual void build(const std::vector<UiDrawContext::RmlUiDrawCallback>& draw_callbacks,
                       const std::vector<UiDrawContext::TextPanel>& text_panels,
                       OverlayFrame& out) = 0;
};

std::unique_ptr<Adapter> CreateAdapter();

} // namespace karma::ui::backend::rmlui

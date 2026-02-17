#pragma once

#include "ui/backend.hpp"

#include <memory>
#include <vector>

namespace karma::ui::rmlui {

class Adapter {
 public:
    virtual ~Adapter() = default;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt, const std::vector<platform::Event>& events) = 0;
    virtual void build(const std::vector<UiDrawContext::RmlUiDrawCallback>& draw_callbacks,
                       const std::vector<UiDrawContext::TextPanel>& text_panels,
                       OverlayFrame& out) = 0;
};

std::unique_ptr<Adapter> CreateAdapter();

} // namespace karma::ui::rmlui

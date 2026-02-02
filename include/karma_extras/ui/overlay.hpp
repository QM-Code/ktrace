#pragma once

#include "karma_extras/ui/types.hpp"
#include "karma/platform/events.hpp"
#include <vector>

namespace ui {

class Overlay {
public:
    virtual ~Overlay() = default;

    virtual void handleEvents(const std::vector<platform::Event>& events) = 0;
    virtual void update() = 0;
    virtual RenderOutput getRenderOutput() const = 0;
    virtual float getRenderBrightness() const = 0;
};

} // namespace ui

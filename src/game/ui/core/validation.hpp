#pragma once

#include <cstdint>
#include <string_view>

#include "ui/models/hud_model.hpp"
#include "ui/models/hud_render_state.hpp"

namespace ui {

struct HudValidationResult {
    bool matches = true;
    uint32_t mismatchCount = 0;
};

HudRenderState BuildExpectedHudState(const HudModel &model, bool consoleVisible);
HudValidationResult ValidateHudState(const HudRenderState &expected,
                                    const HudRenderState &actual,
                                    std::string_view backendName);

class HudValidator {
public:
    void validate(const HudRenderState &expected,
                  const HudRenderState &actual,
                  std::string_view backendName);

private:
    uint64_t lastHash = 0;
};

} // namespace ui

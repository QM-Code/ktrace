#include "ui/config/render_scale.hpp"

#include "karma/common/config_store.hpp"
#include "spdlog/spdlog.h"
#include "ui/config/ui_config.hpp"

#include <algorithm>

namespace ui {

float GetUiRenderScale() {
    static uint64_t lastRevision = 0;
    static float cachedScale = UiConfig::GetRenderScale();
    const uint64_t revision = karma::config::ConfigStore::Revision();
    if (revision != lastRevision) {
        lastRevision = revision;
        const float value = UiConfig::GetRenderScale();
        const float clamped = std::clamp(value, UiConfig::kMinRenderScale, UiConfig::kMaxRenderScale);
        if (clamped != value) {
            spdlog::warn("Config 'ui.RenderScale' clamped from {} to {}", value, clamped);
        }
        cachedScale = clamped;
    }
    return cachedScale;
}

} // namespace ui

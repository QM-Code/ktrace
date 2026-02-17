#include "ui/backends/rmlui/internal.hpp"

#if defined(KARMA_HAS_RMLUI)

#include "karma/common/logging.hpp"

#include <chrono>

#include <spdlog/spdlog.h>

namespace karma::ui::rmlui {

double SystemInterface::GetElapsedTime() {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - start_;
    return elapsed.count();
}

bool SystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    switch (type) {
        case Rml::Log::Type::LT_ERROR:
            spdlog::error("RmlUi: {}", message);
            break;
        case Rml::Log::Type::LT_WARNING:
            spdlog::warn("RmlUi: {}", message);
            break;
        case Rml::Log::Type::LT_INFO:
        case Rml::Log::Type::LT_DEBUG:
        case Rml::Log::Type::LT_ASSERT:
        default:
            KARMA_TRACE("ui.system.rmlui", "RmlUi: {}", message);
            break;
    }
    return true;
}

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)

#pragma once

#include <array>
#include <span>

namespace ui {

struct ConsoleTabSpec {
    const char *key = nullptr;
    const char *labelKey = nullptr;
    const char *fallbackLabel = nullptr;
    bool rightAlign = false;
    bool refreshOnActivate = false;
};

inline constexpr std::array<ConsoleTabSpec, 5> kConsoleTabSpecs = {{
    {"community", "ui.console.tabs.community", nullptr, false, true},
    {"start-server", "ui.console.tabs.start_server", nullptr, false, false},
    {"settings", "ui.console.tabs.settings", nullptr, false, false},
    {"bindings", "ui.console.tabs.bindings", nullptr, false, false},
    {"documentation", "ui.console.tabs.help", nullptr, true, false}
}};

constexpr std::span<const ConsoleTabSpec> GetConsoleTabSpecs() {
    return std::span<const ConsoleTabSpec>(kConsoleTabSpecs);
}

} // namespace ui

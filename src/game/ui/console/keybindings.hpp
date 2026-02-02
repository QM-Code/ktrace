#pragma once

#include "karma/input/bindings_text.hpp"

#include <string>
#include <span>
#include <vector>

namespace ui::bindings {

struct BindingDefinition {
    const char *action;
    const char *label;
    bool isHeader = false;
};

std::span<const BindingDefinition> Definitions();

inline bool IsMouseBindingName(std::string_view name) {
    return input::bindings::IsMouseBindingName(name);
}

bool IsReservedBindingName(std::string_view name);

inline std::string JoinBindings(const std::vector<std::string> &entries) {
    return input::bindings::JoinBindings(entries);
}

inline std::vector<std::string> SplitBindings(const std::string &text) {
    return input::bindings::SplitBindings(text);
}

} // namespace ui::bindings

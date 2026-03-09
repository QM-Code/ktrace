#include <ktrace.hpp>

#include "ktrace.hpp"

#include <stdexcept>

namespace ktrace {

ColorId Color(std::string_view color_name) {
    const std::string token = detail::trimWhitespace(std::string(color_name));
    if (token.empty()) {
        throw std::invalid_argument("trace color name must not be empty");
    }

    if (token == "Default" || token == "default") {
        return kDefaultColor;
    }

    const auto& names = detail::colorNames();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == token) {
            return static_cast<ColorId>(i);
        }
    }

    throw std::invalid_argument("unknown trace color '" + token + "'");
}

void Initialize() {
    detail::ensureInternalTraceChannelsRegistered();
}

OutputOptions detail::getRequestedOutputOptions() {
    auto& state = getTraceState();
    return {
        .filenames = state.requested_filenames.load(std::memory_order_relaxed),
        .line_numbers = state.requested_line_numbers.load(std::memory_order_relaxed),
        .function_names = state.requested_function_names.load(std::memory_order_relaxed),
        .timestamps = state.requested_timestamps.load(std::memory_order_relaxed),
    };
}

void SetOutputOptions(const OutputOptions& options) {
    detail::ensureInternalTraceChannelsRegistered();
    const bool line_numbers_enabled = options.filenames && options.line_numbers;
    const bool function_names_enabled = options.filenames && options.function_names;

    auto& state = detail::getTraceState();
    state.requested_filenames.store(options.filenames, std::memory_order_relaxed);
    state.requested_line_numbers.store(options.line_numbers, std::memory_order_relaxed);
    state.requested_function_names.store(options.function_names, std::memory_order_relaxed);
    state.requested_timestamps.store(options.timestamps, std::memory_order_relaxed);
    state.filenames_enabled.store(options.filenames, std::memory_order_relaxed);
    state.line_numbers_enabled.store(line_numbers_enabled, std::memory_order_relaxed);
    state.function_names_enabled.store(function_names_enabled, std::memory_order_relaxed);
    state.timestamps_enabled.store(options.timestamps, std::memory_order_relaxed);
    KTRACE("api", "updating output options (enable api.output for details)");
    KTRACE("api.output",
           "set output options: filenames={} line_numbers={} function_names={} timestamps={}",
           options.filenames,
           line_numbers_enabled,
           function_names_enabled,
           options.timestamps);
}

} // namespace ktrace

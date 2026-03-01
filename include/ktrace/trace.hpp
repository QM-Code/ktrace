#pragma once

#ifndef KTRACE_NAMESPACE
#error "KTRACE_NAMESPACE must be defined for trace logging."
#endif

#include <cstdint>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ktrace {

using ColorId = std::uint16_t;
inline constexpr ColorId kDefaultColor = 0xFFFFu;

struct OutputOptions {
    bool filenames = false;
    bool line_numbers = false;
    bool function_names = false;
    bool timestamps = false;
};

ColorId Color(std::string_view color_name);
void SetOutputOptions(const OutputOptions& options);
void EnableInternalTrace();
// Processes and consumes all argv entries that begin with trace_root (for example "--trace*").
void ProcessCLI(int& argc, char** argv, std::string_view trace_root = "--trace");
std::vector<std::string> GetNamespaces();
std::vector<std::string> GetChannels(std::string_view trace_namespace);
void EnableChannel(std::string_view qualified_channel,
                   std::string_view trace_namespace = KTRACE_NAMESPACE);
void EnableChannels(std::string_view selectors_csv);
bool ShouldTraceChannel(std::string_view qualified_channel,
                        std::string_view trace_namespace = KTRACE_NAMESPACE);
void DisableChannel(std::string_view qualified_channel,
                    std::string_view trace_namespace = KTRACE_NAMESPACE);
void DisableChannels(std::string_view selectors_csv);
void ClearEnabledChannels();

namespace detail {

void RegisterChannelBridge(std::string_view trace_namespace,
                           std::string_view channel,
                           ColorId color);
void WriteBridge(std::string_view trace_namespace,
                 std::string_view category,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message);

} // namespace detail

inline void RegisterChannel(std::string_view channel) {
    detail::RegisterChannelBridge(KTRACE_NAMESPACE, channel, kDefaultColor);
}

inline void RegisterChannel(std::string_view channel, ColorId color) {
    detail::RegisterChannelBridge(KTRACE_NAMESPACE, channel, color);
}

#define KTRACE(cat, format_text, ...)                                        \
    do {                                                                          \
        ::ktrace::detail::WriteBridge(KTRACE_NAMESPACE,                          \
                                      (cat),                                      \
                                      __FILE__,                                   \
                                      __LINE__,                                   \
                                      __func__,                                   \
                                      fmt::format((format_text), ##__VA_ARGS__)); \
    } while (0)

#define KTRACE_CHANGED(cat, key_expr, format_text, ...)                      \
    do {                                                                          \
        static std::string last_key;                                              \
        std::string next_key = (key_expr);                                        \
        if (next_key != last_key) {                                               \
            last_key = std::move(next_key);                                       \
            ::ktrace::detail::WriteBridge(KTRACE_NAMESPACE,                       \
                                          (cat),                                   \
                                          __FILE__,                                \
                                          __LINE__,                                \
                                          __func__,                                \
                                          fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

} // namespace ktrace

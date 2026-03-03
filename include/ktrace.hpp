#pragma once

#ifndef KTRACE_NAMESPACE
#error "KTRACE_NAMESPACE must be defined for trace logging."
#endif

#include <cstdint>
#include <mutex>
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
void ProcessCLI(int& argc,
                char** argv,
                std::string_view trace_root = "--trace",
                std::string_view local_namespace = KTRACE_NAMESPACE);
std::vector<std::string> GetNamespaces();
std::vector<std::string> GetChannels(std::string_view trace_namespace);

// Channel selector expression forms:
//   ".channel[.sub[.sub]]"          -> channel in local_namespace
//   "namespace.channel[.sub[.sub]]" -> channel in explicit namespace
// Selector patterns (for example "*.net" or "{alpha,beta}.*") are accepted by
// EnableChannels/DisableChannels. Leading-dot selectors in selector APIs
// resolve against local_namespace.
void EnableChannel(std::string_view qualified_channel,
                   std::string_view local_namespace = KTRACE_NAMESPACE);
void EnableChannels(std::string_view selectors_csv,
                    std::string_view local_namespace = KTRACE_NAMESPACE);
bool ShouldTraceChannel(std::string_view qualified_channel,
                        std::string_view local_namespace = KTRACE_NAMESPACE);
void DisableChannel(std::string_view qualified_channel,
                    std::string_view local_namespace = KTRACE_NAMESPACE);
void DisableChannels(std::string_view selectors_csv,
                     std::string_view local_namespace = KTRACE_NAMESPACE);
void ClearEnabledChannels();

namespace detail {

bool ShouldTraceBridge(std::string_view trace_namespace,
                       std::string_view category);
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

#define KTRACE(cat, format_text, ...)                                            \
    do {                                                                          \
        auto&& ktrace_category_expr_ = (cat);                                     \
        if (::ktrace::detail::ShouldTraceBridge(KTRACE_NAMESPACE,                 \
                                                ktrace_category_expr_)) {         \
            ::ktrace::detail::WriteBridge(KTRACE_NAMESPACE,                       \
                                          ktrace_category_expr_,                   \
                                          __FILE__,                                \
                                          __LINE__,                                \
                                          __func__,                                \
                                          fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

#define KTRACE_CHANGED(cat, key_expr, format_text, ...)                      \
    do {                                                                          \
        auto&& ktrace_category_expr_ = (cat);                                     \
        static std::string last_key;                                              \
        static std::mutex last_key_mutex;                                         \
        std::string next_key = (key_expr);                                        \
        bool key_changed = false;                                                 \
        {                                                                         \
            std::lock_guard<std::mutex> lock(last_key_mutex);                     \
            if (next_key != last_key) {                                           \
                last_key = std::move(next_key);                                   \
                key_changed = true;                                                \
            }                                                                     \
        }                                                                         \
        if (key_changed &&                                                         \
            ::ktrace::detail::ShouldTraceBridge(KTRACE_NAMESPACE,                 \
                                                ktrace_category_expr_)) {         \
            ::ktrace::detail::WriteBridge(KTRACE_NAMESPACE,                       \
                                          ktrace_category_expr_,                   \
                                          __FILE__,                                \
                                          __LINE__,                                \
                                          __func__,                                \
                                          fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

} // namespace ktrace

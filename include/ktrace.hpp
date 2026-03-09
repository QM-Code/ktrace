#pragma once

#ifndef KTRACE_NAMESPACE
#error "KTRACE_NAMESPACE must be defined for trace logging."
#endif

#include <kcli.hpp>

#include <cstdint>
#include <mutex>
#include <source_location>
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
void Initialize();
// Builds the inline parser for trace_root (for example "--trace" / "--trace-*").
kcli::InlineParser GetInlineParser(std::string_view trace_root,
                                   std::string_view local_namespace);
inline kcli::InlineParser GetInlineParser(std::string_view trace_root = "trace") {
    return GetInlineParser(trace_root, KTRACE_NAMESPACE);
}
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
// selectors_csv must not be empty after trimming whitespace.
void EnableChannels(std::string_view selectors_csv,
                    std::string_view local_namespace = KTRACE_NAMESPACE);
bool ShouldTraceChannel(std::string_view qualified_channel,
                        std::string_view local_namespace = KTRACE_NAMESPACE);
void DisableChannel(std::string_view qualified_channel,
                    std::string_view local_namespace = KTRACE_NAMESPACE);
// selectors_csv must not be empty after trimming whitespace.
void DisableChannels(std::string_view selectors_csv,
                     std::string_view local_namespace = KTRACE_NAMESPACE);
void ClearEnabledChannels();

namespace detail {

enum class LogSeverity;
struct LogTextWithSource;

bool ShouldTraceBridge(std::string_view trace_namespace,
                       std::string_view channel);
void RegisterChannelBridge(std::string_view trace_namespace,
                           std::string_view channel,
                           ColorId color);
// Internal bridge for KTRACE/KTRACE_CHANGED. Do not call directly.
// Use KTRACE/KTRACE_CHANGED for trace logging.
void TraceChecked(std::string_view trace_namespace,
                  std::string_view channel,
                  std::string_view source_file,
                  int source_line,
                  std::string_view function_name,
                  std::string_view message);
void LogChecked(std::string_view trace_namespace,
                LogSeverity severity,
                std::string_view source_file,
                int source_line,
                std::string_view function_name,
                std::string_view message);

} // namespace detail

namespace detail {

enum class LogSeverity {
    info,
    warning,
    error,
};

struct LogTextWithSource {
    std::string_view text;
    std::source_location source_location;

    constexpr LogTextWithSource(
        const char* format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}

    constexpr LogTextWithSource(
        std::string_view format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}

    LogTextWithSource(
        const std::string& format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}
};

} // namespace detail

inline void RegisterChannel(std::string_view channel) {
    detail::RegisterChannelBridge(KTRACE_NAMESPACE, channel, kDefaultColor);
}

inline void RegisterChannel(std::string_view channel, ColorId color) {
    detail::RegisterChannelBridge(KTRACE_NAMESPACE, channel, color);
}

#define KTRACE(channel, format_text, ...)                                        \
    do {                                                                          \
        auto&& ktrace_channel_expr_ = (channel);                                  \
        if (::ktrace::detail::ShouldTraceBridge(KTRACE_NAMESPACE,                 \
                                                ktrace_channel_expr_)) {          \
            ::ktrace::detail::TraceChecked(KTRACE_NAMESPACE,                      \
                                           ktrace_channel_expr_,                   \
                                           __FILE__,                               \
                                           __LINE__,                               \
                                           __func__,                               \
                                           fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

#define KTRACE_CHANGED(channel, key_expr, format_text, ...)                       \
    do {                                                                          \
        auto&& ktrace_channel_expr_ = (channel);                                  \
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
                                                ktrace_channel_expr_)) {          \
            ::ktrace::detail::TraceChecked(KTRACE_NAMESPACE,                      \
                                           ktrace_channel_expr_,                   \
                                           __FILE__,                               \
                                           __LINE__,                               \
                                           __func__,                               \
                                           fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

namespace log {

template <typename... Args>
inline void Info(detail::LogTextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogChecked(KTRACE_NAMESPACE,
                                 ::ktrace::detail::LogSeverity::info,
                                 format_text.source_location.file_name(),
                                 static_cast<int>(format_text.source_location.line()),
                                 format_text.source_location.function_name(),
                                 fmt::format(
                                     fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

template <typename... Args>
inline void Warn(detail::LogTextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogChecked(KTRACE_NAMESPACE,
                                 ::ktrace::detail::LogSeverity::warning,
                                 format_text.source_location.file_name(),
                                 static_cast<int>(format_text.source_location.line()),
                                 format_text.source_location.function_name(),
                                 fmt::format(
                                     fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

template <typename... Args>
inline void Error(detail::LogTextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogChecked(KTRACE_NAMESPACE,
                                 ::ktrace::detail::LogSeverity::error,
                                 format_text.source_location.file_name(),
                                 static_cast<int>(format_text.source_location.line()),
                                 format_text.source_location.function_name(),
                                 fmt::format(
                                     fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

} // namespace log

} // namespace ktrace

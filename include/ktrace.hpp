#pragma once

#ifndef KTRACE_NAMESPACE
#error "KTRACE_NAMESPACE must be defined for trace logging."
#endif

#include <kcli.hpp>

#include <cstdint>
#include <memory>
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

class Logger;

namespace detail {

struct TraceLoggerData;
struct LoggerData;

enum class LogSeverity {
    info,
    warning,
    error,
};

struct TextWithSource {
    std::string_view text;
    std::source_location source_location;

    constexpr TextWithSource(
        const char* format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}

    constexpr TextWithSource(
        std::string_view format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}

    TextWithSource(
        const std::string& format_text,
        const std::source_location& location = std::source_location::current())
        : text(format_text), source_location(location) {}
};

bool ShouldTraceActive(std::string_view trace_namespace, std::string_view channel);
void TraceActive(std::string_view trace_namespace,
                 std::string_view channel,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message);
void LogActive(std::string_view trace_namespace,
               LogSeverity severity,
               std::string_view source_file,
               int source_line,
               std::string_view function_name,
               std::string_view message);

} // namespace detail

class TraceLogger {
public:
    TraceLogger();

    TraceLogger(const TraceLogger& other);
    TraceLogger& operator=(const TraceLogger& other);
    TraceLogger(TraceLogger&& other) noexcept;
    TraceLogger& operator=(TraceLogger&& other) noexcept;
    ~TraceLogger();

    void addChannel(std::string_view channel, ColorId color = kDefaultColor);

private:
    explicit TraceLogger(std::string_view trace_namespace);

    std::unique_ptr<detail::TraceLoggerData> data_;

    friend class Logger;
};

inline TraceLogger::TraceLogger()
    : TraceLogger(KTRACE_NAMESPACE) {}

class Logger {
public:
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger();

    void addTraceLogger(const TraceLogger& logger);
    void activate();

    void enableChannel(std::string_view qualified_channel,
                       std::string_view local_namespace = KTRACE_NAMESPACE);
    void enableChannels(std::string_view selectors_csv,
                        std::string_view local_namespace = KTRACE_NAMESPACE);
    bool shouldTraceChannel(std::string_view qualified_channel,
                            std::string_view local_namespace = KTRACE_NAMESPACE) const;
    void disableChannel(std::string_view qualified_channel,
                        std::string_view local_namespace = KTRACE_NAMESPACE);
    void disableChannels(std::string_view selectors_csv,
                         std::string_view local_namespace = KTRACE_NAMESPACE);

    void setOutputOptions(const OutputOptions& options);
    OutputOptions getOutputOptions() const;
    std::vector<std::string> getNamespaces() const;
    std::vector<std::string> getChannels(std::string_view trace_namespace) const;

private:
    std::unique_ptr<detail::LoggerData> data_;

    bool shouldTrace(std::string_view trace_namespace, std::string_view channel) const;
    void trace(std::string_view trace_namespace,
               std::string_view channel,
               std::string_view source_file,
               int source_line,
               std::string_view function_name,
               std::string_view message) const;
    void log(detail::LogSeverity severity,
             std::string_view trace_namespace,
             std::string_view source_file,
             int source_line,
             std::string_view function_name,
             std::string_view message) const;

    friend bool detail::ShouldTraceActive(std::string_view trace_namespace, std::string_view channel);
    friend void detail::TraceActive(std::string_view trace_namespace,
                                    std::string_view channel,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name,
                                    std::string_view message);
    friend void detail::LogActive(std::string_view trace_namespace,
                                  detail::LogSeverity severity,
                                  std::string_view source_file,
                                  int source_line,
                                  std::string_view function_name,
                                  std::string_view message);
};

kcli::InlineParser GetInlineParser(std::string_view trace_root,
                                   std::string_view local_namespace);
inline kcli::InlineParser GetInlineParser(std::string_view trace_root = "trace") {
    return GetInlineParser(trace_root, KTRACE_NAMESPACE);
}

#define KTRACE(channel, format_text, ...)                                         \
    do {                                                                          \
        auto&& ktrace_channel_expr_ = (channel);                                  \
        if (::ktrace::detail::ShouldTraceActive(KTRACE_NAMESPACE,                 \
                                                ktrace_channel_expr_)) {          \
            ::ktrace::detail::TraceActive(KTRACE_NAMESPACE,                       \
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
                key_changed = true;                                               \
            }                                                                     \
        }                                                                         \
        if (key_changed &&                                                        \
            ::ktrace::detail::ShouldTraceActive(KTRACE_NAMESPACE,                 \
                                                ktrace_channel_expr_)) {          \
            ::ktrace::detail::TraceActive(KTRACE_NAMESPACE,                       \
                                          ktrace_channel_expr_,                   \
                                          __FILE__,                               \
                                          __LINE__,                               \
                                          __func__,                               \
                                          fmt::format((format_text), ##__VA_ARGS__)); \
        }                                                                         \
    } while (0)

template <typename... Args>
inline void Info(detail::TextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogActive(
        KTRACE_NAMESPACE,
        ::ktrace::detail::LogSeverity::info,
        format_text.source_location.file_name(),
        static_cast<int>(format_text.source_location.line()),
        format_text.source_location.function_name(),
        fmt::format(fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

template <typename... Args>
inline void Warn(detail::TextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogActive(
        KTRACE_NAMESPACE,
        ::ktrace::detail::LogSeverity::warning,
        format_text.source_location.file_name(),
        static_cast<int>(format_text.source_location.line()),
        format_text.source_location.function_name(),
        fmt::format(fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

template <typename... Args>
inline void Error(detail::TextWithSource format_text, Args&&... args) {
    ::ktrace::detail::LogActive(
        KTRACE_NAMESPACE,
        ::ktrace::detail::LogSeverity::error,
        format_text.source_location.file_name(),
        static_cast<int>(format_text.source_location.line()),
        format_text.source_location.function_name(),
        fmt::format(fmt::runtime(format_text.text), std::forward<Args>(args)...));
}

} // namespace ktrace

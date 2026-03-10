#pragma once

#include <kcli.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
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

template <typename>
inline constexpr bool kAlwaysFalse = false;

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

template <typename T>
std::string FormatArgument(T&& value) {
    std::ostringstream stream;
    stream << std::boolalpha;

    if constexpr (requires { stream << std::forward<T>(value); }) {
        stream << std::forward<T>(value);
    } else {
        static_assert(kAlwaysFalse<T>, "ktrace format arguments must support stream insertion");
    }

    if (!stream) {
        throw std::invalid_argument("failed to stringify trace format argument");
    }
    return stream.str();
}

std::string FormatMessagePacked(std::string_view format_text,
                                const std::vector<std::string>& formatted_args);

template <typename... Args>
std::string FormatMessage(std::string_view format_text, Args&&... args) {
    std::vector<std::string> formatted_args;
    formatted_args.reserve(sizeof...(Args));
    if constexpr (sizeof...(Args) > 0) {
        (formatted_args.push_back(FormatArgument(std::forward<Args>(args))), ...);
    }
    return FormatMessagePacked(format_text, formatted_args);
}

} // namespace detail

class TraceLogger {
public:
    explicit TraceLogger(std::string_view trace_namespace);

    TraceLogger(const TraceLogger&) = default;
    TraceLogger& operator=(const TraceLogger&) = default;
    TraceLogger(TraceLogger&&) noexcept = default;
    TraceLogger& operator=(TraceLogger&&) noexcept = default;
    ~TraceLogger() = default;

    void addChannel(std::string_view channel, ColorId color = kDefaultColor);
    const std::string& getNamespace() const;
    bool shouldTraceChannel(std::string_view channel) const;

    template <typename... Args>
    void trace(std::string_view channel, detail::TextWithSource format_text, Args&&... args) const;

    template <typename Key, typename... Args>
    void traceChanged(std::string_view channel,
                      Key&& key_expr,
                      detail::TextWithSource format_text,
                      Args&&... args) const;

    template <typename... Args>
    void info(detail::TextWithSource format_text, Args&&... args) const;

    template <typename... Args>
    void warn(detail::TextWithSource format_text, Args&&... args) const;

    template <typename... Args>
    void error(detail::TextWithSource format_text, Args&&... args) const;

private:
    std::shared_ptr<detail::TraceLoggerData> data_;

    void traceFormatted(std::string_view channel,
                        const std::source_location& source_location,
                        std::string message) const;
    bool updateChangedKey(std::string_view channel,
                          const std::source_location& source_location,
                          std::string key) const;
    void logFormatted(detail::LogSeverity severity,
                      const std::source_location& source_location,
                      std::string message) const;

    friend class Logger;
};

class Logger {
public:
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger() = default;

    void addTraceLogger(const TraceLogger& logger);

    void enableChannel(std::string_view qualified_channel,
                       std::string_view local_namespace = {});
    void enableChannel(const TraceLogger& local_trace_logger,
                       std::string_view qualified_channel);
    void enableChannels(std::string_view selectors_csv,
                        std::string_view local_namespace = {});
    void enableChannels(const TraceLogger& local_trace_logger,
                        std::string_view selectors_csv);
    bool shouldTraceChannel(std::string_view qualified_channel,
                            std::string_view local_namespace = {}) const;
    bool shouldTraceChannel(const TraceLogger& local_trace_logger,
                            std::string_view qualified_channel) const;
    void disableChannel(std::string_view qualified_channel,
                        std::string_view local_namespace = {});
    void disableChannel(const TraceLogger& local_trace_logger,
                        std::string_view qualified_channel);
    void disableChannels(std::string_view selectors_csv,
                         std::string_view local_namespace = {});
    void disableChannels(const TraceLogger& local_trace_logger,
                         std::string_view selectors_csv);

    void setOutputOptions(const OutputOptions& options);
    OutputOptions getOutputOptions() const;
    std::vector<std::string> getNamespaces() const;
    std::vector<std::string> getChannels(std::string_view trace_namespace) const;
    kcli::InlineParser makeInlineParser(const TraceLogger& local_trace_logger,
                                        std::string_view trace_root = "trace");

private:
    std::shared_ptr<detail::LoggerData> data_;
    TraceLogger internal_trace_;

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
};

template <typename... Args>
inline void TraceLogger::trace(std::string_view channel,
                               detail::TextWithSource format_text,
                               Args&&... args) const {
    traceFormatted(channel,
                   format_text.source_location,
                   ::ktrace::detail::FormatMessage(format_text.text,
                                                   std::forward<Args>(args)...));
}

template <typename Key, typename... Args>
inline void TraceLogger::traceChanged(std::string_view channel,
                                      Key&& key_expr,
                                      detail::TextWithSource format_text,
                                      Args&&... args) const {
    if (!updateChangedKey(channel,
                          format_text.source_location,
                          ::ktrace::detail::FormatArgument(std::forward<Key>(key_expr)))) {
        return;
    }

    traceFormatted(channel,
                   format_text.source_location,
                   ::ktrace::detail::FormatMessage(format_text.text,
                                                   std::forward<Args>(args)...));
}

template <typename... Args>
inline void TraceLogger::info(detail::TextWithSource format_text, Args&&... args) const {
    logFormatted(::ktrace::detail::LogSeverity::info,
                 format_text.source_location,
                 ::ktrace::detail::FormatMessage(format_text.text,
                                                 std::forward<Args>(args)...));
}

template <typename... Args>
inline void TraceLogger::warn(detail::TextWithSource format_text, Args&&... args) const {
    logFormatted(::ktrace::detail::LogSeverity::warning,
                 format_text.source_location,
                 ::ktrace::detail::FormatMessage(format_text.text,
                                                 std::forward<Args>(args)...));
}

template <typename... Args>
inline void TraceLogger::error(detail::TextWithSource format_text, Args&&... args) const {
    logFormatted(::ktrace::detail::LogSeverity::error,
                 format_text.source_location,
                 ::ktrace::detail::FormatMessage(format_text.text,
                                                 std::forward<Args>(args)...));
}

inline void Logger::enableChannel(const TraceLogger& local_trace_logger,
                                  std::string_view qualified_channel) {
    enableChannel(qualified_channel, local_trace_logger.getNamespace());
}

inline void Logger::enableChannels(const TraceLogger& local_trace_logger,
                                   std::string_view selectors_csv) {
    enableChannels(selectors_csv, local_trace_logger.getNamespace());
}

inline bool Logger::shouldTraceChannel(const TraceLogger& local_trace_logger,
                                       std::string_view qualified_channel) const {
    return shouldTraceChannel(qualified_channel, local_trace_logger.getNamespace());
}

inline void Logger::disableChannel(const TraceLogger& local_trace_logger,
                                   std::string_view qualified_channel) {
    disableChannel(qualified_channel, local_trace_logger.getNamespace());
}

inline void Logger::disableChannels(const TraceLogger& local_trace_logger,
                                    std::string_view selectors_csv) {
    disableChannels(selectors_csv, local_trace_logger.getNamespace());
}

} // namespace ktrace

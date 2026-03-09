#include <ktrace.hpp>

#include "ktrace.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace {

struct InternalChannelConfig {
    std::string_view channel;
    ktrace::ColorId color;
};

constexpr std::array<InternalChannelConfig, 8> kInternalChannels = {{
    {"api", 6u},
    {"api.channels", ktrace::kDefaultColor},
    {"api.cli", ktrace::kDefaultColor},
    {"api.output", ktrace::kDefaultColor},
    {"selector", 3u},
    {"selector.parse", ktrace::kDefaultColor},
    {"registry", 5u},
    {"registry.query", ktrace::kDefaultColor},
}};

ktrace::TraceLogger makeInternalTraceLogger() {
    ktrace::TraceLogger logger;
    for (const InternalChannelConfig& config : kInternalChannels) {
        logger.addChannel(config.channel, config.color);
    }
    return logger;
}

void emitLine(ktrace::detail::LoggerData& logger_data,
              std::string_view prefix,
              std::string_view message) {
    std::lock_guard<std::mutex> lock(logger_data.output_mutex);
    std::fwrite(prefix.data(), 1, prefix.size(), stdout);
    std::fwrite(" ", 1, 1, stdout);
    std::fwrite(message.data(), 1, message.size(), stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);
}

} // namespace

namespace ktrace::detail {

std::unique_ptr<TraceLoggerData> MakeTraceLoggerData(std::string_view trace_namespace) {
    auto data = std::make_unique<TraceLoggerData>();
    data->trace_namespace = trimWhitespace(std::string(trace_namespace));
    if (!isSelectorIdentifier(data->trace_namespace)) {
        throw std::invalid_argument("invalid trace namespace '" + data->trace_namespace + "'");
    }
    return data;
}

TraceLoggerData CloneTraceLoggerData(const TraceLoggerData& data) {
    return data;
}

std::unique_ptr<LoggerData> MakeLoggerData() {
    return std::make_unique<LoggerData>();
}

} // namespace ktrace::detail

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

TraceLogger::TraceLogger(const TraceLogger& other)
    : data_(std::make_unique<detail::TraceLoggerData>(detail::CloneTraceLoggerData(*other.data_))) {}

TraceLogger::TraceLogger(std::string_view trace_namespace)
    : data_(detail::MakeTraceLoggerData(trace_namespace)) {}

TraceLogger& TraceLogger::operator=(const TraceLogger& other) {
    if (this == &other) {
        return *this;
    }

    *data_ = detail::CloneTraceLoggerData(*other.data_);
    return *this;
}

TraceLogger::TraceLogger(TraceLogger&& other) noexcept = default;

TraceLogger& TraceLogger::operator=(TraceLogger&& other) noexcept = default;

TraceLogger::~TraceLogger() = default;

void TraceLogger::addChannel(std::string_view channel, ColorId color) {
    detail::addChannelSpecOrThrow(*data_, channel, color);
}

Logger::Logger()
    : data_(detail::MakeLoggerData()) {
    addTraceLogger(makeInternalTraceLogger());
}

Logger::~Logger() {
    if (detail::GetActiveLogger() == this) {
        detail::SetActiveLogger(nullptr);
    }
}

void Logger::addTraceLogger(const TraceLogger& logger) {
    detail::mergeTraceLoggerOrThrow(*data_, *logger.data_);
}

void Logger::activate() {
    detail::SetActiveLogger(this);
}

void Logger::setOutputOptions(const OutputOptions& options) {
    detail::setOutputOptions(*data_, options);
    KTRACE("api", "updating output options (enable api.output for details)");
    KTRACE("api.output",
           "set output options: filenames={} line_numbers={} function_names={} timestamps={}",
           getOutputOptions().filenames,
           getOutputOptions().line_numbers,
           getOutputOptions().function_names,
           getOutputOptions().timestamps);
}

OutputOptions Logger::getOutputOptions() const {
    return detail::getOutputOptions(*data_);
}

bool Logger::shouldTrace(std::string_view trace_namespace, std::string_view channel) const {
    return detail::isTraceChannelEnabled(*data_, trace_namespace, channel);
}

void Logger::trace(std::string_view trace_namespace,
                   std::string_view channel,
                   std::string_view source_file,
                   int source_line,
                   std::string_view function_name,
                   std::string_view message) const {
    const auto prefix = detail::buildTraceMessagePrefix(
        *data_, trace_namespace, channel, source_file, source_line, function_name);
    emitLine(*data_, prefix, message);
}

void Logger::log(detail::LogSeverity severity,
                 std::string_view trace_namespace,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message) const {
    const auto prefix = detail::buildLogMessagePrefix(
        *data_, trace_namespace, severity, source_file, source_line, function_name);
    emitLine(*data_, prefix, message);
}

} // namespace ktrace

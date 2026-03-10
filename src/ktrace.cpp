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

void configureInternalTraceLogger(ktrace::TraceLogger& logger) {
    for (const InternalChannelConfig& config : kInternalChannels) {
        logger.addChannel(config.channel, config.color);
    }
}

std::string normalizeChannelOrThrow(const std::string_view channel) {
    const std::string channel_name = ktrace::detail::trimWhitespace(std::string(channel));
    if (!ktrace::detail::isValidChannelPath(channel_name)) {
        throw std::invalid_argument("invalid trace channel '" + channel_name + "'");
    }
    return channel_name;
}

} // namespace

namespace ktrace::detail {

std::shared_ptr<TraceLoggerData> MakeTraceLoggerData(std::string_view trace_namespace) {
    auto data = std::make_shared<TraceLoggerData>();
    data->trace_namespace = trimWhitespace(std::string(trace_namespace));
    if (!isSelectorIdentifier(data->trace_namespace)) {
        throw std::invalid_argument("invalid trace namespace '" + data->trace_namespace + "'");
    }
    return data;
}

std::shared_ptr<LoggerData> MakeLoggerData() {
    return std::make_shared<LoggerData>();
}

void emitLine(LoggerData& logger_data, std::string_view prefix, std::string_view message) {
    std::lock_guard<std::mutex> lock(logger_data.output_mutex);
    std::fwrite(prefix.data(), 1, prefix.size(), stdout);
    std::fwrite(" ", 1, 1, stdout);
    std::fwrite(message.data(), 1, message.size(), stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);
}

std::string FormatMessagePacked(std::string_view format_text,
                                const std::vector<std::string>& formatted_args) {
    std::string out;
    out.reserve(format_text.size());

    std::size_t arg_index = 0;
    for (std::size_t i = 0; i < format_text.size(); ++i) {
        const char ch = format_text[i];
        if (ch == '{') {
            if (i + 1 >= format_text.size()) {
                throw std::invalid_argument("unterminated '{' in trace format string");
            }

            const char next = format_text[i + 1];
            if (next == '{') {
                out.push_back('{');
                ++i;
                continue;
            }
            if (next == '}') {
                if (arg_index >= formatted_args.size()) {
                    throw std::invalid_argument("not enough arguments for trace format string");
                }
                out.append(formatted_args[arg_index++]);
                ++i;
                continue;
            }

            throw std::invalid_argument("unsupported trace format token");
        }

        if (ch == '}') {
            if (i + 1 < format_text.size() && format_text[i + 1] == '}') {
                out.push_back('}');
                ++i;
                continue;
            }

            throw std::invalid_argument("unmatched '}' in trace format string");
        }

        out.push_back(ch);
    }

    if (arg_index != formatted_args.size()) {
        throw std::invalid_argument("too many arguments for trace format string");
    }

    return out;
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

TraceLogger::TraceLogger(std::string_view trace_namespace)
    : data_(detail::MakeTraceLoggerData(trace_namespace)) {}

void TraceLogger::addChannel(std::string_view channel, ColorId color) {
    detail::addChannelSpecOrThrow(*data_, channel, color);
}

const std::string& TraceLogger::getNamespace() const {
    return data_->trace_namespace;
}

bool TraceLogger::shouldTraceChannel(std::string_view channel) const {
    try {
        const std::string channel_name = normalizeChannelOrThrow(channel);
        if (const auto logger = detail::getAttachedLogger(*data_)) {
            return detail::isTraceChannelEnabled(*logger, data_->trace_namespace, channel_name);
        }
    } catch (...) {
        return false;
    }
    return false;
}

void TraceLogger::traceFormatted(std::string_view channel,
                                 const std::source_location& source_location,
                                 std::string message) const {
    const std::string channel_name = normalizeChannelOrThrow(channel);
    const auto logger = detail::getAttachedLogger(*data_);
    if (!logger) {
        return;
    }
    if (!detail::isTraceChannelEnabled(*logger, data_->trace_namespace, channel_name)) {
        return;
    }

    const auto prefix = detail::buildTraceMessagePrefix(
        *logger,
        data_->trace_namespace,
        channel_name,
        source_location.file_name(),
        static_cast<int>(source_location.line()),
        source_location.function_name());
    detail::emitLine(*logger, prefix, message);
}

bool TraceLogger::updateChangedKey(std::string_view channel,
                                   const std::source_location& source_location,
                                   std::string key) const {
    const std::string channel_name = normalizeChannelOrThrow(channel);
    const std::string site_key = detail::makeTraceChangedSiteKey(channel_name, source_location);

    std::lock_guard<std::mutex> lock(data_->changed_keys_mutex);
    std::string& previous = data_->changed_keys[site_key];
    if (key == previous) {
        return false;
    }
    previous = std::move(key);
    return true;
}

void TraceLogger::logFormatted(detail::LogSeverity severity,
                               const std::source_location& source_location,
                               std::string message) const {
    const auto logger = detail::getAttachedLogger(*data_);
    if (!logger) {
        return;
    }

    const auto prefix = detail::buildLogMessagePrefix(
        *logger,
        data_->trace_namespace,
        severity,
        source_location.file_name(),
        static_cast<int>(source_location.line()),
        source_location.function_name());
    detail::emitLine(*logger, prefix, message);
}

Logger::Logger()
    : data_(detail::MakeLoggerData()),
      internal_trace_("ktrace") {
    configureInternalTraceLogger(internal_trace_);
    addTraceLogger(internal_trace_);
}

void Logger::addTraceLogger(const TraceLogger& logger) {
    detail::ensureTraceLoggerCanAttach(*logger.data_, data_);
    detail::mergeTraceLoggerOrThrow(*data_, *logger.data_);
    detail::retainTraceLogger(*data_, logger.data_);
    detail::attachTraceLoggerOrThrow(*logger.data_, data_);
}

void Logger::setOutputOptions(const OutputOptions& options) {
    detail::setOutputOptions(*data_, options);
    internal_trace_.trace("api", "updating output options (enable api.output for details)");
    internal_trace_.trace("api.output",
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
    detail::emitLine(*data_, prefix, message);
}

void Logger::log(detail::LogSeverity severity,
                 std::string_view trace_namespace,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message) const {
    const auto prefix = detail::buildLogMessagePrefix(
        *data_, trace_namespace, severity, source_file, source_line, function_name);
    detail::emitLine(*data_, prefix, message);
}

} // namespace ktrace

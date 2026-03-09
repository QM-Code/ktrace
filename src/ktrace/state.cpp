#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::atomic<ktrace::Logger*> g_active_logger{nullptr};

} // namespace

namespace ktrace::detail {

Logger* GetActiveLogger() {
    return g_active_logger.load(std::memory_order_acquire);
}

Logger& RequireActiveLogger() {
    Logger* logger = GetActiveLogger();
    if (logger == nullptr) {
        throw std::runtime_error("activate a ktrace::Logger before using trace runtime APIs");
    }
    return *logger;
}

void SetActiveLogger(Logger* logger) {
    g_active_logger.store(logger, std::memory_order_release);
}

std::string makeQualifiedChannelKey(std::string_view trace_namespace,
                                    std::string_view channel) {
    const std::string trace_namespace_name = trimWhitespace(std::string(trace_namespace));
    const std::string channel_name = trimWhitespace(std::string(channel));
    if (trace_namespace_name.empty() || channel_name.empty()) {
        return {};
    }

    std::string key;
    key.reserve(trace_namespace_name.size() + channel_name.size() + 1);
    key.append(trace_namespace_name);
    key.push_back('.');
    key.append(channel_name);
    return key;
}

std::string trimWhitespace(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start == value.size()) {
        return {};
    }
    size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

bool isSelectorIdentifierChar(const char c) {
    const auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) != 0 || c == '_' || c == '-';
}

bool isSelectorIdentifier(const std::string_view token) {
    if (token.empty()) {
        return false;
    }
    for (const char c : token) {
        if (!isSelectorIdentifierChar(c)) {
            return false;
        }
    }
    return true;
}

bool isValidChannelPath(const std::string_view channel) {
    if (channel.empty()) {
        return false;
    }
    size_t start = 0;
    int depth = 0;
    while (start <= channel.size()) {
        if (depth >= 3) {
            return false;
        }
        const std::size_t dot = channel.find('.', start);
        const std::string_view token =
            (dot == std::string_view::npos)
                ? channel.substr(start)
                : channel.substr(start, dot - start);
        if (token.empty() || !isSelectorIdentifier(token)) {
            return false;
        }
        ++depth;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return true;
}

int splitChannelPath(std::string_view channel, std::array<std::string_view, 3>& out) {
    if (channel.empty()) {
        return 0;
    }
    size_t start = 0;
    int depth = 0;
    while (start <= channel.size()) {
        if (depth >= 3) {
            return -1;
        }
        const std::size_t dot = channel.find('.', start);
        const std::string_view token =
            (dot == std::string_view::npos)
                ? channel.substr(start)
                : channel.substr(start, dot - start);
        if (token.empty()) {
            return -1;
        }
        out[depth++] = token;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return depth;
}

bool matchesSelectorSegment(const std::string& pattern, const std::string_view value) {
    return pattern == "*" || pattern == value;
}

OutputOptions getOutputOptions(const LoggerData& logger_data) {
    return {
        .filenames = logger_data.filenames_enabled.load(std::memory_order_relaxed),
        .line_numbers = logger_data.line_numbers_enabled.load(std::memory_order_relaxed),
        .function_names = logger_data.function_names_enabled.load(std::memory_order_relaxed),
        .timestamps = logger_data.timestamps_enabled.load(std::memory_order_relaxed),
    };
}

void setOutputOptions(LoggerData& logger_data, const OutputOptions& options) {
    const bool line_numbers_enabled = options.filenames && options.line_numbers;
    const bool function_names_enabled = options.filenames && options.function_names;

    logger_data.filenames_enabled.store(options.filenames, std::memory_order_relaxed);
    logger_data.line_numbers_enabled.store(line_numbers_enabled, std::memory_order_relaxed);
    logger_data.function_names_enabled.store(function_names_enabled, std::memory_order_relaxed);
    logger_data.timestamps_enabled.store(options.timestamps, std::memory_order_relaxed);
}

bool isRegisteredTraceChannel(const LoggerData& logger_data,
                              std::string_view trace_namespace,
                              std::string_view channel) {
    const std::string trace_namespace_name = trimWhitespace(std::string(trace_namespace));
    const std::string channel_name = trimWhitespace(std::string(channel));
    if (!isSelectorIdentifier(trace_namespace_name) || !isValidChannelPath(channel_name)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(logger_data.registry_mutex);
    const auto namespace_it = logger_data.channels_by_namespace.find(trace_namespace_name);
    if (namespace_it == logger_data.channels_by_namespace.end()) {
        return false;
    }

    return std::find(namespace_it->second.begin(), namespace_it->second.end(), channel_name) !=
           namespace_it->second.end();
}

bool isTraceChannelEnabled(const LoggerData& logger_data,
                           std::string_view trace_namespace,
                           std::string_view channel) {
    if (!isValidChannelPath(channel)) {
        return false;
    }

    if (!logger_data.has_enabled_channels.load(std::memory_order_relaxed)) {
        return false;
    }

    if (!isRegisteredTraceChannel(logger_data, trace_namespace, channel)) {
        return false;
    }

    const std::string key = makeQualifiedChannelKey(trace_namespace, channel);
    if (key.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(logger_data.enabled_channels_mutex);
    return logger_data.enabled_channel_keys.find(key) != logger_data.enabled_channel_keys.end();
}

} // namespace ktrace::detail

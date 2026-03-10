#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <source_location>
#include <stdexcept>

namespace {

ktrace::detail::Selector parseQualifiedChannelSelectorOrThrow(
    const std::string_view qualified_channel,
    const std::string_view local_namespace) {
    const std::string qualified = ktrace::detail::trimWhitespace(std::string(qualified_channel));
    const std::size_t dot = qualified.find('.');
    if (dot == std::string::npos) {
        throw std::invalid_argument(
            "invalid channel selector '" + qualified +
            "' (expected namespace.channel or .channel; use .channel for local namespace)");
    }

    std::string trace_namespace;
    if (dot == 0) {
        trace_namespace = ktrace::detail::trimWhitespace(std::string(local_namespace));
    } else {
        trace_namespace = qualified.substr(0, dot);
    }
    const std::string channel = qualified.substr(dot + 1);
    if (!ktrace::detail::isSelectorIdentifier(trace_namespace)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace + "'");
    }
    if (!ktrace::detail::isValidChannelPath(channel)) {
        throw std::invalid_argument("invalid trace channel '" + channel + "'");
    }

    std::array<std::string_view, 3> channel_parts{};
    const int depth = ktrace::detail::splitChannelPath(channel, channel_parts);
    if (depth <= 0) {
        throw std::invalid_argument("invalid trace channel '" + channel + "'");
    }

    ktrace::detail::Selector selector{};
    selector.any_namespace = false;
    selector.trace_namespace = trace_namespace;
    selector.channel_depth = depth;
    selector.include_top_level = false;
    for (int i = 0; i < depth; ++i) {
        selector.channel_tokens[static_cast<std::size_t>(i)] =
            std::string(channel_parts[static_cast<std::size_t>(i)]);
    }
    return selector;
}

struct ExactChannelResolution {
    std::string key;
    std::string trace_namespace;
    std::string channel;
    bool registered = false;
};

ExactChannelResolution resolveExactChannelOrThrow(const ktrace::detail::LoggerData& logger_data,
                                                  std::string_view qualified_channel,
                                                  std::string_view local_namespace) {
    const ktrace::detail::Selector selector =
        parseQualifiedChannelSelectorOrThrow(qualified_channel, local_namespace);

    std::string channel;
    channel.reserve(64);
    for (int i = 0; i < selector.channel_depth; ++i) {
        if (i > 0) {
            channel.push_back('.');
        }
        channel.append(selector.channel_tokens[static_cast<std::size_t>(i)]);
    }

    ExactChannelResolution result;
    result.trace_namespace = selector.trace_namespace;
    result.channel = channel;
    result.key = ktrace::detail::makeQualifiedChannelKey(selector.trace_namespace, channel);
    result.registered =
        ktrace::detail::isRegisteredTraceChannel(logger_data, selector.trace_namespace, channel);
    return result;
}

void enableChannelKeys(ktrace::detail::LoggerData& logger_data,
                       const std::vector<std::string>& channel_keys) {
    if (channel_keys.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(logger_data.enabled_channels_mutex);
    for (const std::string& key : channel_keys) {
        if (!key.empty()) {
            logger_data.enabled_channel_keys.insert(key);
        }
    }
    logger_data.has_enabled_channels.store(
        !logger_data.enabled_channel_keys.empty(), std::memory_order_relaxed);
}

void disableChannelKeys(ktrace::detail::LoggerData& logger_data,
                        const std::vector<std::string>& channel_keys) {
    if (channel_keys.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(logger_data.enabled_channels_mutex);
    for (const std::string& key : channel_keys) {
        logger_data.enabled_channel_keys.erase(key);
    }
    logger_data.has_enabled_channels.store(
        !logger_data.enabled_channel_keys.empty(), std::memory_order_relaxed);
}

} // namespace

namespace ktrace {

void Logger::enableChannel(std::string_view qualified_channel, std::string_view local_namespace) {
    const std::string qualified = detail::trimWhitespace(std::string(qualified_channel));
    const ExactChannelResolution resolution =
        resolveExactChannelOrThrow(*data_, qualified_channel, local_namespace);
    if (!resolution.registered) {
        const std::source_location location = std::source_location::current();
        log(detail::LogSeverity::warning,
            local_namespace,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            detail::FormatMessage("enable ignored channel '{}' because it is not registered",
                                  resolution.key));
        return;
    }

    enableChannelKeys(*data_, {resolution.key});
    internal_trace_.trace("api.channels", "enabled channel '{}'", qualified);
}

void Logger::enableChannels(std::string_view selectors_csv,
                            std::string_view local_namespace) {
    const std::string selector_text = detail::trimWhitespace(std::string(selectors_csv));
    const detail::SelectorResolution resolution =
        detail::resolveSelectorExpressionOrThrow(*data_, selector_text, local_namespace);
    enableChannelKeys(*data_, resolution.channel_keys);
    {
        const std::source_location location = std::source_location::current();
        for (const std::string& selector : resolution.unmatched_selectors) {
            log(detail::LogSeverity::warning,
                local_namespace,
                location.file_name(),
                static_cast<int>(location.line()),
                location.function_name(),
                detail::FormatMessage(
                    "enable ignored channel selector '{}' because it matched no registered channels",
                    selector));
        }
    }

    internal_trace_.trace(
        "api",
        "processing channels (enable api.channels for details): enabled {} channel(s), {} unmatched selector(s)",
        resolution.channel_keys.size(),
        resolution.unmatched_selectors.size());
    internal_trace_.trace("api.channels",
                          "enabled {} channel(s) from '{}' ({} unmatched selector(s))",
                          resolution.channel_keys.size(),
                          selector_text,
                          resolution.unmatched_selectors.size());
}

bool Logger::shouldTraceChannel(std::string_view qualified_channel,
                                std::string_view local_namespace) const {
    try {
        const detail::Selector selector =
            parseQualifiedChannelSelectorOrThrow(qualified_channel, local_namespace);

        std::string channel;
        channel.reserve(64);
        for (int i = 0; i < selector.channel_depth; ++i) {
            if (i > 0) {
                channel.push_back('.');
            }
            channel.append(selector.channel_tokens[static_cast<std::size_t>(i)]);
        }
        return detail::isTraceChannelEnabled(*data_, selector.trace_namespace, channel);
    } catch (...) {
        return false;
    }
}

void Logger::disableChannel(std::string_view qualified_channel, std::string_view local_namespace) {
    const std::string qualified = detail::trimWhitespace(std::string(qualified_channel));
    const ExactChannelResolution resolution =
        resolveExactChannelOrThrow(*data_, qualified_channel, local_namespace);
    if (!resolution.registered) {
        const std::source_location location = std::source_location::current();
        log(detail::LogSeverity::warning,
            local_namespace,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            detail::FormatMessage("disable ignored channel '{}' because it is not registered",
                                  resolution.key));
        return;
    }

    disableChannelKeys(*data_, {resolution.key});
    internal_trace_.trace("api.channels", "disabled channel '{}'", qualified);
}

void Logger::disableChannels(std::string_view selectors_csv,
                             std::string_view local_namespace) {
    const std::string selector_text = detail::trimWhitespace(std::string(selectors_csv));
    const detail::SelectorResolution resolution =
        detail::resolveSelectorExpressionOrThrow(*data_, selector_text, local_namespace);
    disableChannelKeys(*data_, resolution.channel_keys);
    {
        const std::source_location location = std::source_location::current();
        for (const std::string& selector : resolution.unmatched_selectors) {
            log(detail::LogSeverity::warning,
                local_namespace,
                location.file_name(),
                static_cast<int>(location.line()),
                location.function_name(),
                detail::FormatMessage(
                    "disable ignored channel selector '{}' because it matched no registered channels",
                    selector));
        }
    }

    internal_trace_.trace(
        "api",
        "processing channels (enable api.channels for details): disabled {} channel(s), {} unmatched selector(s)",
        resolution.channel_keys.size(),
        resolution.unmatched_selectors.size());
    internal_trace_.trace("api.channels",
                          "disabled {} channel(s) from '{}' ({} unmatched selector(s))",
                          resolution.channel_keys.size(),
                          selector_text,
                          resolution.unmatched_selectors.size());
}

} // namespace ktrace

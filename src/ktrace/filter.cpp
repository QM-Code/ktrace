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
    bool registered = false;
};

ExactChannelResolution resolveExactChannelOrThrow(std::string_view qualified_channel,
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
    result.key = ktrace::detail::makeQualifiedChannelKey(selector.trace_namespace, channel);
    result.registered = ktrace::detail::isRegisteredTraceChannel(selector.trace_namespace, channel);
    return result;
}

void enableChannelKeys(const std::vector<std::string>& channel_keys) {
    if (channel_keys.empty()) {
        return;
    }

    auto& state = ktrace::detail::getTraceState();
    std::lock_guard<std::mutex> lock(state.enabled_channels_mutex);
    for (const std::string& key : channel_keys) {
        if (!key.empty()) {
            state.enabled_channel_keys.insert(key);
        }
    }
    state.has_enabled_channels.store(!state.enabled_channel_keys.empty(), std::memory_order_relaxed);
}

void disableChannelKeys(const std::vector<std::string>& channel_keys) {
    if (channel_keys.empty()) {
        return;
    }

    auto& state = ktrace::detail::getTraceState();
    std::lock_guard<std::mutex> lock(state.enabled_channels_mutex);
    for (const std::string& key : channel_keys) {
        state.enabled_channel_keys.erase(key);
    }
    state.has_enabled_channels.store(!state.enabled_channel_keys.empty(), std::memory_order_relaxed);
}

void logUnmatchedSelectors(const std::vector<std::string>& unmatched_selectors,
                          std::string_view action,
                          std::string_view report_namespace) {
    const std::source_location location = std::source_location::current();
    for (const std::string& selector : unmatched_selectors) {
        ktrace::detail::LogChecked(
            report_namespace,
            ktrace::detail::LogSeverity::warning,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            fmt::format(
                "{} ignored channel selector '{}' because it matched no registered channels",
                action,
                selector));
    }
}

} // namespace

namespace ktrace {

void EnableChannel(std::string_view qualified_channel, std::string_view local_namespace) {
    detail::ensureInternalTraceChannelsRegistered();
    const std::string qualified = detail::trimWhitespace(std::string(qualified_channel));
    const ExactChannelResolution resolution =
        resolveExactChannelOrThrow(qualified_channel, local_namespace);
    if (!resolution.registered) {
        const std::source_location location = std::source_location::current();
        ktrace::detail::LogChecked(
            local_namespace,
            ktrace::detail::LogSeverity::warning,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            fmt::format("enable ignored channel '{}' because it is not registered",
                        resolution.key));
        return;
    }
    enableChannelKeys({resolution.key});
    KTRACE("api.channels", "enabled channel '{}'", qualified);
}

void EnableChannels(std::string_view selectors_csv, std::string_view local_namespace) {
    detail::ensureInternalTraceChannelsRegistered();
    const std::string selector_text = detail::trimWhitespace(std::string(selectors_csv));
    const detail::SelectorResolution resolution =
        detail::resolveSelectorExpressionOrThrow(selector_text, local_namespace);
    enableChannelKeys(resolution.channel_keys);
    logUnmatchedSelectors(resolution.unmatched_selectors, "enable", local_namespace);

    KTRACE("api",
           "processing channels (enable api.channels for details): enabled {} channel(s), {} unmatched selector(s)",
           resolution.channel_keys.size(),
           resolution.unmatched_selectors.size());
    KTRACE("api.channels",
           "enabled {} channel(s) from '{}' ({} unmatched selector(s))",
           resolution.channel_keys.size(),
           selector_text,
           resolution.unmatched_selectors.size());
}

bool ShouldTraceChannel(std::string_view qualified_channel, std::string_view local_namespace) {
    try {
        detail::ensureInternalTraceChannelsRegistered();
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
        return detail::isTraceChannelEnabled(selector.trace_namespace, channel);
    } catch (...) {
        return false;
    }
}

void DisableChannel(std::string_view qualified_channel, std::string_view local_namespace) {
    detail::ensureInternalTraceChannelsRegistered();
    const std::string qualified = detail::trimWhitespace(std::string(qualified_channel));
    const ExactChannelResolution resolution =
        resolveExactChannelOrThrow(qualified_channel, local_namespace);
    if (!resolution.registered) {
        const std::source_location location = std::source_location::current();
        ktrace::detail::LogChecked(
            local_namespace,
            ktrace::detail::LogSeverity::warning,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            fmt::format("disable ignored channel '{}' because it is not registered",
                        resolution.key));
        return;
    }
    disableChannelKeys({resolution.key});
    KTRACE("api.channels", "disabled channel '{}'", qualified);
}

void DisableChannels(std::string_view selectors_csv, std::string_view local_namespace) {
    detail::ensureInternalTraceChannelsRegistered();
    const std::string selector_text = detail::trimWhitespace(std::string(selectors_csv));
    const detail::SelectorResolution resolution =
        detail::resolveSelectorExpressionOrThrow(selector_text, local_namespace);
    disableChannelKeys(resolution.channel_keys);
    logUnmatchedSelectors(resolution.unmatched_selectors, "disable", local_namespace);

    KTRACE("api",
           "processing channels (enable api.channels for details): disabled {} channel(s), {} unmatched selector(s)",
           resolution.channel_keys.size(),
           resolution.unmatched_selectors.size());
    KTRACE("api.channels",
           "disabled {} channel(s) from '{}' ({} unmatched selector(s))",
           resolution.channel_keys.size(),
           selector_text,
           resolution.unmatched_selectors.size());
}

void ClearEnabledChannels() {
    detail::ensureInternalTraceChannelsRegistered();
    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.enabled_channels_mutex);
        state.enabled_channel_keys.clear();
        state.has_enabled_channels.store(false, std::memory_order_relaxed);
    }
    KTRACE("api",
           "processing channels (enable api.channels for details): cleared enabled channels");
    KTRACE("api.channels", "cleared all enabled channels");
}

} // namespace ktrace

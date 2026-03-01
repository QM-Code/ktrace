#include "trace.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace {

bool areSelectorsEqual(const ktrace::detail::Selector& lhs, const ktrace::detail::Selector& rhs) {
    if (lhs.any_namespace != rhs.any_namespace) {
        return false;
    }
    if (lhs.trace_namespace != rhs.trace_namespace) {
        return false;
    }
    if (lhs.channel_depth != rhs.channel_depth) {
        return false;
    }
    if (lhs.include_top_level != rhs.include_top_level) {
        return false;
    }
    for (int i = 0; i < lhs.channel_depth; ++i) {
        if (lhs.channel_tokens[static_cast<std::size_t>(i)] != rhs.channel_tokens[static_cast<std::size_t>(i)]) {
            return false;
        }
    }
    return true;
}

void addSelectorsIfMissing(std::vector<ktrace::detail::Selector>& target,
                           const std::vector<ktrace::detail::Selector>& selectors) {
    for (const ktrace::detail::Selector& selector : selectors) {
        const auto existing = std::find_if(
            target.begin(),
            target.end(),
            [&selector](const ktrace::detail::Selector& candidate) {
                return areSelectorsEqual(candidate, selector);
            });
        if (existing == target.end()) {
            target.push_back(selector);
        }
    }
}

void removeSelectorsIfPresent(std::vector<ktrace::detail::Selector>& target,
                              const std::vector<ktrace::detail::Selector>& selectors) {
    target.erase(
        std::remove_if(
            target.begin(),
            target.end(),
            [&selectors](const ktrace::detail::Selector& candidate) {
                return std::any_of(
                    selectors.begin(),
                    selectors.end(),
                    [&candidate](const ktrace::detail::Selector& selector) {
                        return areSelectorsEqual(candidate, selector);
                    });
            }),
        target.end());
}

std::vector<ktrace::detail::Selector> parseSelectorListOrThrow(const std::string& list) {
    if (list.empty()) {
        return {};
    }

    std::vector<std::string> invalid_tokens;
    const std::vector<ktrace::detail::Selector> selectors =
        ktrace::detail::parseSelectorList(list, invalid_tokens);
    if (!invalid_tokens.empty()) {
        std::ostringstream message;
        message << "Invalid trace selector";
        if (invalid_tokens.size() > 1) {
            message << "s";
        }
        message << ": ";
        for (size_t i = 0; i < invalid_tokens.size(); ++i) {
            if (i > 0) {
                message << ", ";
            }
            message << "'" << invalid_tokens[i] << "'";
        }
        throw std::runtime_error(message.str());
    }
    return selectors;
}

ktrace::detail::Selector parseQualifiedChannelSelectorOrThrow(const std::string_view qualified_channel) {
    const std::string qualified = ktrace::detail::trimWhitespace(std::string(qualified_channel));
    const std::size_t dot = qualified.find('.');
    if (dot == std::string::npos) {
        throw std::invalid_argument(
            "invalid channel selector '" + qualified + "' (expected namespace.channel)");
    }
    const std::string trace_namespace = qualified.substr(0, dot);
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

} // namespace

namespace ktrace {

void EnableChannel(std::string_view qualified_channel) {
    const detail::Selector selector = parseQualifiedChannelSelectorOrThrow(qualified_channel);

    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.selector_mutex);
        addSelectorsIfMissing(state.enabled_selectors, {selector});
        removeSelectorsIfPresent(state.disabled_selectors, {selector});
        state.selector_enabled.store(!state.enabled_selectors.empty(), std::memory_order_relaxed);
    }
}

void EnableChannels(std::string_view selectors_csv) {
    const std::vector<detail::Selector> selectors =
        parseSelectorListOrThrow(std::string(selectors_csv));
    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.selector_mutex);
        addSelectorsIfMissing(state.enabled_selectors, selectors);
        removeSelectorsIfPresent(state.disabled_selectors, selectors);
        state.selector_enabled.store(!state.enabled_selectors.empty(), std::memory_order_relaxed);
    }
}

void DisableChannel(std::string_view qualified_channel) {
    const detail::Selector selector = parseQualifiedChannelSelectorOrThrow(qualified_channel);
    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.selector_mutex);
        addSelectorsIfMissing(state.disabled_selectors, {selector});
    }
}

void DisableChannels(std::string_view selectors_csv) {
    const std::vector<detail::Selector> selectors =
        parseSelectorListOrThrow(std::string(selectors_csv));
    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.selector_mutex);
        addSelectorsIfMissing(state.disabled_selectors, selectors);
    }
}

void ClearEnabledChannels() {
    auto& state = detail::getTraceState();
    {
        std::lock_guard<std::mutex> lock(state.selector_mutex);
        state.enabled_selectors.clear();
        state.disabled_selectors.clear();
        state.selector_enabled.store(false, std::memory_order_relaxed);
    }
}

} // namespace ktrace

namespace ktrace::detail {

bool isTraceChannelEnabled(std::string_view trace_namespace, std::string_view channel) {
    if (channel.empty()) {
        return false;
    }

    auto& state = getTraceState();
    if (!state.selector_enabled.load(std::memory_order_relaxed)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state.selector_mutex);
    for (const Selector& selector : state.disabled_selectors) {
        if (matchesSelector(selector, trace_namespace, channel)) {
            return false;
        }
    }
    for (const Selector& selector : state.enabled_selectors) {
        if (matchesSelector(selector, trace_namespace, channel)) {
            return true;
        }
    }
    return false;
}

} // namespace ktrace::detail

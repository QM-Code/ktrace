#include "trace.hpp"

#include <algorithm>
#include <stdexcept>

namespace ktrace::detail {

std::optional<colors::Id> resolveChannelColor(std::string_view trace_namespace,
                                              std::string_view category) {
    auto& state = getTraceState();
    std::lock_guard<std::mutex> lock(state.registry_mutex);
    const auto ns_it = state.channel_colors_by_namespace.find(std::string(trace_namespace));
    if (ns_it == state.channel_colors_by_namespace.end()) {
        return std::nullopt;
    }

    std::string key(category);
    while (!key.empty()) {
        const auto it = ns_it->second.find(key);
        if (it != ns_it->second.end()) {
            return it->second;
        }
        const std::size_t dot = key.rfind('.');
        if (dot == std::string::npos) {
            break;
        }
        key.resize(dot);
    }
    return std::nullopt;
}

} // namespace ktrace::detail

namespace ktrace {

std::vector<std::string> GetNamespaces() {
    detail::ensureInternalTraceChannelsRegistered();
    std::vector<std::string> namespaces;
    {
        auto& state = detail::getTraceState();
        std::lock_guard<std::mutex> lock(state.registry_mutex);
        namespaces.reserve(state.namespaces.size());
        for (const std::string& trace_namespace : state.namespaces) {
            if (!trace_namespace.empty()) {
                namespaces.push_back(trace_namespace);
            }
        }
    }
    std::sort(namespaces.begin(), namespaces.end());
    namespaces.erase(std::unique(namespaces.begin(), namespaces.end()), namespaces.end());
    KTRACE("registry",
           "querying registry (enable registry.query for details): {} namespace(s)",
           namespaces.size());
    return namespaces;
}

std::vector<std::string> GetChannels(std::string_view trace_namespace) {
    detail::ensureInternalTraceChannelsRegistered();
    const std::string trace_namespace_name = detail::trimWhitespace(std::string(trace_namespace));
    if (!detail::isSelectorIdentifier(trace_namespace_name)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace_name + "'");
    }

    std::vector<std::string> channels;
    {
        auto& state = detail::getTraceState();
        std::lock_guard<std::mutex> lock(state.registry_mutex);
        const auto it = state.channels_by_namespace.find(trace_namespace_name);
        if (it == state.channels_by_namespace.end()) {
            return channels;
        }
        channels = it->second;
    }
    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
    KTRACE("registry.query",
           "returned {} channel(s) for namespace '{}'",
           channels.size(),
           trace_namespace_name);
    return channels;
}

} // namespace ktrace

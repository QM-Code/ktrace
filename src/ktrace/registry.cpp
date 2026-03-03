#include "../ktrace.hpp"

#include <algorithm>
#include <stdexcept>

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

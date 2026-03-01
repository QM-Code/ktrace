#include "private.hpp"

#include <algorithm>
#include <stdexcept>

namespace ktrace::detail {

std::optional<colors::Id> ResolveRegisteredColor(std::string_view traceNamespace,
                                                  std::string_view category) {
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.registryMutex);
    const auto nsIt = state.channelColorsByNamespace.find(std::string(traceNamespace));
    if (nsIt == state.channelColorsByNamespace.end()) {
        return std::nullopt;
    }

    std::string key(category);
    while (!key.empty()) {
        const auto it = nsIt->second.find(key);
        if (it != nsIt->second.end()) {
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
    std::vector<std::string> namespaces;
    {
        auto& state = detail::GetState();
        std::lock_guard<std::mutex> lock(state.registryMutex);
        namespaces.reserve(state.namespaces.size());
        for (const std::string& traceNamespace : state.namespaces) {
            if (!traceNamespace.empty()) {
                namespaces.push_back(traceNamespace);
            }
        }
    }
    std::sort(namespaces.begin(), namespaces.end());
    namespaces.erase(std::unique(namespaces.begin(), namespaces.end()), namespaces.end());
    return namespaces;
}

std::vector<std::string> GetChannels(std::string_view traceNamespace) {
    const std::string traceNamespaceName = detail::TrimCopy(std::string(traceNamespace));
    if (!detail::IsIdentifierToken(traceNamespaceName)) {
        throw std::invalid_argument("invalid trace namespace '" + traceNamespaceName + "'");
    }

    std::vector<std::string> channels;
    {
        auto& state = detail::GetState();
        std::lock_guard<std::mutex> lock(state.registryMutex);
        const auto it = state.channelsByNamespace.find(traceNamespaceName);
        if (it == state.channelsByNamespace.end()) {
            return channels;
        }
        channels = it->second;
    }
    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
    return channels;
}

} // namespace ktrace

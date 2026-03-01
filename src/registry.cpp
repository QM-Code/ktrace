#include "internal.hpp"
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

const char* BuildChannelsHelpForNamespace(std::string_view traceNamespace,
                                          bool includeNamespacePrefix) {
    thread_local std::string help;

    std::vector<std::string> channels;
    {
        auto& state = GetState();
        std::lock_guard<std::mutex> lock(state.registryMutex);
        const auto it = state.channelsByNamespace.find(std::string(traceNamespace));
        if (it == state.channelsByNamespace.end() || it->second.empty()) {
            return nullptr;
        }
        channels = it->second;
    }

    std::unordered_set<std::string> seen;
    help.clear();
    for (const std::string& channel : channels) {
        if (channel.empty()) {
            continue;
        }
        if (!seen.emplace(channel).second) {
            continue;
        }
        help.append("    ");
        if (includeNamespacePrefix) {
            help.append(traceNamespace);
            help.push_back('.');
        }
        help.append(channel);
        help.push_back('\n');
    }

    if (help.empty()) {
        return nullptr;
    }
    help.push_back('\n');
    return help.c_str();
}

const char* BuildAllChannelsHelp() {
    thread_local std::string help;
    std::vector<std::string> channels;

    {
        auto& state = GetState();
        std::lock_guard<std::mutex> lock(state.registryMutex);
        for (const auto& [traceNamespace, namespaceChannels] : state.channelsByNamespace) {
            if (traceNamespace.empty()) {
                continue;
            }
            for (const std::string& channel : namespaceChannels) {
                if (channel.empty()) {
                    continue;
                }
                channels.push_back(traceNamespace + "." + channel);
            }
        }
    }

    if (channels.empty()) {
        return nullptr;
    }

    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());

    help.clear();
    for (const std::string& channel : channels) {
        help.append("    ");
        help.append(channel);
        help.push_back('\n');
    }
    help.push_back('\n');
    return help.c_str();
}

const char* BuildNamespacesHelp() {
    thread_local std::string help;

    std::vector<std::string> namespaces;
    {
        auto& state = GetState();
        std::lock_guard<std::mutex> lock(state.registryMutex);
        namespaces.reserve(state.namespaces.size());
        for (const std::string& traceNamespace : state.namespaces) {
            if (traceNamespace.empty()) {
                continue;
            }
            namespaces.push_back(traceNamespace);
        }
    }

    if (namespaces.empty()) {
        return nullptr;
    }

    std::sort(namespaces.begin(), namespaces.end());
    namespaces.erase(std::unique(namespaces.begin(), namespaces.end()), namespaces.end());

    help.clear();
    for (const std::string& traceNamespace : namespaces) {
        help.append("    ");
        help.append(traceNamespace);
        help.push_back('\n');
    }
    help.push_back('\n');
    return help.c_str();
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

void RegisterTraceNamespace(std::string_view traceNamespace) {
    const std::string traceNamespaceName = detail::TrimCopy(std::string(traceNamespace));
    if (!detail::IsIdentifierToken(traceNamespaceName)) {
        throw std::invalid_argument("invalid trace namespace '" + traceNamespaceName + "'");
    }

    auto& state = detail::GetState();
    std::lock_guard<std::mutex> lock(state.registryMutex);
    state.namespaces.emplace(traceNamespaceName);
}

const char* GetTraceNamespacesHelp() {
    if (const char* help = detail::BuildNamespacesHelp()) {
        return help;
    }
    return "\n";
}

const char* GetDefaultTraceChannelsHelp() {
    if (const char* help = detail::BuildAllChannelsHelp()) {
        return help;
    }
    return "\n";
}

const char* GetTraceChannelsHelpForNamespace(std::string_view traceNamespace) {
    return detail::BuildChannelsHelpForNamespace(traceNamespace, true);
}

} // namespace ktrace

#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

ktrace::detail::ChannelSpec* findChannelSpec(ktrace::detail::TraceLoggerData& data,
                                             std::string_view channel_name) {
    for (auto& channel : data.channels) {
        if (channel.name == channel_name) {
            return &channel;
        }
    }
    return nullptr;
}

const ktrace::detail::ChannelSpec* findChannelSpec(const ktrace::detail::TraceLoggerData& data,
                                                   std::string_view channel_name) {
    for (const auto& channel : data.channels) {
        if (channel.name == channel_name) {
            return &channel;
        }
    }
    return nullptr;
}

void mergeColorOrThrow(ktrace::ColorId& existing_color,
                       const ktrace::ColorId new_color,
                       std::string_view trace_namespace,
                       std::string_view channel_name) {
    if (new_color == ktrace::kDefaultColor) {
        return;
    }
    if (new_color > 255u) {
        throw std::invalid_argument("invalid trace color id '" + std::to_string(new_color) + "'");
    }

    if (existing_color == ktrace::kDefaultColor) {
        existing_color = new_color;
        return;
    }

    if (existing_color != new_color) {
        throw std::invalid_argument("conflicting trace color for '" +
                                    std::string(trace_namespace) + "." +
                                    std::string(channel_name) + "'");
    }
}

bool containsChannel(const std::vector<std::string>& channels, std::string_view channel_name) {
    return std::find(channels.begin(), channels.end(), channel_name) != channels.end();
}

} // namespace

namespace ktrace::detail {

void addChannelSpecOrThrow(TraceLoggerData& data,
                           std::string_view channel,
                           ColorId color) {
    const std::string trace_namespace_name = trimWhitespace(data.trace_namespace);
    const std::string channel_name = trimWhitespace(std::string(channel));

    if (!isSelectorIdentifier(trace_namespace_name)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace_name + "'");
    }
    if (!isValidChannelPath(channel_name)) {
        throw std::invalid_argument("invalid trace channel '" + channel_name + "'");
    }

    const std::size_t parent_separator = channel_name.rfind('.');
    if (parent_separator != std::string::npos) {
        const std::string parent_channel = channel_name.substr(0, parent_separator);
        if (findChannelSpec(data, parent_channel) == nullptr) {
            throw std::invalid_argument(
                "cannot add unparented trace channel '" + channel_name +
                "' (missing parent '" + parent_channel + "')");
        }
    }

    if (ChannelSpec* existing = findChannelSpec(data, channel_name)) {
        mergeColorOrThrow(existing->color, color, trace_namespace_name, channel_name);
        return;
    }

    if (color != kDefaultColor && color > 255u) {
        throw std::invalid_argument("invalid trace color id '" + std::to_string(color) + "'");
    }

    data.channels.push_back({channel_name, color});
}

void mergeTraceLoggerOrThrow(LoggerData& logger_data, const TraceLoggerData& trace_logger) {
    const std::string trace_namespace_name = trimWhitespace(trace_logger.trace_namespace);
    if (!isSelectorIdentifier(trace_namespace_name)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace_name + "'");
    }

    std::lock_guard<std::mutex> lock(logger_data.registry_mutex);

    logger_data.namespaces.emplace(trace_namespace_name);
    auto& registered_channels = logger_data.channels_by_namespace[trace_namespace_name];
    auto& registered_colors = logger_data.channel_colors_by_namespace[trace_namespace_name];

    for (const ChannelSpec& channel : trace_logger.channels) {
        if (!isValidChannelPath(channel.name)) {
            throw std::invalid_argument("invalid trace channel '" + channel.name + "'");
        }

        const std::size_t parent_separator = channel.name.rfind('.');
        if (parent_separator != std::string::npos) {
            const std::string parent_channel = channel.name.substr(0, parent_separator);
            if (!containsChannel(registered_channels, parent_channel)) {
                throw std::invalid_argument(
                    "cannot register unparented trace channel '" + channel.name +
                    "' (missing parent '" + parent_channel + "')");
            }
        }

        if (!containsChannel(registered_channels, channel.name)) {
            registered_channels.push_back(channel.name);
        }

        auto color_it = registered_colors.find(channel.name);
        ColorId existing_color =
            (color_it == registered_colors.end()) ? kDefaultColor : color_it->second;
        mergeColorOrThrow(existing_color, channel.color, trace_namespace_name, channel.name);
        if (existing_color != kDefaultColor) {
            registered_colors[channel.name] = existing_color;
        }
    }
}

std::vector<std::string> getNamespaces(const LoggerData& logger_data) {
    std::vector<std::string> namespaces;
    {
        std::lock_guard<std::mutex> lock(logger_data.registry_mutex);
        namespaces.reserve(logger_data.namespaces.size());
        for (const std::string& trace_namespace : logger_data.namespaces) {
            if (!trace_namespace.empty()) {
                namespaces.push_back(trace_namespace);
            }
        }
    }

    std::sort(namespaces.begin(), namespaces.end());
    KTRACE("registry",
           "querying registry (enable registry.query for details): {} namespace(s)",
           namespaces.size());
    return namespaces;
}

std::vector<std::string> getChannels(const LoggerData& logger_data,
                                     std::string_view trace_namespace) {
    const std::string trace_namespace_name = trimWhitespace(std::string(trace_namespace));
    if (!isSelectorIdentifier(trace_namespace_name)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace_name + "'");
    }

    std::vector<std::string> channels;
    {
        std::lock_guard<std::mutex> lock(logger_data.registry_mutex);
        const auto it = logger_data.channels_by_namespace.find(trace_namespace_name);
        if (it == logger_data.channels_by_namespace.end()) {
            return channels;
        }
        channels = it->second;
    }

    std::sort(channels.begin(), channels.end());
    KTRACE("registry.query",
           "returned {} channel(s) for namespace '{}'",
           channels.size(),
           trace_namespace_name);
    return channels;
}

} // namespace ktrace::detail

namespace ktrace {

std::vector<std::string> Logger::getNamespaces() const {
    return detail::getNamespaces(*data_);
}

std::vector<std::string> Logger::getChannels(std::string_view trace_namespace) const {
    return detail::getChannels(*data_, trace_namespace);
}

} // namespace ktrace

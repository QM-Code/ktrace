#include "trace.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

namespace {

constexpr std::string_view kInternalTraceNamespace = "ktrace";
struct InternalChannelConfig {
    std::string_view channel;
    ktrace::ColorId color;
};

constexpr std::array<InternalChannelConfig, 8> kInternalChannels = {{
    {"api", 6u}, // cyan
    {"api.channels", ktrace::kDefaultColor},
    {"api.cli", ktrace::kDefaultColor},
    {"api.output", ktrace::kDefaultColor},
    {"selector", 3u}, // yellow
    {"selector.parse", ktrace::kDefaultColor},
    {"registry", 5u}, // magenta
    {"registry.query", ktrace::kDefaultColor},
}};

spdlog::logger* getLogger(const std::string& name) {
    if (name.empty()) {
        return nullptr;
    }

    auto& state = ktrace::detail::getTraceState();
    std::lock_guard<std::mutex> lock(state.logger_mutex);

    if (auto existing = spdlog::get(name)) {
        return existing.get();
    }

    auto base = spdlog::default_logger();
    if (!base) {
        return nullptr;
    }

    const auto& sinks = base->sinks();
    auto created = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    created->set_level(spdlog::level::trace);
    created->set_pattern("%v");
    spdlog::register_logger(created);
    return created.get();
}

void registerChannelImpl(std::string_view trace_namespace, std::string_view channel) {
    const std::string trace_namespace_name = ktrace::detail::trimWhitespace(std::string(trace_namespace));
    const std::string channel_name = ktrace::detail::trimWhitespace(std::string(channel));

    if (!ktrace::detail::isSelectorIdentifier(trace_namespace_name)) {
        throw std::invalid_argument("invalid trace namespace '" + trace_namespace_name + "'");
    }
    if (!ktrace::detail::isValidChannelPath(channel_name)) {
        throw std::invalid_argument("invalid trace channel '" + channel_name + "'");
    }

    auto& state = ktrace::detail::getTraceState();
    std::lock_guard<std::mutex> lock(state.registry_mutex);
    state.namespaces.emplace(trace_namespace_name);
    auto& registered_channels = state.channels_by_namespace[trace_namespace_name];

    const std::size_t parent_separator = channel_name.rfind('.');
    if (parent_separator != std::string::npos) {
        const std::string parent_channel = channel_name.substr(0, parent_separator);
        if (std::find(registered_channels.begin(), registered_channels.end(), parent_channel) ==
            registered_channels.end()) {
            throw std::invalid_argument(
                "cannot register unparented trace channel '" + channel_name +
                "' (missing parent '" + parent_channel + "')");
        }
    }

    if (std::find(registered_channels.begin(), registered_channels.end(), channel_name) ==
        registered_channels.end()) {
        registered_channels.push_back(channel_name);
    }
}

void registerInternalChannels() {
    for (const InternalChannelConfig& config : kInternalChannels) {
        ktrace::detail::RegisterChannelBridge(
            kInternalTraceNamespace, config.channel, config.color);
    }
}

} // namespace

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

void EnableInternalTrace() {
    detail::ensureInternalTraceChannelsRegistered();
}

} // namespace ktrace

namespace ktrace::detail {

void ensureInternalTraceChannelsRegistered() {
    static std::once_flag init_once;
    std::call_once(init_once, []() {
        registerInternalChannels();
    });
}

void RegisterChannelBridge(std::string_view trace_namespace,
                           std::string_view channel,
                           ColorId color) {
    registerChannelImpl(trace_namespace, channel);
    if (color == kDefaultColor) {
        return;
    }
    if (color > 255u) {
        throw std::invalid_argument("invalid trace color id '" + std::to_string(color) + "'");
    }

    const std::string trace_namespace_name = trimWhitespace(std::string(trace_namespace));
    const std::string channel_name = trimWhitespace(std::string(channel));

    auto& state = getTraceState();
    std::lock_guard<std::mutex> lock(state.registry_mutex);
    state.channel_colors_by_namespace[trace_namespace_name][channel_name] = color;
}

void WriteBridge(std::string_view trace_namespace,
                 std::string_view category,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message) {
    const std::string trace_namespace_name = trimWhitespace(std::string(trace_namespace));
    if (!isTraceChannelEnabled(trace_namespace_name, category)) {
        return;
    }
    const std::string category_name{category};

    if (auto* logger = getLogger(category_name)) {
        const auto prefix = buildTraceMessagePrefix(
            trace_namespace_name, category, source_file, source_line, function_name);
        logger->trace("{} {}", prefix, message);
    }
}

} // namespace ktrace::detail

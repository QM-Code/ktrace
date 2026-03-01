#include "ktrace/trace.hpp"

#include "internal.hpp"
#include "private.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {

spdlog::logger* GetLogger(const std::string& name) {
    if (name.empty()) {
        return nullptr;
    }

    auto& state = ktrace::detail::GetState();
    std::lock_guard<std::mutex> lock(state.loggerMutex);

    if (auto existing = spdlog::get(name)) {
        return existing.get();
    }

    auto base = spdlog::default_logger();
    if (!base) {
        return nullptr;
    }

    const auto& sinks = base->sinks();
    std::string messagePatternCopy;
    {
        std::lock_guard<std::mutex> patternLock(state.patternMutex);
        messagePatternCopy = state.messagePattern.empty() ? std::string("%v") : state.messagePattern;
    }

    auto created = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    created->set_level(spdlog::level::trace);
    created->set_pattern(messagePatternCopy);
    spdlog::register_logger(created);
    state.loggerChannels.emplace(name);
    return created.get();
}

void RegisterChannelImpl(std::string_view traceNamespace, std::string_view channel) {
    const std::string traceNamespaceName = ktrace::detail::TrimCopy(std::string(traceNamespace));
    const std::string channelName = ktrace::detail::TrimCopy(std::string(channel));

    if (!ktrace::detail::IsIdentifierToken(traceNamespaceName)) {
        throw std::invalid_argument("invalid trace namespace '" + traceNamespaceName + "'");
    }
    if (!ktrace::detail::IsValidRegisteredChannel(channelName)) {
        throw std::invalid_argument("invalid trace channel '" + channelName + "'");
    }

    auto& state = ktrace::detail::GetState();
    std::lock_guard<std::mutex> lock(state.registryMutex);
    state.namespaces.emplace(traceNamespaceName);
    auto& registeredChannels = state.channelsByNamespace[traceNamespaceName];

    const std::size_t parentSeparator = channelName.rfind('.');
    if (parentSeparator != std::string::npos) {
        const std::string parentChannel = channelName.substr(0, parentSeparator);
        if (std::find(registeredChannels.begin(), registeredChannels.end(), parentChannel) ==
            registeredChannels.end()) {
            throw std::invalid_argument(
                "cannot register unparented trace channel '" + channelName +
                "' (missing parent '" + parentChannel + "')");
        }
    }

    if (std::find(registeredChannels.begin(), registeredChannels.end(), channelName) ==
        registeredChannels.end()) {
        registeredChannels.push_back(channelName);
    }

    ktrace::detail::RememberLoggerChannel(channelName);
}

} // namespace

namespace ktrace {

ColorId ResolveColor(std::string_view colorName) {
    const std::string token = detail::TrimCopy(std::string(colorName));
    if (token.empty()) {
        throw std::invalid_argument("trace color name must not be empty");
    }

    if (token == "Default" || token == "default") {
        return kDefaultColor;
    }

    const auto& names = detail::ColorNames();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == token) {
            return static_cast<ColorId>(i);
        }
    }

    throw std::invalid_argument("unknown trace color '" + token + "'");
}

} // namespace ktrace

namespace ktrace::detail {

void RegisterChannelBridge(std::string_view traceNamespace,
                           std::string_view channel,
                           ColorId color) {
    RegisterChannelImpl(traceNamespace, channel);
    if (color == kDefaultColor) {
        return;
    }
    if (color > 255u) {
        throw std::invalid_argument("invalid trace color id '" + std::to_string(color) + "'");
    }

    const std::string traceNamespaceName = TrimCopy(std::string(traceNamespace));
    const std::string channelName = TrimCopy(std::string(channel));

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.registryMutex);
    state.channelColorsByNamespace[traceNamespaceName][channelName] = color;
}

void WriteBridge(std::string_view traceNamespace,
                 std::string_view category,
                 std::string_view sourceFile,
                 int sourceLine,
                 std::string_view functionName,
                 std::string_view message) {
    const std::string traceNamespaceName = TrimCopy(std::string(traceNamespace));
    const std::string categoryName{category};
    if (!::ktrace::ShouldTraceChannel(traceNamespaceName, categoryName)) {
        return;
    }

    if (auto* logger = GetLogger(categoryName)) {
        const auto prefix = BuildMessagePrefix(
            traceNamespaceName, category, sourceFile, sourceLine, functionName);
        logger->trace("{} {}", prefix, message);
    }
}

} // namespace ktrace::detail

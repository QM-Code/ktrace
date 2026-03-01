#include "internal.hpp"
#include "private.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

bool SelectorEquals(const ktrace::detail::Selector& lhs, const ktrace::detail::Selector& rhs) {
    if (lhs.anyNamespace != rhs.anyNamespace) {
        return false;
    }
    if (lhs.traceNamespace != rhs.traceNamespace) {
        return false;
    }
    if (lhs.channelDepth != rhs.channelDepth) {
        return false;
    }
    if (lhs.includeTopLevel != rhs.includeTopLevel) {
        return false;
    }
    for (int i = 0; i < lhs.channelDepth; ++i) {
        if (lhs.channelTokens[static_cast<std::size_t>(i)] != rhs.channelTokens[static_cast<std::size_t>(i)]) {
            return false;
        }
    }
    return true;
}

void AddUniqueSelectors(std::vector<ktrace::detail::Selector>& target,
                        const std::vector<ktrace::detail::Selector>& selectors) {
    for (const ktrace::detail::Selector& selector : selectors) {
        const auto existing = std::find_if(
            target.begin(),
            target.end(),
            [&selector](const ktrace::detail::Selector& candidate) {
                return SelectorEquals(candidate, selector);
            });
        if (existing == target.end()) {
            target.push_back(selector);
        }
    }
}

void RemoveSelectors(std::vector<ktrace::detail::Selector>& target,
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
                        return SelectorEquals(candidate, selector);
                    });
            }),
        target.end());
}

std::vector<ktrace::detail::Selector> ParseSelectorsOrThrow(const std::string& list,
                                                            const bool allowLocalSelectors) {
    if (list.empty()) {
        return {};
    }

    if (!allowLocalSelectors) {
        std::stringstream ss(list);
        std::string token;
        while (std::getline(ss, token, ',')) {
            const std::string trimmed = ktrace::detail::TrimCopy(token);
            if (trimmed.rfind(ktrace::detail::kLocalSelectorPrefix, 0) == 0) {
                throw std::invalid_argument("local selectors are not supported in this API");
            }
        }
    }

    std::vector<std::string> invalidTokens;
    const std::vector<ktrace::detail::Selector> selectors =
        ktrace::detail::ParseAndValidateSelectors(list, invalidTokens);
    if (!invalidTokens.empty()) {
        std::ostringstream message;
        message << "Invalid trace selector";
        if (invalidTokens.size() > 1) {
            message << "s";
        }
        message << ": ";
        for (size_t i = 0; i < invalidTokens.size(); ++i) {
            if (i > 0) {
                message << ", ";
            }
            message << "'" << invalidTokens[i] << "'";
        }
        message << "\n\n" << ktrace::detail::SelectorUsage();
        message << "\nAvailable trace channels:\n" << ktrace::GetDefaultTraceChannelsHelp();
        throw std::runtime_error(message.str());
    }
    return selectors;
}

ktrace::detail::Selector ParseQualifiedChannelOrThrow(const std::string_view qualifiedChannel) {
    const std::string qualified = ktrace::detail::TrimCopy(std::string(qualifiedChannel));
    const std::size_t dot = qualified.find('.');
    if (dot == std::string::npos) {
        throw std::invalid_argument(
            "invalid channel selector '" + qualified + "' (expected namespace.channel)");
    }
    const std::string traceNamespace = qualified.substr(0, dot);
    const std::string channel = qualified.substr(dot + 1);
    if (!ktrace::detail::IsIdentifierToken(traceNamespace)) {
        throw std::invalid_argument("invalid trace namespace '" + traceNamespace + "'");
    }
    if (!ktrace::detail::IsValidRegisteredChannel(channel)) {
        throw std::invalid_argument("invalid trace channel '" + channel + "'");
    }

    std::array<std::string_view, 3> channelParts{};
    const int depth = ktrace::detail::SplitCategory(channel, channelParts);
    if (depth <= 0) {
        throw std::invalid_argument("invalid trace channel '" + channel + "'");
    }

    ktrace::detail::Selector selector{};
    selector.anyNamespace = false;
    selector.traceNamespace = traceNamespace;
    selector.channelDepth = depth;
    selector.includeTopLevel = false;
    for (int i = 0; i < depth; ++i) {
        selector.channelTokens[static_cast<std::size_t>(i)] =
            std::string(channelParts[static_cast<std::size_t>(i)]);
    }
    return selector;
}

} // namespace

namespace ktrace::detail {

void EnsureColorSupportInitialized() {
    auto& state = GetState();
    std::call_once(state.colorInitOnce, [&state]() {
#ifndef _WIN32
        state.colorEnabled = (isatty(fileno(stdout)) != 0);
#else
        state.colorEnabled = false;
#endif
    });
}

void AppendTimestamp(std::string& out) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto sinceEpoch = now.time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(sinceEpoch - seconds).count();
    auto secCount = seconds.count();
    if (micros < 0) {
        --secCount;
        micros += 1000000;
    }

    char compactBuffer[48];
    std::snprintf(compactBuffer,
                  sizeof(compactBuffer),
                  "[%lld.%06lld]",
                  static_cast<long long>(secCount),
                  static_cast<long long>(micros));
    out.append(compactBuffer);
}

std::string_view SourceLabel(std::string_view source) {
    constexpr std::string_view kPrefixes[] = {
        "ktrace/src/",
        "bz3/src/",
        "src/",
        "ktrace/include/",
        "bz3/include/",
        "include/"
    };
    for (const std::string_view prefix : kPrefixes) {
        if (source.rfind(prefix, 0) == 0) {
            source.remove_prefix(prefix.size());
            break;
        }
    }

    const auto trimAfterMarker = [&](std::string_view marker) {
        const std::size_t pos = source.rfind(marker);
        if (pos == std::string_view::npos) {
            return false;
        }
        source.remove_prefix(pos + marker.size());
        return true;
    };
    if (!trimAfterMarker("/src/")) {
        trimAfterMarker("\\src\\");
    }

    const std::size_t slash = source.find_last_of("/\\");
    const std::size_t dot = source.find_last_of('.');
    if (dot != std::string_view::npos && (slash == std::string_view::npos || dot > slash)) {
        source = source.substr(0, dot);
    }
    return source;
}

const char* CategoryColor(std::string_view traceNamespace, std::string_view category) {
    EnsureColorSupportInitialized();

    auto& state = GetState();
    if (!state.colorEnabled) {
        return "";
    }

    if (const auto registered = ResolveRegisteredColor(traceNamespace, category)) {
        return AnsiColorCode(*registered);
    }
    return "";
}

std::string BuildMessagePrefix(std::string_view traceNamespace,
                               std::string_view category,
                               std::string_view sourceFile,
                               int sourceLine,
                               std::string_view functionName) {
    const std::string traceNamespaceLabel(traceNamespace);
    const char* color = CategoryColor(traceNamespace, category);
    const bool hasColor = color && color[0] != '\0';

    auto& state = GetState();
    const bool filenamesEnabled = state.filenamesEnabled.load(std::memory_order_relaxed);
    const bool lineNumbersEnabled = state.lineNumbersEnabled.load(std::memory_order_relaxed);
    const bool functionNamesEnabled = state.functionNamesEnabled.load(std::memory_order_relaxed);

    std::string out;
    out.reserve(category.size() + sourceFile.size() + functionName.size() + traceNamespaceLabel.size() + 24);
    if (!traceNamespaceLabel.empty()) {
        if (state.colorEnabled) {
            out.append("\x1b[38;5;250m");
        }
        out.push_back('[');
        out.append(traceNamespaceLabel);
        out.push_back(']');
        if (state.colorEnabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    if (state.timestampsEnabled.load(std::memory_order_relaxed)) {
        if (state.colorEnabled) {
            out.append("\x1b[38;5;245m");
        }
        AppendTimestamp(out);
        if (state.colorEnabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    out.push_back('[');
    if (hasColor) {
        out.append(color);
    }
    out.append(category);
    if (hasColor) {
        out.append("\x1b[0m");
    }
    out.push_back(']');

    if (filenamesEnabled || lineNumbersEnabled) {
        const std::string_view sourceLabel = SourceLabel(sourceFile);
        out.push_back(' ');
        if (state.colorEnabled) {
            out.append("\x1b[38;5;245m");
        }
        out.push_back('[');
        if (filenamesEnabled) {
            out.append(sourceLabel.begin(), sourceLabel.end());
        }
        if (lineNumbersEnabled && sourceLine > 0) {
            if (filenamesEnabled) {
                out.push_back(':');
            } else {
                out.push_back('L');
            }
            out.append(std::to_string(sourceLine));
        }
        out.push_back(']');
        if (state.colorEnabled) {
            out.append("\x1b[0m");
        }
    }

    if (functionNamesEnabled && !functionName.empty()) {
        out.push_back(' ');
        if (state.colorEnabled) {
            out.append("\x1b[38;5;245m");
        }
        out.push_back('[');
        out.append(functionName.begin(), functionName.end());
        out.push_back(']');
        if (state.colorEnabled) {
            out.append("\x1b[0m");
        }
    }
    return out;
}

} // namespace ktrace::detail

namespace ktrace {

void ConfigureLogPatterns(bool timestampLogging) {
    detail::EnsureColorSupportInitialized();

    const char* base = "[%^%l%$] %v";
    const char* named = "[%^%l%$][%n] %v";
    const char* message = "%v";
    const char* baseTs = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";
    const char* namedTs = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$][%n] %v";

    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.patternMutex);
        state.defaultPattern = timestampLogging ? baseTs : base;
        state.namedPattern = timestampLogging ? namedTs : named;
        state.messagePattern = message;
    }
    state.timestampsEnabled.store(timestampLogging, std::memory_order_relaxed);

    spdlog::set_pattern(state.defaultPattern);
    if (auto baseLogger = spdlog::default_logger()) {
        baseLogger->set_pattern(state.defaultPattern);
    }

    for (const std::string& channel : detail::LoggerChannelSnapshot()) {
        if (auto logger = spdlog::get(channel)) {
            logger->set_pattern(state.messagePattern);
        }
    }
}

void ConfigureOutput(bool filenames, bool lineNumbers, bool functionNames) {
    auto& state = detail::GetState();
    state.filenamesEnabled.store(filenames, std::memory_order_relaxed);
    state.lineNumbersEnabled.store(lineNumbers, std::memory_order_relaxed);
    state.functionNamesEnabled.store(functionNames, std::memory_order_relaxed);
}

void EnableTraceChannels(const std::string& list) {
    const std::vector<detail::Selector> validSelectors = ParseSelectorsOrThrow(list, true);
    if (list.empty()) {
        return;
    }

    auto& state = detail::GetState();
    state.selectorEnabled.store(!validSelectors.empty(), std::memory_order_relaxed);

    auto base = spdlog::default_logger();
    if (!base) {
        return;
    }

    std::string messagePatternCopy;
    {
        std::lock_guard<std::mutex> lock(state.patternMutex);
        messagePatternCopy = state.messagePattern.empty() ? std::string("%v") : state.messagePattern;
    }

    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        state.enabledSelectors = validSelectors;
        state.disabledSelectors.clear();
    }

    for (const std::string& channel : detail::LoggerChannelSnapshot()) {
        if (auto logger = spdlog::get(channel)) {
            logger->set_level(spdlog::level::trace);
            logger->set_pattern(messagePatternCopy);
        }
    }
}

void EnableChannel(std::string_view qualified_channel) {
    const std::string selectorText = detail::TrimCopy(std::string(qualified_channel));
    if (selectorText.empty()) {
        spdlog::warn(
            "EnableChannel called with an empty selector. "
            "EnableChannel requires <namespace>.<channel>[.<sub>[.<sub>]] syntax or is effectively a no-op.");
        return;
    }
    if (selectorText.find('*') != std::string::npos ||
        selectorText.rfind(detail::kLocalSelectorPrefix, 0) == 0) {
        spdlog::warn(
            "EnableChannel('{}') is not an exact namespace-qualified channel selector and is a no-op. "
            "EnableChannel requires <namespace>.<channel>[.<sub>[.<sub>]] syntax; use EnableChannels(...) for "
            "wildcard selectors.",
            selectorText);
        return;
    }

    detail::Selector selector{};
    try {
        selector = ParseQualifiedChannelOrThrow(selectorText);
    } catch (const std::invalid_argument&) {
        spdlog::warn(
            "EnableChannel('{}') is not an exact namespace-qualified channel selector and is a no-op. "
            "EnableChannel requires <namespace>.<channel>[.<sub>[.<sub>]] syntax; use EnableChannels(...) for "
            "wildcard selectors.",
            selectorText);
        return;
    }

    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        AddUniqueSelectors(state.enabledSelectors, {selector});
        RemoveSelectors(state.disabledSelectors, {selector});
        state.selectorEnabled.store(!state.enabledSelectors.empty(), std::memory_order_relaxed);
    }
}

void EnableChannels(std::string_view selectors_csv) {
    const std::vector<detail::Selector> selectors =
        ParseSelectorsOrThrow(std::string(selectors_csv), false);
    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        AddUniqueSelectors(state.enabledSelectors, selectors);
        RemoveSelectors(state.disabledSelectors, selectors);
        state.selectorEnabled.store(!state.enabledSelectors.empty(), std::memory_order_relaxed);
    }
}

void DisableChannels(std::string_view selectors_csv);

void DisableChannel(std::string_view qualified_channel) {
    const std::string selectorText = detail::TrimCopy(std::string(qualified_channel));
    if (selectorText.empty() || selectorText == "*") {
        return;
    }
    if (selectorText.find('*') != std::string::npos ||
        selectorText.rfind(detail::kLocalSelectorPrefix, 0) == 0) {
        DisableChannels(selectorText);
        return;
    }

    const detail::Selector selector = ParseQualifiedChannelOrThrow(selectorText);
    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        AddUniqueSelectors(state.disabledSelectors, {selector});
    }
}

void DisableChannels(std::string_view selectors_csv) {
    const std::vector<detail::Selector> selectors =
        ParseSelectorsOrThrow(std::string(selectors_csv), false);
    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        AddUniqueSelectors(state.disabledSelectors, selectors);
    }
}

void ClearEnabledChannels() {
    auto& state = detail::GetState();
    {
        std::lock_guard<std::mutex> lock(state.selectorMutex);
        state.enabledSelectors.clear();
        state.disabledSelectors.clear();
        state.selectorEnabled.store(false, std::memory_order_relaxed);
    }
}

bool ShouldTraceChannel(std::string_view traceNamespace, const std::string& name) {
    if (name.empty()) {
        return false;
    }

    auto& state = detail::GetState();
    if (!state.selectorEnabled.load(std::memory_order_relaxed)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state.selectorMutex);
    for (const detail::Selector& selector : state.disabledSelectors) {
        if (detail::SelectorMatches(selector, traceNamespace, name)) {
            return false;
        }
    }
    for (const detail::Selector& selector : state.enabledSelectors) {
        if (detail::SelectorMatches(selector, traceNamespace, name)) {
            return true;
        }
    }
    return false;
}

} // namespace ktrace

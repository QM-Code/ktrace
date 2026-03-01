#pragma once

#include "colors.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ktrace::detail {

struct Selector {
    bool anyNamespace = false;
    std::string traceNamespace{};
    std::array<std::string, 3> channelTokens{};
    int channelDepth = 0;
    bool includeTopLevel = false;
};

struct State {
    std::mutex selectorMutex;
    std::vector<Selector> enabledSelectors;
    std::vector<Selector> disabledSelectors;
    std::atomic<bool> selectorEnabled{false};

    std::mutex patternMutex;
    std::string defaultPattern;
    std::string namedPattern;
    std::string messagePattern;

    std::atomic<bool> filenamesEnabled{false};
    std::atomic<bool> lineNumbersEnabled{false};
    std::atomic<bool> functionNamesEnabled{false};
    std::atomic<bool> timestampsEnabled{false};

    std::once_flag colorInitOnce;
    bool colorEnabled = false;

    std::mutex registryMutex;
    std::unordered_set<std::string> namespaces;
    std::unordered_map<std::string, std::vector<std::string>> channelsByNamespace;
    std::unordered_map<std::string, std::unordered_map<std::string, colors::Id>>
        channelColorsByNamespace;

    std::mutex loggerMutex;
    std::unordered_set<std::string> loggerChannels;
};

State& GetState();

constexpr std::string_view kLocalSelectorPrefix = "@local:";

std::string TrimCopy(const std::string& value);
bool IsIdentifierChar(char c);
bool IsIdentifierToken(std::string_view token);
bool IsValidRegisteredChannel(std::string_view channel);
int SplitCategory(std::string_view category, std::array<std::string_view, 3>& out);
bool SegmentMatches(const std::string& pattern, std::string_view value);

void RememberLoggerChannel(std::string_view channel);
std::vector<std::string> LoggerChannelSnapshot();

const std::vector<std::string>& ColorNames();
const char* AnsiColorCode(colors::Id color);
void EnsureColorSupportInitialized();

bool ParseChannelPattern(std::string_view expression, Selector& selector, std::string& error);
bool ParseSelectorToken(std::string_view rawToken, Selector& selector, std::string& error);
std::vector<Selector> ParseAndValidateSelectors(const std::string& list,
                                                std::vector<std::string>& invalidTokens);
bool SelectorMatches(const Selector& selector,
                     std::string_view trace_namespace,
                     std::string_view category);
std::string SelectorUsage();

std::optional<colors::Id> ResolveRegisteredColor(std::string_view traceNamespace,
                                                  std::string_view category);
const char* BuildChannelsHelpForNamespace(std::string_view traceNamespace,
                                          bool includeNamespacePrefix);
const char* BuildAllChannelsHelp();
const char* BuildNamespacesHelp();

void AppendTimestamp(std::string& out);
std::string_view SourceLabel(std::string_view source);
std::string BuildMessagePrefix(std::string_view trace_namespace,
                               std::string_view category,
                               std::string_view source_file,
                               int source_line,
                               std::string_view function_name);

} // namespace ktrace::detail

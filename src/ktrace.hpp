#pragma once

#include <ktrace.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ktrace::detail {

struct Selector {
    bool any_namespace = false;
    std::string trace_namespace{};
    std::array<std::string, 3> channel_tokens{};
    int channel_depth = 0;
    bool include_top_level = false;
};

struct SelectorResolution {
    std::vector<std::string> channel_keys;
    std::vector<std::string> unmatched_selectors;
};

struct State {
    std::mutex enabled_channels_mutex;
    std::unordered_set<std::string> enabled_channel_keys;
    std::atomic<bool> has_enabled_channels{false};

    std::atomic<bool> requested_filenames{false};
    std::atomic<bool> requested_line_numbers{false};
    std::atomic<bool> requested_function_names{false};
    std::atomic<bool> requested_timestamps{false};

    std::atomic<bool> filenames_enabled{false};
    std::atomic<bool> line_numbers_enabled{false};
    std::atomic<bool> function_names_enabled{false};
    std::atomic<bool> timestamps_enabled{false};

    std::once_flag color_init_once;
    bool color_enabled = false;

    std::mutex registry_mutex;
    std::unordered_set<std::string> namespaces;
    std::unordered_map<std::string, std::vector<std::string>> channels_by_namespace;
    std::unordered_map<std::string, std::unordered_map<std::string, ColorId>>
        channel_colors_by_namespace;

    std::mutex logger_mutex;
};

State& getTraceState();

std::string makeQualifiedChannelKey(std::string_view trace_namespace,
                                    std::string_view channel);
std::string trimWhitespace(const std::string& value);
bool isSelectorIdentifierChar(char c);
bool isSelectorIdentifier(std::string_view token);
bool isValidChannelPath(std::string_view channel);
int splitChannelPath(std::string_view channel, std::array<std::string_view, 3>& out);
bool matchesSelectorSegment(const std::string& pattern, std::string_view value);

bool isRegisteredTraceChannel(std::string_view trace_namespace, std::string_view channel);
const std::array<std::string_view, 256>& colorNames();
const char* ansiColorCode(ColorId color);
void initializeColorSupport();
std::string buildLogMessagePrefix(std::string_view trace_namespace,
                                  LogSeverity severity,
                                  std::string_view source_file,
                                  int source_line,
                                  std::string_view function_name);
OutputOptions getRequestedOutputOptions();

std::vector<Selector> parseSelectorList(const std::string& list,
                                        std::string_view local_namespace,
                                        std::vector<std::string>& invalid_tokens);
SelectorResolution resolveSelectorsToChannelKeys(const std::vector<Selector>& selectors);
SelectorResolution resolveSelectorExpressionOrThrow(std::string_view selectors_csv,
                                                    std::string_view local_namespace);
bool matchesSelector(const Selector& selector,
                     std::string_view trace_namespace,
                     std::string_view channel);
bool isTraceChannelEnabled(std::string_view trace_namespace, std::string_view channel);
void ensureInternalTraceChannelsRegistered();

std::optional<ColorId> resolveChannelColor(std::string_view trace_namespace,
                                           std::string_view channel);
std::string buildTraceMessagePrefix(std::string_view trace_namespace,
                                    std::string_view channel,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name);

} // namespace ktrace::detail

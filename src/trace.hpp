#pragma once

#include "ktrace/trace.hpp"

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

namespace ktrace::colors {

using Id = std::uint16_t;
inline constexpr Id Default = 0xFFFFu;

} // namespace ktrace::colors

namespace ktrace::detail {

struct Selector {
    bool any_namespace = false;
    std::string trace_namespace{};
    std::array<std::string, 3> channel_tokens{};
    int channel_depth = 0;
    bool include_top_level = false;
};

struct State {
    std::mutex selector_mutex;
    std::vector<Selector> enabled_selectors;
    std::vector<Selector> disabled_selectors;
    std::atomic<bool> selector_enabled{false};

    std::atomic<bool> filenames_enabled{false};
    std::atomic<bool> line_numbers_enabled{false};
    std::atomic<bool> function_names_enabled{false};
    std::atomic<bool> timestamps_enabled{false};

    std::once_flag color_init_once;
    bool color_enabled = false;

    std::mutex registry_mutex;
    std::unordered_set<std::string> namespaces;
    std::unordered_map<std::string, std::vector<std::string>> channels_by_namespace;
    std::unordered_map<std::string, std::unordered_map<std::string, colors::Id>>
        channel_colors_by_namespace;

    std::mutex logger_mutex;
};

State& getTraceState();

std::string trimWhitespace(const std::string& value);
bool isSelectorIdentifierChar(char c);
bool isSelectorIdentifier(std::string_view token);
bool isValidChannelPath(std::string_view channel);
int splitChannelPath(std::string_view category, std::array<std::string_view, 3>& out);
bool matchesSelectorSegment(const std::string& pattern, std::string_view value);

const std::array<std::string_view, 256>& colorNames();
const char* ansiColorCode(colors::Id color);
void initializeColorSupport();

bool parseSelectorChannelPattern(std::string_view expression, Selector& selector, std::string& error);
bool parseSelectorExpression(std::string_view raw_token, Selector& selector, std::string& error);
std::vector<Selector> parseSelectorList(const std::string& list,
                                        std::vector<std::string>& invalid_tokens);
bool matchesSelector(const Selector& selector,
                     std::string_view trace_namespace,
                     std::string_view category);
bool isTraceChannelEnabled(std::string_view trace_namespace, std::string_view channel);
void ensureInternalTraceChannelsRegistered();

std::optional<colors::Id> resolveChannelColor(std::string_view trace_namespace,
                                              std::string_view category);
void appendCompactTimestamp(std::string& out);
std::string_view formatSourceLabel(std::string_view source_path);
std::string buildTraceMessagePrefix(std::string_view trace_namespace,
                                    std::string_view category,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name);

} // namespace ktrace::detail

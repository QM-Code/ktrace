#pragma once

#include <ktrace.hpp>

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

struct ChannelSpec {
    std::string name{};
    ColorId color = kDefaultColor;
};

struct TraceLoggerData {
    std::string trace_namespace{};
    std::vector<ChannelSpec> channels{};
};

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

struct LoggerData {
    mutable std::mutex enabled_channels_mutex;
    std::unordered_set<std::string> enabled_channel_keys;
    std::atomic<bool> has_enabled_channels{false};
    mutable std::mutex output_mutex;

    std::atomic<bool> filenames_enabled{false};
    std::atomic<bool> line_numbers_enabled{false};
    std::atomic<bool> function_names_enabled{false};
    std::atomic<bool> timestamps_enabled{false};

    mutable std::mutex registry_mutex;
    std::unordered_set<std::string> namespaces;
    std::unordered_map<std::string, std::vector<std::string>> channels_by_namespace;
    std::unordered_map<std::string, std::unordered_map<std::string, ColorId>>
        channel_colors_by_namespace;
};

std::unique_ptr<TraceLoggerData> MakeTraceLoggerData(std::string_view trace_namespace);
TraceLoggerData CloneTraceLoggerData(const TraceLoggerData& data);
std::unique_ptr<LoggerData> MakeLoggerData();

Logger* GetActiveLogger();
Logger& RequireActiveLogger();
void SetActiveLogger(Logger* logger);

std::string makeQualifiedChannelKey(std::string_view trace_namespace,
                                    std::string_view channel);
std::string trimWhitespace(const std::string& value);
bool isSelectorIdentifierChar(char c);
bool isSelectorIdentifier(std::string_view token);
bool isValidChannelPath(std::string_view channel);
int splitChannelPath(std::string_view channel, std::array<std::string_view, 3>& out);
bool matchesSelectorSegment(const std::string& pattern, std::string_view value);

void addChannelSpecOrThrow(TraceLoggerData& data,
                           std::string_view channel,
                           ColorId color);
void mergeTraceLoggerOrThrow(LoggerData& logger_data, const TraceLoggerData& trace_logger);

std::vector<std::string> getNamespaces(const LoggerData& logger_data);
std::vector<std::string> getChannels(const LoggerData& logger_data,
                                     std::string_view trace_namespace);

OutputOptions getOutputOptions(const LoggerData& logger_data);
void setOutputOptions(LoggerData& logger_data, const OutputOptions& options);

std::vector<Selector> parseSelectorList(const std::string& list,
                                        std::string_view local_namespace,
                                        std::vector<std::string>& invalid_tokens);
SelectorResolution resolveSelectorsToChannelKeys(const LoggerData& logger_data,
                                                 const std::vector<Selector>& selectors);
SelectorResolution resolveSelectorExpressionOrThrow(const LoggerData& logger_data,
                                                    std::string_view selectors_csv,
                                                    std::string_view local_namespace);
bool matchesSelector(const Selector& selector,
                     std::string_view trace_namespace,
                     std::string_view channel);

bool isRegisteredTraceChannel(const LoggerData& logger_data,
                              std::string_view trace_namespace,
                              std::string_view channel);
bool isTraceChannelEnabled(const LoggerData& logger_data,
                           std::string_view trace_namespace,
                           std::string_view channel);
std::optional<ColorId> resolveChannelColor(const LoggerData& logger_data,
                                           std::string_view trace_namespace,
                                           std::string_view channel);

const std::array<std::string_view, 256>& colorNames();
const char* ansiColorCode(ColorId color);
void initializeColorSupport();
std::string buildTraceMessagePrefix(const LoggerData& logger_data,
                                    std::string_view trace_namespace,
                                    std::string_view channel,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name);
std::string buildLogMessagePrefix(const LoggerData& logger_data,
                                  std::string_view trace_namespace,
                                  LogSeverity severity,
                                  std::string_view source_file,
                                  int source_line,
                                  std::string_view function_name);

} // namespace ktrace::detail

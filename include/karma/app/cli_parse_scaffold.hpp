#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace karma::app {

struct CliCommonState {
    bool trace_explicit = false;
    std::string trace_channels;
    bool timestamp_logging = false;
    std::string language;
    bool language_explicit = false;
    std::string data_dir;
    bool data_dir_explicit = false;
    std::string user_config_path;
    bool user_config_explicit = false;
    bool strict_config = true;
};

struct CliConsumeResult {
    bool consumed = false;
    bool help_requested = false;
    std::string error;
};

enum class CliRegisteredOptionKind {
    Flag,
    String,
    UInt16
};

struct CliRegisteredOption {
    std::string short_name{};
    std::string long_name{};
    std::string value_name{};
    std::string help{};
    CliRegisteredOptionKind kind = CliRegisteredOptionKind::Flag;
    bool allow_equals_form = true;
    std::function<void()> on_flag{};
    std::function<void(const std::string&)> on_string{};
    std::function<void(uint16_t)> on_uint16{};
};

CliRegisteredOption DefineFlagOption(std::string short_name,
                                     std::string long_name,
                                     std::string help,
                                     std::function<void()> on_flag);
CliRegisteredOption DefineStringOption(std::string short_name,
                                       std::string long_name,
                                       std::string value_name,
                                       std::string help,
                                       std::function<void(const std::string&)> on_string,
                                       bool allow_equals_form = true);
CliRegisteredOption DefineUInt16Option(std::string short_name,
                                       std::string long_name,
                                       std::string value_name,
                                       std::string help,
                                       std::function<void(uint16_t)> on_uint16,
                                       bool allow_equals_form = true);

bool StartsWith(std::string_view value, std::string_view prefix);
std::string ValueAfterEquals(const std::string& arg, std::string_view prefix);
std::optional<std::string> RequireValue(const std::string& option,
                                        int& index,
                                        int argc,
                                        char** argv,
                                        std::string* out_error);
std::optional<uint16_t> ParseUInt16Port(const std::string& value, std::string* out_error);
std::optional<bool> ParseBoolOption(const std::string& value, std::string* out_error);
std::optional<std::string> ParseChoiceLower(const std::string& raw_value,
                                            const std::vector<std::string>& allowed_values);
std::string JoinChoices(const std::vector<std::string>& values);
void RequireTraceList(int argc, char** argv);

CliConsumeResult ConsumeCommonCliOption(const std::string& arg,
                                        int& index,
                                        int argc,
                                        char** argv,
                                        CliCommonState& state);
CliConsumeResult ConsumePhysicsBackendCliOption(const std::string& arg,
                                                int& index,
                                                int argc,
                                                char** argv,
                                                std::string& backend_out,
                                                bool& explicit_out);
CliConsumeResult ConsumeAudioBackendCliOption(const std::string& arg,
                                              int& index,
                                              int argc,
                                              char** argv,
                                              std::string& backend_out,
                                              bool& explicit_out);

bool ShouldExposePhysicsBackendCliOption();
bool ShouldExposeAudioBackendCliOption();

void AppendCommonCliHelp(std::ostream& out, bool include_user_config_option = true);
void AppendCoreBackendCliHelp(std::ostream& out);
CliConsumeResult ConsumeRegisteredCliOption(const std::string& arg,
                                            int& index,
                                            int argc,
                                            char** argv,
                                            const std::vector<CliRegisteredOption>& options);
void AppendRegisteredCliHelp(std::ostream& out, const std::vector<CliRegisteredOption>& options);

} // namespace karma::app

#include "karma/cli/shared/parse.hpp"

#include "karma/audio/backend.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/physics/backend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <utility>

namespace karma::cli::shared {
namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> PhysicsBackendChoices(bool include_auto) {
    std::vector<std::string> choices{};
    const auto compiled = physics_backend::CompiledBackends();
    if (include_auto && compiled.size() > 1) {
        choices.emplace_back("auto");
    }
    for (const auto backend : compiled) {
        choices.emplace_back(physics_backend::BackendKindName(backend));
    }
    return choices;
}

std::vector<std::string> AudioBackendChoices(bool include_auto) {
    std::vector<std::string> choices{};
    const auto compiled = audio_backend::CompiledBackends();
    if (include_auto && compiled.size() > 1) {
        choices.emplace_back("auto");
    }
    for (const auto backend : compiled) {
        choices.emplace_back(audio_backend::BackendKindName(backend));
    }
    return choices;
}

std::string ComposeOptionLabel(const RegisteredOption& option) {
    std::string label{};
    if (!option.short_name.empty()) {
        label += option.short_name;
    }
    if (!option.long_name.empty()) {
        if (!label.empty()) {
            label += ", ";
        }
        label += option.long_name;
    }
    if (option.kind != RegisteredOptionKind::Flag) {
        const std::string value_name = option.value_name.empty() ? "<value>" : option.value_name;
        if (!value_name.empty()) {
            label += " ";
            label += value_name;
        }
    }
    return label;
}

bool MatchesExactOptionName(const std::string& arg, const RegisteredOption& option) {
    return (!option.short_name.empty() && arg == option.short_name)
        || (!option.long_name.empty() && arg == option.long_name);
}

} // namespace

RegisteredOption DefineFlag(std::string short_name,
                                     std::string long_name,
                                     std::string help,
                                     std::function<void()> on_flag) {
    RegisteredOption out{};
    out.short_name = std::move(short_name);
    out.long_name = std::move(long_name);
    out.help = std::move(help);
    out.kind = RegisteredOptionKind::Flag;
    out.on_flag = std::move(on_flag);
    out.allow_equals_form = false;
    return out;
}

RegisteredOption DefineString(std::string short_name,
                                       std::string long_name,
                                       std::string value_name,
                                       std::string help,
                                       std::function<void(const std::string&)> on_string,
                                       bool allow_equals_form) {
    RegisteredOption out{};
    out.short_name = std::move(short_name);
    out.long_name = std::move(long_name);
    out.value_name = std::move(value_name);
    out.help = std::move(help);
    out.kind = RegisteredOptionKind::String;
    out.on_string = std::move(on_string);
    out.allow_equals_form = allow_equals_form;
    return out;
}

RegisteredOption DefineUInt16(std::string short_name,
                                       std::string long_name,
                                       std::string value_name,
                                       std::string help,
                                       std::function<void(uint16_t)> on_uint16,
                                       bool allow_equals_form) {
    RegisteredOption out{};
    out.short_name = std::move(short_name);
    out.long_name = std::move(long_name);
    out.value_name = std::move(value_name);
    out.help = std::move(help);
    out.kind = RegisteredOptionKind::UInt16;
    out.on_uint16 = std::move(on_uint16);
    out.allow_equals_form = allow_equals_form;
    return out;
}

std::string ResolveExecutableName(const char* argv0, std::string_view fallback_name) {
    const std::string fallback =
        fallback_name.empty() ? std::string("app") : std::string(fallback_name);
    if (!argv0 || *argv0 == '\0') {
        return fallback;
    }

    const std::filesystem::path executable_path(argv0);
    const std::string executable_name = executable_path.filename().string();
    if (executable_name.empty() || executable_name == "." || executable_name == "..") {
        return fallback;
    }
    return executable_name;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string ValueAfterEquals(const std::string& arg, std::string_view prefix) {
    return arg.substr(prefix.size());
}

std::optional<std::string> RequireValue(const std::string& option,
                                        int& index,
                                        int argc,
                                        char** argv,
                                        std::string* out_error) {
    if (index + 1 >= argc) {
        if (out_error) {
            *out_error = "Option '" + option + "' requires a value.";
        }
        return std::nullopt;
    }
    return std::string(argv[++index]);
}

std::optional<uint16_t> ParseUInt16Port(const std::string& value, std::string* out_error) {
    try {
        const unsigned long raw = std::stoul(value);
        if (raw > 65535UL) {
            if (out_error) {
                *out_error = "Invalid port '" + value + "'. Expected 0..65535.";
            }
            return std::nullopt;
        }
        return static_cast<uint16_t>(raw);
    } catch (const std::exception&) {
        if (out_error) {
            *out_error = "Invalid port '" + value + "'.";
        }
        return std::nullopt;
    }
}

std::optional<bool> ParseBoolOption(const std::string& value, std::string* out_error) {
    const std::string normalized = ToLower(value);
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    if (out_error) {
        *out_error = "Invalid boolean value '" + value + "'.";
    }
    return std::nullopt;
}

std::optional<std::string> ParseChoiceLower(const std::string& raw_value,
                                            const std::vector<std::string>& allowed_values) {
    const std::string normalized = ToLower(raw_value);
    for (const auto& allowed : allowed_values) {
        if (normalized == allowed) {
            return normalized;
        }
    }
    return std::nullopt;
}

std::string JoinChoices(const std::vector<std::string>& values) {
    std::string joined{};
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            joined += "|";
        }
        joined += values[i];
    }
    return joined;
}

void RequireTraceList(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--trace") {
            const bool has_value = (i + 1 < argc) && argv[i + 1][0] != '-';
            if (!has_value) {
                std::cerr << "Error: --trace requires a comma-separated channel list.\n";
                std::cerr << "\nAvailable trace channels:\n"
                          << karma::common::logging::GetDefaultTraceChannelsHelp();
                std::exit(1);
            }
        }
    }
}

ConsumeResult ConsumeCommonOption(const std::string& arg,
                                        int& index,
                                        int argc,
                                        char** argv,
                                        CommonState& state) {
    ConsumeResult out{};

    if (arg == "-h" || arg == "--help") {
        out.consumed = true;
        out.help_requested = true;
        return out;
    }
    if (arg == "--timestamp-logging") {
        state.timestamp_logging = true;
        out.consumed = true;
        return out;
    }
    if (arg == "--trace") {
        std::string error{};
        auto value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        state.trace_channels = *value;
        state.trace_explicit = true;
        return out;
    }
    if (StartsWith(arg, "--trace=")) {
        state.trace_channels = ValueAfterEquals(arg, "--trace=");
        state.trace_explicit = true;
        out.consumed = true;
        return out;
    }
    if (arg == "--data-dir") {
        std::string error{};
        auto value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        state.data_dir = *value;
        state.data_dir_explicit = true;
        return out;
    }
    if (StartsWith(arg, "--data-dir=")) {
        state.data_dir = ValueAfterEquals(arg, "--data-dir=");
        state.data_dir_explicit = true;
        out.consumed = true;
        return out;
    }
    if (arg == "--language") {
        std::string error{};
        auto value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        state.language = *value;
        state.language_explicit = true;
        return out;
    }
    if (StartsWith(arg, "--language=")) {
        state.language = ValueAfterEquals(arg, "--language=");
        state.language_explicit = true;
        out.consumed = true;
        return out;
    }
    if (arg == "--user-config") {
        std::string error{};
        auto value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        state.user_config_path = *value;
        state.user_config_explicit = true;
        return out;
    }
    if (StartsWith(arg, "--user-config=")) {
        state.user_config_path = ValueAfterEquals(arg, "--user-config=");
        state.user_config_explicit = true;
        out.consumed = true;
        return out;
    }
    if (arg == "--strict-config") {
        state.strict_config = true;
        out.consumed = true;
        return out;
    }
    if (StartsWith(arg, "--strict-config=")) {
        std::string error{};
        auto parsed = ParseBoolOption(ValueAfterEquals(arg, "--strict-config="), &error);
        out.consumed = true;
        if (!parsed) {
            out.error = "Invalid boolean for --strict-config: '" + ValueAfterEquals(arg, "--strict-config=") + "'.";
            return out;
        }
        state.strict_config = *parsed;
        return out;
    }

    return out;
}

ConsumeResult ConsumePhysicsBackendOption(const std::string& arg,
                                                int& index,
                                                int argc,
                                                char** argv,
                                                std::string& backend_out,
                                                bool& explicit_out) {
    ConsumeResult out{};
    if (!ShouldExposePhysicsBackendOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-physics") {
        std::string error{};
        raw_value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (StartsWith(arg, "--backend-physics=")) {
        raw_value = ValueAfterEquals(arg, "--backend-physics=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = PhysicsBackendChoices(true);
    const auto parsed = ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-physics. Expected: "
            + JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

ConsumeResult ConsumeAudioBackendOption(const std::string& arg,
                                              int& index,
                                              int argc,
                                              char** argv,
                                              std::string& backend_out,
                                              bool& explicit_out) {
    ConsumeResult out{};
    if (!ShouldExposeAudioBackendOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-audio") {
        std::string error{};
        raw_value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (StartsWith(arg, "--backend-audio=")) {
        raw_value = ValueAfterEquals(arg, "--backend-audio=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = AudioBackendChoices(true);
    const auto parsed = ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-audio. Expected: "
            + JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

bool ShouldExposePhysicsBackendOption() {
    return physics_backend::CompiledBackends().size() > 1;
}

bool ShouldExposeAudioBackendOption() {
    return audio_backend::CompiledBackends().size() > 1;
}

void AppendCommonHelp(std::ostream& out, bool include_user_config_option) {
    out
        << "  -h, --help                      Show this help message\n"
        << "      --trace <channels>          Enable comma-separated trace channels\n"
        << "      --data-dir <dir>            Data directory override\n"
        << "      --language <code>           Language override (runtime only)\n"
        << "      --strict-config=<bool>      Required-config validation (default: true)\n"
        << "      --timestamp-logging         Enable timestamped log output\n";
    if (include_user_config_option) {
        out << "      --user-config <path>        User config file path override\n";
    }
}

void AppendCoreBackendHelp(std::ostream& out) {
    if (ShouldExposePhysicsBackendOption()) {
        const auto choices = PhysicsBackendChoices(true);
        out << "      --backend-physics <name>    Physics backend override ("
            << JoinChoices(choices) << ")\n";
    }
    if (ShouldExposeAudioBackendOption()) {
        const auto choices = AudioBackendChoices(true);
        out << "      --backend-audio <name>      Audio backend override ("
            << JoinChoices(choices) << ")\n";
    }
}

ConsumeResult ConsumeRegisteredOption(const std::string& arg,
                                            int& index,
                                            int argc,
                                            char** argv,
                                            const std::vector<RegisteredOption>& options) {
    ConsumeResult out{};
    for (const auto& option : options) {
        if (option.short_name.empty() && option.long_name.empty()) {
            continue;
        }

        if (option.kind == RegisteredOptionKind::Flag) {
            if (!MatchesExactOptionName(arg, option)) {
                continue;
            }
            out.consumed = true;
            if (!option.on_flag) {
                out.error = "Internal CLI configuration error for option '" + arg + "'.";
                return out;
            }
            option.on_flag();
            return out;
        }

        std::optional<std::string> raw_value{};
        std::string option_name{};
        if (MatchesExactOptionName(arg, option)) {
            option_name = arg;
            raw_value = RequireValue(arg, index, argc, argv, &out.error);
            out.consumed = true;
            if (!raw_value) {
                return out;
            }
        } else if (option.allow_equals_form
                   && !option.long_name.empty()
                   && StartsWith(arg, option.long_name + "=")) {
            option_name = option.long_name;
            raw_value = ValueAfterEquals(arg, option.long_name + "=");
            out.consumed = true;
        } else {
            continue;
        }

        if (option.kind == RegisteredOptionKind::String) {
            if (!option.on_string) {
                out.error = "Internal CLI configuration error for option '" + option_name + "'.";
                return out;
            }
            option.on_string(*raw_value);
            return out;
        }

        if (option.kind == RegisteredOptionKind::UInt16) {
            if (!option.on_uint16) {
                out.error = "Internal CLI configuration error for option '" + option_name + "'.";
                return out;
            }
            std::string parse_error{};
            const auto parsed = ParseUInt16Port(*raw_value, &parse_error);
            if (!parsed) {
                out.error = parse_error;
                return out;
            }
            option.on_uint16(*parsed);
            return out;
        }
    }
    return out;
}

void AppendRegisteredHelp(std::ostream& out, const std::vector<RegisteredOption>& options) {
    constexpr size_t kColumnWidth = 33;
    for (const auto& option : options) {
        const std::string label = ComposeOptionLabel(option);
        if (label.empty()) {
            continue;
        }
        out << "  " << label;
        if (label.size() < kColumnWidth) {
            out << std::string(kColumnWidth - label.size(), ' ');
        } else {
            out << " ";
        }
        out << option.help << "\n";
    }
}

} // namespace karma::cli::shared

#include "server/cli_options.hpp"

#include "karma/common/logging.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>

namespace bz3::server {

namespace {

void PrintHelp() {
    std::cout
        << "Usage: bz3-server [options]\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help                      Show this help message\n"
        << "  -v, --verbose                   Enable debug-level logging\n"
        << "  -t, --trace <channels>          Enable comma-separated trace channels\n"
        << "  -w, --world <dir>               World directory\n"
        << "  -D, --default-world             Use 'defaultWorld' from server config\n"
        << "  -p, --port <port>               Server port (parsed; not yet wired)\n"
        << "  -d, --data-dir <dir>            Data directory override\n"
        << "  -c, --config <path>             User config file path override\n"
        << "  -C, --community <url>           Community endpoint (parsed; not yet wired)\n"
        << "      --backend-physics <name>    Physics backend override (auto|jolt|physx)\n"
        << "      --backend-audio <name>      Audio backend override (auto|sdl3audio|miniaudio)\n"
        << "      --strict-config=<bool>      Required-config validation (default: true)\n"
        << "  -T, --timestamp-logging         Enable timestamped log output\n";
}

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "Error: " << message << "\n\n";
    PrintHelp();
    std::exit(1);
}

std::string RequireValue(const std::string& option, int& i, int argc, char** argv) {
    if (i + 1 >= argc) {
        Fail("Option '" + option + "' requires a value.");
    }
    return argv[++i];
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string ValueAfterEquals(const std::string& arg, std::string_view prefix) {
    return arg.substr(prefix.size());
}

std::string ToLower(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string ParseChoice(const std::string& option,
                        const std::string& raw_value,
                        std::initializer_list<std::string_view> allowed) {
    const std::string value = ToLower(raw_value);
    for (const auto candidate : allowed) {
        if (value == candidate) {
            return value;
        }
    }

    std::string expected;
    for (const auto candidate : allowed) {
        if (!expected.empty()) {
            expected += "|";
        }
        expected += std::string(candidate);
    }
    Fail("Invalid value '" + raw_value + "' for " + option + ". Expected: " + expected + ".");
}

uint16_t ParsePort(const std::string& value) {
    try {
        const unsigned long raw = std::stoul(value);
        if (raw > 65535UL) {
            Fail("Invalid port '" + value + "'. Expected 0..65535.");
        }
        return static_cast<uint16_t>(raw);
    } catch (const std::exception&) {
        Fail("Invalid port '" + value + "'.");
    }
}

bool ParseBool(const std::string& value, const std::string& optionName) {
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    Fail("Invalid boolean for " + optionName + ": '" + value + "'.");
}

void RequireTraceList(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-t" || arg == "--trace") {
            const bool has_value = (i + 1 < argc) && argv[i + 1][0] != '-';
            if (!has_value) {
                std::cerr << "Error: --trace/-t requires a comma-separated channel list.\n";
                std::cerr << "\nAvailable trace channels:\n"
                          << karma::logging::GetDefaultTraceChannelsHelp();
                std::exit(1);
            }
        }
    }
}

} // namespace

CLIOptions ParseCLIOptions(int argc, char** argv) {
    RequireTraceList(argc, argv);

    CLIOptions opts{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintHelp();
            std::exit(0);
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-T" || arg == "--timestamp-logging") {
            opts.timestamp_logging = true;
        } else if (arg == "-t" || arg == "--trace") {
            opts.trace_channels = RequireValue(arg, i, argc, argv);
            opts.trace_explicit = true;
        } else if (StartsWith(arg, "--trace=")) {
            opts.trace_channels = ValueAfterEquals(arg, "--trace=");
            opts.trace_explicit = true;
        } else if (arg == "-w" || arg == "--world") {
            opts.world_dir = RequireValue(arg, i, argc, argv);
            opts.world_specified = true;
        } else if (StartsWith(arg, "--world=")) {
            opts.world_dir = ValueAfterEquals(arg, "--world=");
            opts.world_specified = true;
        } else if (arg == "-D" || arg == "--default-world") {
            opts.use_default_world = true;
        } else if (arg == "-p" || arg == "--port") {
            opts.host_port = ParsePort(RequireValue(arg, i, argc, argv));
            opts.host_port_explicit = true;
        } else if (StartsWith(arg, "--port=")) {
            opts.host_port = ParsePort(ValueAfterEquals(arg, "--port="));
            opts.host_port_explicit = true;
        } else if (arg == "-d" || arg == "--data-dir") {
            opts.data_dir = RequireValue(arg, i, argc, argv);
            opts.data_dir_explicit = true;
        } else if (StartsWith(arg, "--data-dir=")) {
            opts.data_dir = ValueAfterEquals(arg, "--data-dir=");
            opts.data_dir_explicit = true;
        } else if (arg == "-c" || arg == "--config") {
            opts.user_config_path = RequireValue(arg, i, argc, argv);
            opts.user_config_explicit = true;
        } else if (StartsWith(arg, "--config=")) {
            opts.user_config_path = ValueAfterEquals(arg, "--config=");
            opts.user_config_explicit = true;
        } else if (arg == "-C" || arg == "--community") {
            opts.community = RequireValue(arg, i, argc, argv);
            opts.community_explicit = true;
        } else if (StartsWith(arg, "--community=")) {
            opts.community = ValueAfterEquals(arg, "--community=");
            opts.community_explicit = true;
        } else if (arg == "--backend-physics") {
            opts.backend_physics = ParseChoice(arg,
                                               RequireValue(arg, i, argc, argv),
                                               {"auto", "jolt", "physx"});
            opts.backend_physics_explicit = true;
        } else if (StartsWith(arg, "--backend-physics=")) {
            opts.backend_physics = ParseChoice("--backend-physics",
                                               ValueAfterEquals(arg, "--backend-physics="),
                                               {"auto", "jolt", "physx"});
            opts.backend_physics_explicit = true;
        } else if (arg == "--backend-audio") {
            opts.backend_audio = ParseChoice(arg,
                                             RequireValue(arg, i, argc, argv),
                                             {"auto", "sdl3audio", "miniaudio"});
            opts.backend_audio_explicit = true;
        } else if (StartsWith(arg, "--backend-audio=")) {
            opts.backend_audio = ParseChoice("--backend-audio",
                                             ValueAfterEquals(arg, "--backend-audio="),
                                             {"auto", "sdl3audio", "miniaudio"});
            opts.backend_audio_explicit = true;
        } else if (arg == "--strict-config") {
            opts.strict_config = true;
        } else if (StartsWith(arg, "--strict-config=")) {
            opts.strict_config = ParseBool(ValueAfterEquals(arg, "--strict-config="), "--strict-config");
        } else {
            Fail("Unknown option '" + arg + "'.");
        }
    }

    if (opts.world_specified && opts.use_default_world) {
        Fail("Cannot specify both -w/--world and -D/--default-world.");
    }

    if (opts.trace_explicit && opts.trace_channels.empty()) {
        std::cerr << "Error: --trace/-t requires a comma-separated channel list.\n";
        std::cerr << "\nAvailable trace channels:\n"
                  << karma::logging::GetDefaultTraceChannelsHelp();
        std::exit(1);
    }

    return opts;
}

} // namespace bz3::server

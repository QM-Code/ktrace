#include "client/cli_options.hpp"

#include "karma/common/logging.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace bz3::client {

namespace {

void PrintHelp() {
    std::cout
        << "Usage: bz3 [options]\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help                      Show this help message\n"
        << "  -v, --verbose                   Enable debug-level logging\n"
        << "  -t, --trace <channels>          Enable comma-separated trace channels\n"
        << "  -n, --name <name>               Player name for join request\n"
        << "  -a, --addr <address>            Server address to connect\n"
        << "  -p, --port <port>               Server port to connect\n"
        << "  -d, --data-dir <dir>            Data directory override\n"
        << "  -c, --config <path>             User config file path override\n"
        << "      --language <code>           Language override (applied to config)\n"
        << "      --backend-render <name>     Render backend override (auto|bgfx|diligent)\n"
        << "      --backend-ui <name>         UI backend override (imgui|rmlui)\n"
        << "      --backend-physics <name>    Physics backend override (auto|jolt|physx)\n"
        << "      --backend-audio <name>      Audio backend override (auto|sdl3audio|miniaudio)\n"
        << "      --backend-platform <name>   Platform backend override (sdl3|sdl2|glfw)\n"
        << "      --dev-quick-start           Dev flag (parsed; not yet wired)\n"
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
        } else if (arg == "-n" || arg == "--name") {
            opts.player_name = RequireValue(arg, i, argc, argv);
            opts.name_explicit = true;
        } else if (StartsWith(arg, "--name=")) {
            opts.player_name = ValueAfterEquals(arg, "--name=");
            opts.name_explicit = true;
        } else if (arg == "-a" || arg == "--addr") {
            opts.connect_addr = RequireValue(arg, i, argc, argv);
            opts.addr_explicit = true;
        } else if (StartsWith(arg, "--addr=")) {
            opts.connect_addr = ValueAfterEquals(arg, "--addr=");
            opts.addr_explicit = true;
        } else if (arg == "-p" || arg == "--port") {
            opts.connect_port = ParsePort(RequireValue(arg, i, argc, argv));
            opts.port_explicit = true;
        } else if (StartsWith(arg, "--port=")) {
            opts.connect_port = ParsePort(ValueAfterEquals(arg, "--port="));
            opts.port_explicit = true;
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
        } else if (arg == "--language") {
            opts.language = RequireValue(arg, i, argc, argv);
            opts.language_explicit = true;
        } else if (StartsWith(arg, "--language=")) {
            opts.language = ValueAfterEquals(arg, "--language=");
            opts.language_explicit = true;
        } else if (arg == "--backend-render") {
            opts.backend_render = ParseChoice(arg,
                                              RequireValue(arg, i, argc, argv),
                                              {"auto", "bgfx", "diligent"});
            opts.backend_render_explicit = true;
        } else if (StartsWith(arg, "--backend-render=")) {
            opts.backend_render = ParseChoice("--backend-render",
                                              ValueAfterEquals(arg, "--backend-render="),
                                              {"auto", "bgfx", "diligent"});
            opts.backend_render_explicit = true;
        } else if (arg == "--backend-ui") {
            opts.backend_ui = ParseChoice(arg, RequireValue(arg, i, argc, argv), {"imgui", "rmlui"});
            opts.backend_ui_explicit = true;
        } else if (StartsWith(arg, "--backend-ui=")) {
            opts.backend_ui = ParseChoice("--backend-ui",
                                          ValueAfterEquals(arg, "--backend-ui="),
                                          {"imgui", "rmlui"});
            opts.backend_ui_explicit = true;
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
        } else if (arg == "--backend-platform") {
            opts.backend_platform = ParseChoice(arg,
                                                RequireValue(arg, i, argc, argv),
                                                {"sdl3", "sdl2", "glfw"});
            opts.backend_platform_explicit = true;
        } else if (StartsWith(arg, "--backend-platform=")) {
            opts.backend_platform = ParseChoice("--backend-platform",
                                                ValueAfterEquals(arg, "--backend-platform="),
                                                {"sdl3", "sdl2", "glfw"});
            opts.backend_platform_explicit = true;
        } else if (arg == "--dev-quick-start") {
            opts.dev_quick_start = true;
        } else if (arg == "--strict-config") {
            opts.strict_config = true;
        } else if (StartsWith(arg, "--strict-config=")) {
            opts.strict_config = ParseBool(ValueAfterEquals(arg, "--strict-config="), "--strict-config");
        } else {
            Fail("Unknown option '" + arg + "'.");
        }
    }

    if (opts.trace_explicit && opts.trace_channels.empty()) {
        std::cerr << "Error: --trace/-t requires a comma-separated channel list.\n";
        std::cerr << "\nAvailable trace channels:\n"
                  << karma::logging::GetDefaultTraceChannelsHelp();
        std::exit(1);
    }

    return opts;
}

} // namespace bz3::client

#include "server/cli_options.hpp"

#include "karma/app/cli_parse_scaffold.hpp"
#include "karma/common/logging.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace bz3::server {

namespace {

std::vector<karma::app::CliRegisteredOption> BuildGameCliOptions(CLIOptions& opts) {
    std::vector<karma::app::CliRegisteredOption> options{};
    options.push_back(karma::app::DefineStringOption(
        "-w",
        "--world",
        "<dir>",
        "World directory",
        [&opts](const std::string& value) {
            opts.world_dir = value;
            opts.world_specified = true;
        }));
    options.push_back(karma::app::DefineUInt16Option(
        "-p",
        "--port",
        "<port>",
        "Server port (parsed; not yet wired)",
        [&opts](uint16_t value) {
            opts.host_port = value;
            opts.host_port_explicit = true;
        }));
    options.push_back(karma::app::DefineStringOption(
        "-C",
        "--community",
        "<url>",
        "Community endpoint (parsed; not yet wired)",
        [&opts](const std::string& value) {
            opts.community = value;
            opts.community_explicit = true;
        }));
    return options;
}

void PrintHelp(const std::vector<karma::app::CliRegisteredOption>& game_options) {
    std::cout
        << "Usage: bz3-server [options]\n"
        << "\n"
        << "Options:\n";
    karma::app::AppendCommonCliHelp(std::cout);
    karma::app::AppendCoreBackendCliHelp(std::cout);
    karma::app::AppendRegisteredCliHelp(std::cout, game_options);
}

[[noreturn]] void Fail(const std::string& message,
                       const std::vector<karma::app::CliRegisteredOption>& game_options) {
    std::cerr << "Error: " << message << "\n\n";
    PrintHelp(game_options);
    std::exit(1);
}

} // namespace

CLIOptions ParseCLIOptions(int argc, char** argv) {
    karma::app::RequireTraceList(argc, argv);

    CLIOptions opts{};
    karma::app::CliCommonState common{};
    const auto game_options = BuildGameCliOptions(opts);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        const auto common_result = karma::app::ConsumeCommonCliOption(arg, i, argc, argv, common);
        if (common_result.consumed) {
            if (!common_result.error.empty()) {
                Fail(common_result.error, game_options);
            }
            if (common_result.help_requested) {
                PrintHelp(game_options);
                std::exit(0);
            }
            continue;
        }

        const auto physics_result =
            karma::app::ConsumePhysicsBackendCliOption(arg,
                                                       i,
                                                       argc,
                                                       argv,
                                                       opts.backend_physics,
                                                       opts.backend_physics_explicit);
        if (physics_result.consumed) {
            if (!physics_result.error.empty()) {
                Fail(physics_result.error, game_options);
            }
            continue;
        }

        const auto audio_result =
            karma::app::ConsumeAudioBackendCliOption(arg, i, argc, argv, opts.backend_audio, opts.backend_audio_explicit);
        if (audio_result.consumed) {
            if (!audio_result.error.empty()) {
                Fail(audio_result.error, game_options);
            }
            continue;
        }

        const auto game_result = karma::app::ConsumeRegisteredCliOption(arg, i, argc, argv, game_options);
        if (game_result.consumed) {
            if (!game_result.error.empty()) {
                Fail(game_result.error, game_options);
            }
            continue;
        }

        Fail("Unknown option '" + arg + "'.", game_options);
    }

    opts.verbose = common.verbose;
    opts.trace_explicit = common.trace_explicit;
    opts.trace_channels = common.trace_channels;
    opts.timestamp_logging = common.timestamp_logging;
    opts.data_dir = common.data_dir;
    opts.data_dir_explicit = common.data_dir_explicit;
    opts.user_config_path = common.user_config_path;
    opts.user_config_explicit = common.user_config_explicit;
    opts.strict_config = common.strict_config;

    if (opts.trace_explicit && opts.trace_channels.empty()) {
        std::cerr << "Error: --trace/-t requires a comma-separated channel list.\n";
        std::cerr << "\nAvailable trace channels:\n"
                  << karma::logging::GetDefaultTraceChannelsHelp();
        std::exit(1);
    }

    return opts;
}

} // namespace bz3::server

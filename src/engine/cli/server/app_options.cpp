#include "karma/cli/server/app_options.hpp"

#include "karma/cli/server/runtime_options.hpp"
#include "karma/common/logging/logging.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace karma::cli::server {
namespace {

void PrintHelp(std::string_view app_name,
               const std::vector<shared::RegisteredOption>& extra_options) {
    std::cout
        << "Usage: " << app_name << " [options]\n"
        << "\n"
        << "Options:\n";
    shared::AppendCommonHelp(std::cout, false);
    shared::AppendCoreBackendHelp(std::cout);
    AppendRuntimeHelp(std::cout);
    shared::AppendRegisteredHelp(std::cout, extra_options);
}

[[noreturn]] void Fail(const std::string& message,
                       std::string_view app_name,
                       const std::vector<shared::RegisteredOption>& extra_options) {
    std::cerr << "Error: " << message << "\n\n";
    PrintHelp(app_name, extra_options);
    std::exit(1);
}

} // namespace

AppOptions ParseAppOptions(int argc,
                           char** argv,
                           std::string_view fallback_app_name,
                           const std::vector<shared::RegisteredOption>& extra_options) {
    shared::RequireTraceList(argc, argv);

    AppOptions opts{};
    opts.app_name = shared::ResolveExecutableName((argc > 0 && argv) ? argv[0] : nullptr, fallback_app_name);
    shared::CommonState common{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        const auto common_result = shared::ConsumeCommonOption(arg, i, argc, argv, common);
        if (common_result.consumed) {
            if (!common_result.error.empty()) {
                Fail(common_result.error, opts.app_name, extra_options);
            }
            if (common_result.help_requested) {
                PrintHelp(opts.app_name, extra_options);
                std::exit(0);
            }
            continue;
        }

        const auto physics_result = shared::ConsumePhysicsBackendOption(arg,
                                                                        i,
                                                                        argc,
                                                                        argv,
                                                                        opts.backend_physics,
                                                                        opts.backend_physics_explicit);
        if (physics_result.consumed) {
            if (!physics_result.error.empty()) {
                Fail(physics_result.error, opts.app_name, extra_options);
            }
            continue;
        }

        const auto audio_result =
            shared::ConsumeAudioBackendOption(arg, i, argc, argv, opts.backend_audio, opts.backend_audio_explicit);
        if (audio_result.consumed) {
            if (!audio_result.error.empty()) {
                Fail(audio_result.error, opts.app_name, extra_options);
            }
            continue;
        }

        const auto runtime_result = ConsumeRuntimeOption(arg,
                                                         i,
                                                         argc,
                                                         argv,
                                                         opts.server_config_path,
                                                         opts.server_config_explicit,
                                                         opts.listen_port,
                                                         opts.listen_port_explicit,
                                                         opts.community,
                                                         opts.community_explicit);
        if (runtime_result.consumed) {
            if (!runtime_result.error.empty()) {
                Fail(runtime_result.error, opts.app_name, extra_options);
            }
            continue;
        }

        const auto extra_result = shared::ConsumeRegisteredOption(arg, i, argc, argv, extra_options);
        if (extra_result.consumed) {
            if (!extra_result.error.empty()) {
                Fail(extra_result.error, opts.app_name, extra_options);
            }
            continue;
        }

        Fail("Unknown option '" + arg + "'.", opts.app_name, extra_options);
    }

    opts.trace_explicit = common.trace_explicit;
    opts.trace_channels = common.trace_channels;
    opts.timestamp_logging = common.timestamp_logging;
    opts.data_dir = common.data_dir;
    opts.data_dir_explicit = common.data_dir_explicit;
    opts.user_config_path = common.user_config_path;
    opts.user_config_explicit = common.user_config_explicit;
    opts.strict_config = common.strict_config;

    if (opts.trace_explicit && opts.trace_channels.empty()) {
        std::cerr << "Error: --trace requires a comma-separated channel list.\n";
        std::cerr << "\nAvailable trace channels:\n"
                  << karma::common::logging::GetDefaultTraceChannelsHelp();
        std::exit(1);
    }

    return opts;
}

} // namespace karma::cli::server

#include "client/cli_options.hpp"

#include "karma/common/logging.hpp"

#include <iostream>
#include <string>

namespace bz3::client {

namespace {
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
}

CLIOptions ParseCLIOptions(int argc, char** argv) {
    RequireTraceList(argc, argv);

    CLIOptions opts{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-v") {
            opts.verbose = true;
        } else if (arg == "-t" && i + 1 < argc) {
            opts.trace_channels = argv[++i];
            opts.trace_explicit = true;
        } else if (arg.rfind("--trace=", 0) == 0) {
            opts.trace_channels = arg.substr(8);
            opts.trace_explicit = true;
        }
    }

    return opts;
}

} // namespace bz3::client

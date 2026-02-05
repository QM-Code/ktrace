#pragma once

#include <string>

namespace bz3::client {

struct CLIOptions {
    bool verbose = false;
    bool trace_explicit = false;
    std::string trace_channels;
};

CLIOptions ParseCLIOptions(int argc, char** argv);

} // namespace bz3::client

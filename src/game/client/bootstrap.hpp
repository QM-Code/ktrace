#pragma once

#include "client/cli_options.hpp"

namespace bz3::client {

void ConfigureLogging(const CLIOptions& options);
void ConfigureDataAndConfig(int argc, char** argv);

} // namespace bz3::client

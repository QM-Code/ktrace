#pragma once

#include "karma/app/cli_parse_scaffold.hpp"

#include <ostream>
#include <string>

namespace karma::app {

CliConsumeResult ConsumeRenderBackendCliOption(const std::string& arg,
                                               int& index,
                                               int argc,
                                               char** argv,
                                               std::string& backend_out,
                                               bool& explicit_out);
CliConsumeResult ConsumeUiBackendCliOption(const std::string& arg,
                                           int& index,
                                           int argc,
                                           char** argv,
                                           std::string& backend_out,
                                           bool& explicit_out);
CliConsumeResult ConsumePlatformBackendCliOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out);

void AppendClientBackendCliHelp(std::ostream& out);

} // namespace karma::app

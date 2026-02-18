#pragma once

#include "karma/cli/shared/parse.hpp"

#include <ostream>
#include <string>

namespace karma::cli::client {

shared::ConsumeResult ConsumeRenderBackendOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out);
shared::ConsumeResult ConsumeUiBackendOption(const std::string& arg,
                                             int& index,
                                             int argc,
                                             char** argv,
                                             std::string& backend_out,
                                             bool& explicit_out);
shared::ConsumeResult ConsumeWindowBackendOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out);

void AppendBackendHelp(std::ostream& out);

} // namespace karma::cli::client

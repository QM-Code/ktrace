#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "server/cli_options.hpp"

namespace bz3::server::domain {

struct WorldSessionContext {
    std::string world_name{};
    std::filesystem::path world_dir{};
    std::filesystem::path world_config_path{};
};

std::optional<WorldSessionContext> LoadWorldSessionContext(const CLIOptions& options);

} // namespace bz3::server::domain

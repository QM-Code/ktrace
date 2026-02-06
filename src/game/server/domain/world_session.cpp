#include "server/domain/world_session.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

namespace bz3::server::domain {

namespace {

std::string ResolveWorldDirectory(const CLIOptions& options) {
    if (options.world_specified) {
        return options.world_dir;
    }
    if (options.use_default_world) {
        return karma::config::ReadStringConfig("defaultWorld", "");
    }
    return karma::config::ReadStringConfig("defaultWorld", "");
}

} // namespace

std::optional<WorldSessionContext> LoadWorldSessionContext(const CLIOptions& options) {
    const std::string world_dir = ResolveWorldDirectory(options);
    if (world_dir.empty()) {
        spdlog::error("bz3-server: no world directory specified and 'defaultWorld' is missing.");
        return std::nullopt;
    }

    const std::filesystem::path world_dir_path = karma::data::Resolve(world_dir);
    if (!std::filesystem::is_directory(world_dir_path)) {
        spdlog::error("bz3-server: world directory not found: {}", world_dir_path.string());
        return std::nullopt;
    }

    const std::filesystem::path world_config_path = world_dir_path / "config.json";
    const auto world_config_opt =
        karma::data::LoadJsonFile(world_config_path, "world config", spdlog::level::err);
    if (!world_config_opt || !world_config_opt->is_object()) {
        spdlog::error("bz3-server: failed to load world config object from {}", world_config_path.string());
        return std::nullopt;
    }

    if (!karma::config::ConfigStore::AddRuntimeLayer("world config", *world_config_opt, world_config_path.parent_path())) {
        spdlog::error("bz3-server: failed to add world config runtime layer.");
        return std::nullopt;
    }

    const auto* layer = karma::config::ConfigStore::LayerByLabel("world config");
    if (!layer || !layer->is_object()) {
        spdlog::error("bz3-server: runtime layer lookup failed for world config.");
        return std::nullopt;
    }

    WorldSessionContext context{};
    context.world_dir = world_dir_path;
    context.world_config_path = world_config_path;
    context.world_name =
        karma::config::ReadStringConfig("worldName", world_dir_path.filename().string());

    KARMA_TRACE("engine.server",
                "bz3-server: world '{}' loaded from '{}'",
                context.world_name,
                context.world_dir.string());
    return context;
}

} // namespace bz3::server::domain

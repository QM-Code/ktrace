#include "server/domain/world_session.hpp"

#include "karma/cli/server/runtime_options.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/common/content/archive.hpp"
#include "karma/common/content/manifest.hpp"
#include "karma/common/content/primitives.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace bz3::server::domain {

namespace {

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    return karma::common::content::ComputeWorldPackageHash(bytes);
}

struct WorldManifestSummary {
    std::string content_hash{};
    std::string manifest_hash{};
    std::vector<WorldManifestEntry> entries{};
};

WorldManifestSummary ComputeWorldManifestSummary(const std::filesystem::path& world_dir) {
    const auto summary = karma::common::content::ComputeDirectoryManifestSummary(world_dir);
    if (!summary.has_value()) {
        throw std::runtime_error("Failed to compute world manifest summary for directory: " + world_dir.string());
    }

    WorldManifestSummary converted{};
    converted.content_hash = summary->content_hash;
    converted.manifest_hash = summary->manifest_hash;
    converted.entries.reserve(summary->entries.size());
    for (const auto& entry : summary->entries) {
        converted.entries.push_back(WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

} // namespace

std::optional<WorldSessionContext> LoadWorldSessionContext(
    const karma::cli::server::AppOptions& options) {
    WorldSessionContext context{};
    const std::string app_name = options.app_name.empty() ? std::string("server") : options.app_name;
    context.world_package_enabled = options.server_config_explicit;

    const auto overlay_path = karma::cli::server::ResolveConfigOverlayPath(options.server_config_path,
                                                                         options.server_config_explicit);
    if (overlay_path.has_value()) {
        const std::filesystem::path overlay_base_dir = overlay_path->parent_path();
        context.world_config_path = *overlay_path;
        context.world_dir = overlay_base_dir;
    }

    const std::string default_world_name = context.world_package_enabled && !context.world_dir.empty()
        ? context.world_dir.filename().string()
        : std::string("Default");
    context.world_name =
        karma::common::config::ReadStringConfig("worldName", default_world_name);

    if (context.world_package_enabled) {
        if (!std::filesystem::is_directory(context.world_dir)) {
            spdlog::error("{}: server config overlay base directory not found: {}",
                          app_name,
                          context.world_dir.string());
            return std::nullopt;
        }
        try {
            context.world_package = karma::common::content::BuildWorldArchive(context.world_dir);
        } catch (const std::exception& ex) {
            spdlog::error("{}: failed to package server config base directory '{}': {}",
                          app_name,
                          context.world_dir.string(),
                          ex.what());
            return std::nullopt;
        }

        context.world_package_size = static_cast<uint64_t>(context.world_package.size());
        context.world_package_hash = ComputeWorldPackageHash(context.world_package);
        const WorldManifestSummary manifest = ComputeWorldManifestSummary(context.world_dir);
        context.world_content_hash = manifest.content_hash;
        context.world_manifest_hash = manifest.manifest_hash;
        context.world_manifest = manifest.entries;
        context.world_manifest_file_count = static_cast<uint32_t>(context.world_manifest.size());

        KARMA_TRACE("net.server",
                    "{}: packaged server config overlay '{}' bytes={} hash={} content_hash={} manifest_hash={} files={} dir='{}'",
                    app_name,
                    context.world_name,
                    context.world_package_size,
                    context.world_package_hash,
                    context.world_content_hash.empty() ? "-" : context.world_content_hash,
                    context.world_manifest_hash.empty() ? "-" : context.world_manifest_hash,
                    context.world_manifest_file_count,
                    context.world_dir.string());
    } else {
        KARMA_TRACE("net.server",
                    "{}: bundled default mode '{}' (no world package transfer)",
                    app_name,
                    context.world_name);
    }

    context.world_id = karma::common::config::ReadStringConfig("worldId", context.world_name);
    if (context.world_package_enabled) {
        const std::string default_revision = !context.world_content_hash.empty()
            ? context.world_content_hash
            : (context.world_package_hash.empty() ? std::string("custom") : context.world_package_hash);
        context.world_revision = karma::common::config::ReadStringConfig("worldRevision",
                                                                 default_revision);
    } else {
        context.world_revision = karma::common::config::ReadStringConfig("worldRevision", "bundled");
    }

    KARMA_TRACE("engine.server",
                "{}: world '{}' (id='{}' rev='{}') loaded from '{}'",
                app_name,
                context.world_name,
                context.world_id,
                context.world_revision,
                context.world_package_enabled ? context.world_dir.string() : std::string("default server config"));
    return context;
}

} // namespace bz3::server::domain

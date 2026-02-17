#include "server/domain/world_session.hpp"

#include "karma/cli/server_runtime_options.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/content/primitives.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/world_archive.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string_view>
#include <utility>
#include <vector>

namespace bz3::server::domain {

namespace {

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    karma::content::HashBytesFNV1a(hash, bytes, count);
}

void HashStringFNV1a(uint64_t& hash, std::string_view value) {
    karma::content::HashStringFNV1a(hash, value);
}

std::string Hash64ToHex(uint64_t hash) {
    return karma::content::Hash64Hex(hash);
}

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    return karma::content::ComputeWorldPackageHash(bytes);
}

struct WorldManifestSummary {
    std::string content_hash{};
    std::string manifest_hash{};
    std::vector<WorldManifestEntry> entries{};
};

WorldManifestSummary ComputeWorldManifestSummary(const std::filesystem::path& world_dir) {
    std::vector<std::filesystem::path> files{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(world_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());

    uint64_t content_hash = 14695981039346656037ULL;
    uint64_t manifest_hash = 14695981039346656037ULL;
    std::array<char, 64 * 1024> buffer{};
    const std::byte separator = std::byte{0};
    std::vector<WorldManifestEntry> entries{};
    entries.reserve(files.size());
    for (const auto& file_path : files) {
        const std::filesystem::path rel_path = std::filesystem::relative(file_path, world_dir);
        const std::string rel = rel_path.generic_string();
        HashStringFNV1a(content_hash, rel);
        HashBytesFNV1a(content_hash, &separator, 1);

        std::ifstream input(file_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Failed to open world file for content hash: " + file_path.string());
        }

        uint64_t file_hash = 14695981039346656037ULL;
        uint64_t file_size = 0;
        while (input.good()) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read_count = input.gcount();
            if (read_count > 0) {
                const auto* bytes = reinterpret_cast<const std::byte*>(buffer.data());
                HashBytesFNV1a(content_hash, bytes, static_cast<size_t>(read_count));
                HashBytesFNV1a(file_hash, bytes, static_cast<size_t>(read_count));
                file_size += static_cast<uint64_t>(read_count);
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("Failed while hashing world file: " + file_path.string());
        }
        HashBytesFNV1a(content_hash, &separator, 1);

        const std::string file_hash_hex = Hash64ToHex(file_hash);
        const std::string file_size_text = std::to_string(file_size);
        HashStringFNV1a(manifest_hash, rel);
        HashBytesFNV1a(manifest_hash, &separator, 1);
        HashStringFNV1a(manifest_hash, file_size_text);
        HashBytesFNV1a(manifest_hash, &separator, 1);
        HashStringFNV1a(manifest_hash, file_hash_hex);
        HashBytesFNV1a(manifest_hash, &separator, 1);
        entries.push_back(WorldManifestEntry{
            .path = rel,
            .size = file_size,
            .hash = file_hash_hex});
    }

    return WorldManifestSummary{
        .content_hash = Hash64ToHex(content_hash),
        .manifest_hash = Hash64ToHex(manifest_hash),
        .entries = std::move(entries)};
}

} // namespace

std::optional<WorldSessionContext> LoadWorldSessionContext(
    const karma::cli::ServerAppOptions& options) {
    WorldSessionContext context{};
    const std::string app_name = options.app_name.empty() ? std::string("server") : options.app_name;
    context.world_package_enabled = options.server_config_explicit;

    const auto overlay_path = karma::cli::ResolveServerConfigOverlayPath(options.server_config_path,
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
        karma::config::ReadStringConfig("worldName", default_world_name);

    if (context.world_package_enabled) {
        if (!std::filesystem::is_directory(context.world_dir)) {
            spdlog::error("{}: server config overlay base directory not found: {}",
                          app_name,
                          context.world_dir.string());
            return std::nullopt;
        }
        try {
            context.world_package = world::BuildWorldArchive(context.world_dir);
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

    context.world_id = karma::config::ReadStringConfig("worldId", context.world_name);
    if (context.world_package_enabled) {
        const std::string default_revision = !context.world_content_hash.empty()
            ? context.world_content_hash
            : (context.world_package_hash.empty() ? std::string("custom") : context.world_package_hash);
        context.world_revision = karma::config::ReadStringConfig("worldRevision",
                                                                 default_revision);
    } else {
        context.world_revision = karma::config::ReadStringConfig("worldRevision", "bundled");
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

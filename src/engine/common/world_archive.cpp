#include "karma/common/world_archive.hpp"

#include "karma/common/content/archive.hpp"

namespace fs = std::filesystem;

namespace world {

ArchiveBytes BuildWorldArchive(const fs::path& worldDir) {
    return karma::content::BuildWorldArchive(worldDir);
}

bool ExtractWorldArchive(const ArchiveBytes& data, const fs::path& destDir) {
    return karma::content::ExtractWorldArchive(data, destDir);
}

std::optional<karma::json::Value> ReadWorldJsonFile(const fs::path& path) {
    return karma::content::ReadWorldJsonFile(path);
}

} // namespace world

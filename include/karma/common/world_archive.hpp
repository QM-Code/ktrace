#pragma once

#include "karma/common/world_content.hpp"

#include <filesystem>
#include <optional>

namespace world {

ArchiveBytes BuildWorldArchive(const std::filesystem::path& worldDir);
bool ExtractWorldArchive(const ArchiveBytes& data, const std::filesystem::path& destDir);
std::optional<karma::json::Value> ReadWorldJsonFile(const std::filesystem::path& path);

} // namespace world

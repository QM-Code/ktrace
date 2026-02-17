#pragma once

#include "karma/common/content/types.hpp"
#include "karma/common/json.hpp"

#include <filesystem>
#include <optional>

namespace karma::content {

ArchiveBytes BuildWorldArchive(const std::filesystem::path& world_dir);
bool ExtractWorldArchive(const ArchiveBytes& data, const std::filesystem::path& dest_dir);
std::optional<karma::json::Value> ReadWorldJsonFile(const std::filesystem::path& path);

} // namespace karma::content

#pragma once

#include <filesystem>
#include "karma/renderer/types.hpp"

namespace karma::geometry {

bool LoadMesh(const std::filesystem::path& path, renderer::MeshData& out);

} // namespace karma::geometry

#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "karma/renderer/types.hpp"

namespace karma::geometry {

struct SceneMesh {
    renderer::MeshData mesh{};
    glm::mat4 transform{1.0f};
};

bool LoadMesh(const std::filesystem::path& path, renderer::MeshData& out);
bool LoadScene(const std::filesystem::path& path, std::vector<SceneMesh>& out);

} // namespace karma::geometry

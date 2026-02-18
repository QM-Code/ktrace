#include "karma/geometry/mesh_loader.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <cstdlib>
#include <cfloat>
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace karma::geometry {

namespace {
std::optional<renderer::MeshData::TextureData> loadTextureFromFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!data) {
        spdlog::error("MeshLoader: failed to load texture '{}': {}", path.string(), stbi_failure_reason());
        return std::nullopt;
    }
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.assign(data, data + (width * height * 4));
    stbi_image_free(data);
    KARMA_TRACE("render.mesh", "MeshLoader: loaded texture '{}' {}x{}", path.string(), width, height);
    return tex;
}

std::optional<renderer::MeshData::TextureData> loadTextureFromMemory(const unsigned char* bytes, size_t size, const char* label) {
    if (!bytes || size == 0) {
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, 4);
    if (!data) {
        spdlog::error("MeshLoader: failed to decode embedded texture '{}': {}", label ? label : "(embedded)", stbi_failure_reason());
        return std::nullopt;
    }
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.assign(data, data + (width * height * 4));
    stbi_image_free(data);
    KARMA_TRACE("render.mesh", "MeshLoader: decoded embedded texture '{}' {}x{}", label ? label : "(embedded)", width, height);
    return tex;
}

std::optional<renderer::MeshData::TextureData> loadTextureFromAssimpReference(const aiScene* scene,
                                                                               const std::filesystem::path& path,
                                                                               const aiString& texPath) {
    if (!scene) {
        return std::nullopt;
    }
    std::string texName = texPath.C_Str();
    if (texName.empty()) {
        return std::nullopt;
    }
    if (texName[0] == '*') {
        const int texIndex = std::atoi(texName.c_str() + 1);
        if (texIndex >= 0 && texIndex < static_cast<int>(scene->mNumTextures)) {
            const aiTexture* tex = scene->mTextures[texIndex];
            if (tex && tex->mHeight == 0) {
                return loadTextureFromMemory(
                    reinterpret_cast<const unsigned char*>(tex->pcData),
                    tex->mWidth,
                    tex->mFilename.C_Str());
            }
            spdlog::warn("MeshLoader: embedded texture '{}' has raw data; skipping", texName);
        }
        return std::nullopt;
    }
    const auto texFile = path.parent_path() / texName;
    return loadTextureFromFile(texFile);
}

std::optional<renderer::MeshData::TextureData> loadMaterialTexture(const aiScene* scene,
                                                                   aiMaterial* material,
                                                                   const std::filesystem::path& path,
                                                                   aiTextureType type) {
    if (!scene || !material) {
        return std::nullopt;
    }
    aiString texPath;
    if (material->GetTexture(type, 0, &texPath) != AI_SUCCESS) {
        return std::nullopt;
    }
    return loadTextureFromAssimpReference(scene, path, texPath);
}

bool loadMeshData(const aiScene* scene,
                  const aiMesh* mesh,
                  const std::filesystem::path& path,
                  renderer::MeshData& out,
                  renderer::MaterialDesc& out_material,
                  const aiMatrix4x4& transform) {
    if (!scene || !mesh) {
        return false;
    }

    out.positions.clear();
    out.normals.clear();
    out.uvs.clear();
    out.indices.clear();
    out.albedo.reset();
    out_material = renderer::MaterialDesc{};

    out.positions.reserve(mesh->mNumVertices);
    out.normals.reserve(mesh->mNumVertices);
    out.uvs.reserve(mesh->mNumVertices);

    aiMatrix3x3 normal_matrix = aiMatrix3x3(transform);
    normal_matrix = normal_matrix.Inverse().Transpose();
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        const aiVector3D& v = mesh->mVertices[i];
        const aiVector3D pos = transform * v;
        out.positions.emplace_back(pos.x, pos.y, pos.z);
        if (mesh->HasNormals()) {
            const aiVector3D& n = mesh->mNormals[i];
            aiVector3D nn = normal_matrix * n;
            nn.Normalize();
            out.normals.emplace_back(nn.x, nn.y, nn.z);
        } else {
            out.normals.emplace_back(0.0f, 1.0f, 0.0f);
        }
        if (mesh->HasTextureCoords(0)) {
            const aiVector3D& uv = mesh->mTextureCoords[0][i];
            out.uvs.emplace_back(uv.x, uv.y);
        } else {
            out.uvs.emplace_back(0.0f, 0.0f);
        }
    }

    const float det = transform.Determinant();
    const bool flip_winding = det < 0.0f;

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            continue;
        }
        if (flip_winding) {
            out.indices.push_back(face.mIndices[0]);
            out.indices.push_back(face.mIndices[2]);
            out.indices.push_back(face.mIndices[1]);
        } else {
            out.indices.push_back(face.mIndices[0]);
            out.indices.push_back(face.mIndices[1]);
            out.indices.push_back(face.mIndices[2]);
        }
    }

    if (scene->mNumMaterials > 0 && mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (material) {
            aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
            if (material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
                material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                out_material.base_color = glm::vec4(color.r, color.g, color.b, color.a);
            }

            ai_real metallic = out_material.metallic_factor;
            if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
                out_material.metallic_factor = std::clamp(static_cast<float>(metallic), 0.0f, 1.0f);
            }

            ai_real roughness = out_material.roughness_factor;
            if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                out_material.roughness_factor = std::clamp(static_cast<float>(roughness), 0.0f, 1.0f);
            }

            aiColor3D emissive(0.0f, 0.0f, 0.0f);
            if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                out_material.emissive_color = glm::vec3(emissive.r, emissive.g, emissive.b);
            }

            int two_sided = 0;
            if (material->Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS) {
                out_material.double_sided = (two_sided != 0);
            }

#ifdef AI_MATKEY_GLTF_ALPHAMODE
            aiString alpha_mode;
            if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) == AI_SUCCESS) {
                const std::string mode = alpha_mode.C_Str();
                if (mode == "MASK") {
                    out_material.alpha_mode = renderer::MaterialAlphaMode::Mask;
                } else if (mode == "BLEND") {
                    out_material.alpha_mode = renderer::MaterialAlphaMode::Blend;
                } else {
                    out_material.alpha_mode = renderer::MaterialAlphaMode::Opaque;
                }
            }
#endif

#ifdef AI_MATKEY_GLTF_ALPHACUTOFF
            ai_real alpha_cutoff = out_material.alpha_cutoff;
            if (material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alpha_cutoff) == AI_SUCCESS) {
                out_material.alpha_cutoff = std::clamp(static_cast<float>(alpha_cutoff), 0.0f, 1.0f);
            }
#endif
        }

        out_material.albedo = loadMaterialTexture(scene, material, path, aiTextureType_BASE_COLOR);
        if (!out_material.albedo) {
            out_material.albedo = loadMaterialTexture(scene, material, path, aiTextureType_DIFFUSE);
        }
        out.albedo = out_material.albedo;

        out_material.metallic_roughness = loadMaterialTexture(scene, material, path, aiTextureType_METALNESS);
        if (!out_material.metallic_roughness) {
            out_material.metallic_roughness = loadMaterialTexture(scene, material, path, aiTextureType_DIFFUSE_ROUGHNESS);
        }
        out_material.emissive = loadMaterialTexture(scene, material, path, aiTextureType_EMISSIVE);
    }

    return true;
}

void collectSceneMeshes(const aiScene* scene,
                        const aiNode* node,
                        const std::filesystem::path& path,
                        const aiMatrix4x4& parent_transform,
                        std::vector<SceneMesh>& out) {
    if (!node) {
        return;
    }
    const aiMatrix4x4 node_transform = parent_transform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const unsigned int mesh_index = node->mMeshes[i];
        if (mesh_index >= scene->mNumMeshes) {
            continue;
        }
        const aiMesh* mesh = scene->mMeshes[mesh_index];
        SceneMesh entry{};
        entry.transform = glm::mat4(1.0f);
        entry.material_index = mesh->mMaterialIndex;
        if (loadMeshData(scene, mesh, path, entry.mesh, entry.material, node_transform)) {
            out.push_back(std::move(entry));
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        collectSceneMeshes(scene, node->mChildren[i], path, node_transform, out);
    }
}
}

bool LoadMesh(const std::filesystem::path& path, renderer::MeshData& out) {
    Assimp::Importer importer;
    KARMA_TRACE("render.mesh", "MeshLoader: loading '{}'", path.string());
    const aiScene* scene = importer.ReadFile(path.string(),
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_GenNormals |
                                             aiProcess_CalcTangentSpace);
    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
        spdlog::error("MeshLoader: failed to load '{}': {}", path.string(), importer.GetErrorString());
        return false;
    }

    const aiMesh* mesh = scene->mMeshes[0];
    if (!mesh) {
        spdlog::error("MeshLoader: '{}' contains no mesh data", path.string());
        return false;
    }
    renderer::MaterialDesc material{};
    if (!loadMeshData(scene, mesh, path, out, material, aiMatrix4x4())) {
        spdlog::error("MeshLoader: '{}' failed to decode mesh data", path.string());
        return false;
    }

    KARMA_TRACE("render.mesh", "MeshLoader: '{}' vertices={} indices={} albedo={}",
                path.string(), out.positions.size(), out.indices.size(), out.albedo ? 1 : 0);
    return true;
}

bool LoadScene(const std::filesystem::path& path, std::vector<SceneMesh>& out) {
    Assimp::Importer importer;
    KARMA_TRACE("render.mesh", "MeshLoader: loading scene '{}'", path.string());
    const aiScene* scene = importer.ReadFile(path.string(),
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_GenNormals |
                                             aiProcess_CalcTangentSpace);
    if (!scene || !scene->mRootNode) {
        spdlog::error("MeshLoader: failed to load scene '{}': {}", path.string(), importer.GetErrorString());
        return false;
    }

    out.clear();
    collectSceneMeshes(scene, scene->mRootNode, path, aiMatrix4x4(), out);

    KARMA_TRACE("render.mesh", "MeshLoader: scene '{}' meshes={}", path.string(), out.size());
    // Compute simple scene bounds for debugging orientation.
    if (!out.empty()) {
        glm::vec3 minv(FLT_MAX);
        glm::vec3 maxv(-FLT_MAX);
        for (const auto& entry : out) {
            for (const auto& p : entry.mesh.positions) {
                minv.x = std::min(minv.x, p.x);
                minv.y = std::min(minv.y, p.y);
                minv.z = std::min(minv.z, p.z);
                maxv.x = std::max(maxv.x, p.x);
                maxv.y = std::max(maxv.y, p.y);
                maxv.z = std::max(maxv.z, p.z);
            }
        }
        KARMA_TRACE("render.mesh", "MeshLoader: scene bounds min=({:.2f},{:.2f},{:.2f}) max=({:.2f},{:.2f},{:.2f})",
                    minv.x, minv.y, minv.z, maxv.x, maxv.y, maxv.z);
    }
    return !out.empty();
}

} // namespace karma::geometry

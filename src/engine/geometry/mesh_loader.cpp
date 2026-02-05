#include "geometry/mesh_loader.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <cstdlib>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>

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

glm::mat4 toGlm(const aiMatrix4x4& m) {
    glm::mat4 out(1.0f);
    out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
    out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
    out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
    out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
    return out;
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

bool loadMeshData(const aiScene* scene,
                  const aiMesh* mesh,
                  const std::filesystem::path& path,
                  renderer::MeshData& out) {
    if (!scene || !mesh) {
        return false;
    }

    out.positions.clear();
    out.normals.clear();
    out.uvs.clear();
    out.indices.clear();
    out.albedo.reset();

    out.positions.reserve(mesh->mNumVertices);
    out.normals.reserve(mesh->mNumVertices);
    out.uvs.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        const aiVector3D& v = mesh->mVertices[i];
        out.positions.emplace_back(v.x, v.y, v.z);
        if (mesh->HasNormals()) {
            const aiVector3D& n = mesh->mNormals[i];
            out.normals.emplace_back(n.x, n.y, n.z);
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

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            continue;
        }
        out.indices.push_back(face.mIndices[0]);
        out.indices.push_back(face.mIndices[1]);
        out.indices.push_back(face.mIndices[2]);
    }

    if (scene->mNumMaterials > 0 && mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        if (material && material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            std::string texName = texPath.C_Str();
            if (!texName.empty()) {
                if (texName[0] == '*') {
                    const int texIndex = std::atoi(texName.c_str() + 1);
                    if (texIndex >= 0 && texIndex < static_cast<int>(scene->mNumTextures)) {
                        const aiTexture* tex = scene->mTextures[texIndex];
                        if (tex && tex->mHeight == 0) {
                            out.albedo = loadTextureFromMemory(
                                reinterpret_cast<const unsigned char*>(tex->pcData),
                                tex->mWidth,
                                tex->mFilename.C_Str());
                        } else {
                            spdlog::warn("MeshLoader: embedded texture '{}' has raw data; skipping", texName);
                        }
                    }
                } else {
                    const auto texFile = path.parent_path() / texName;
                    out.albedo = loadTextureFromFile(texFile);
                }
            }
        }
    }

    return true;
}

void collectSceneMeshes(const aiScene* scene,
                        const aiNode* node,
                        const std::filesystem::path& path,
                        const glm::mat4& parent_transform,
                        std::vector<SceneMesh>& out) {
    if (!node) {
        return;
    }
    const glm::mat4 node_transform = parent_transform * toGlm(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const unsigned int mesh_index = node->mMeshes[i];
        if (mesh_index >= scene->mNumMeshes) {
            continue;
        }
        const aiMesh* mesh = scene->mMeshes[mesh_index];
        SceneMesh entry{};
        entry.transform = node_transform;
        if (loadMeshData(scene, mesh, path, entry.mesh)) {
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
    if (!loadMeshData(scene, mesh, path, out)) {
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
    collectSceneMeshes(scene, scene->mRootNode, path, glm::mat4(1.0f), out);

    KARMA_TRACE("render.mesh", "MeshLoader: scene '{}' meshes={}", path.string(), out.size());
    return !out.empty();
}

} // namespace karma::geometry

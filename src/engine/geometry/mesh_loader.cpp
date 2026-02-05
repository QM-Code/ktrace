#include "geometry/mesh_loader.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <cstdlib>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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

    out.positions.clear();
    out.normals.clear();
    out.uvs.clear();
    out.indices.clear();

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

    KARMA_TRACE("render.mesh", "MeshLoader: '{}' vertices={} indices={} albedo={}",
                path.string(), out.positions.size(), out.indices.size(), out.albedo ? 1 : 0);
    return true;
}

} // namespace karma::geometry

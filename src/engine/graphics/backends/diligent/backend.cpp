#include "karma/graphics/backends/diligent/backend.hpp"
#include "karma/graphics/backends/diligent/ui_bridge.hpp"
#if defined(KARMA_UI_BACKEND_IMGUI)
#include "karma/ui/platform/imgui/renderer_diligent.hpp"
#endif

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/geometry/mesh_loader.hpp"
#include "platform/window.hpp"

#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <DiligentCore/Platforms/interface/NativeWindow.h>
#include <DiligentCore/Primitives/interface/DebugOutput.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(KARMA_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

namespace graphics_backend {

namespace {

struct Constants {
    Diligent::float4x4 mvp;
    Diligent::float4 color;
};

struct SkyboxConstants {
    Diligent::float4x4 viewProj;
};

struct Vertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

void DILIGENT_CALL_TYPE DiligentMessageCallback(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                                const Diligent::Char* message,
                                                const Diligent::Char* /*function*/,
                                                const Diligent::Char* /*file*/,
                                                int /*line*/) {
    if (!message) {
        return;
    }
    switch (severity) {
        case Diligent::DEBUG_MESSAGE_SEVERITY_INFO:
            spdlog::trace("Diligent: {}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_WARNING:
            spdlog::warn("Diligent: {}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_ERROR:
            spdlog::error("Diligent: {}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
            spdlog::critical("Diligent: {}", message);
            break;
        default:
            spdlog::debug("Diligent: {}", message);
            break;
    }
}

bool isWorldModelPath(const std::filesystem::path& path) {
    return path.filename() == "world.glb";
}

bool isShotModelPath(const std::filesystem::path& path) {
    return path.filename() == "shot.glb";
}

std::string getThemeName() {
    return karma::config::ReadRequiredStringConfig("graphics.theme");
}

bool isLikelyGrass(const MeshLoader::TextureData& texture) {
    if (texture.pixels.empty() || texture.width <= 0 || texture.height <= 0) {
        return false;
    }
    const int sampleCount = 4096;
    const int totalPixels = texture.width * texture.height;
    const int step = std::max(1, totalPixels / sampleCount);
    uint64_t sumR = 0;
    uint64_t sumG = 0;
    uint64_t sumB = 0;
    int samples = 0;
    for (int i = 0; i < totalPixels; i += step) {
        const size_t idx = static_cast<size_t>(i) * 4;
        if (idx + 2 >= texture.pixels.size()) {
            break;
        }
        sumR += texture.pixels[idx + 0];
        sumG += texture.pixels[idx + 1];
        sumB += texture.pixels[idx + 2];
        ++samples;
    }
    if (samples == 0) {
        return false;
    }
    const float r = static_cast<float>(sumR) / samples;
    const float g = static_cast<float>(sumG) / samples;
    const float b = static_cast<float>(sumB) / samples;
    return g > r * 1.15f && g > b * 1.15f;
}

std::filesystem::path themePathFor(const std::string& theme, const std::string& slot) {
    return karma::data::Resolve("common/textures/themes/" + theme + "_" + slot + ".png");
}

std::filesystem::path skyboxPathFor(const std::string& name, const std::string& face) {
    return karma::data::Resolve("common/textures/skybox/" + name + "_" + face + ".png");
}

std::string themeSlotForWorldSubmesh(const MeshLoader::MeshData& submesh) {
    const bool isEmbeddedGrass = submesh.albedo && submesh.albedo->key.find("embedded:0") != std::string::npos;
    const bool isEmbeddedBuildingTop = submesh.albedo && submesh.albedo->key.find("embedded:2") != std::string::npos;
    const bool isGrass = (submesh.albedo && isLikelyGrass(*submesh.albedo)) || isEmbeddedGrass;
    if (isGrass) {
        return "grass";
    }
    if (isEmbeddedBuildingTop) {
        return "building-top";
    }
    return "building";
}

glm::vec4 readVec4ConfigRequired(const char* path) {
    const auto* value = karma::config::ConfigStore::Get(path);
    if (!value || !value->is_array() || value->size() < 4) {
        throw std::runtime_error(std::string("Missing required vec4 config: ") + path);
    }
    glm::vec4 out(0.0f);
    for (size_t i = 0; i < 4; ++i) {
        const auto& v = (*value)[i];
        if (v.is_number_float()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<double>());
        } else if (v.is_number_integer()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<int64_t>());
        } else if (v.is_number_unsigned()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<uint64_t>());
        } else {
            throw std::runtime_error(std::string("Invalid vec4 config type at: ") + path);
        }
    }
    return out;
}

} // namespace

Diligent::RefCntAutoPtr<Diligent::ITexture> createTextureRGBA8(Diligent::IRenderDevice* device,
                                                               int width,
                                                               int height,
                                                               const uint8_t* pixels,
                                                               const char* name) {
    if (!device || width <= 0 || height <= 0 || !pixels) {
        return {};
    }
    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.Name = name;

    Diligent::TextureData initData;
    Diligent::TextureSubResData sub{};
    sub.pData = pixels;
    sub.Stride = static_cast<Diligent::Uint32>(width * 4);
    initData.pSubResources = &sub;
    initData.NumSubresources = 1;

    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    device->CreateTexture(desc, &initData, &texture);
    return texture;
}

bool createCubemapRGBA8(Diligent::IRenderDevice* device,
                        int width,
                        int height,
                        const std::array<std::vector<uint8_t>, 6>& faces,
                        Diligent::RefCntAutoPtr<Diligent::ITexture>& outTexture,
                        Diligent::ITextureView*& outSrv) {
    if (!device || width <= 0 || height <= 0) {
        return false;
    }
    const size_t faceSize = static_cast<size_t>(width * height * 4);
    for (const auto& face : faces) {
        if (face.size() != faceSize) {
            return false;
        }
    }

    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_CUBE;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.ArraySize = 6;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.Name = "KARMA Diligent Skybox";

    std::array<Diligent::TextureSubResData, 6> subresources{};
    for (size_t i = 0; i < faces.size(); ++i) {
        subresources[i].pData = faces[i].data();
        subresources[i].Stride = static_cast<Diligent::Uint32>(width * 4);
    }

    Diligent::TextureData initData;
    initData.pSubResources = subresources.data();
    initData.NumSubresources = static_cast<Diligent::Uint32>(subresources.size());

    device->CreateTexture(desc, &initData, &outTexture);
    if (!outTexture) {
        return false;
    }
    outSrv = outTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return outSrv != nullptr;
}

DiligentBackend::DiligentBackend(platform::Window& windowIn)
    : window(&windowIn) {
    if (window) {
        window->getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth <= 0) {
            framebufferWidth = 1;
        }
        if (framebufferHeight <= 0) {
            framebufferHeight = 1;
        }
    }
    themeName = getThemeName();
    initDiligent();
    buildSkyboxResources();
}

DiligentBackend::~DiligentBackend() {
    uiBridge_.reset();
    if (uiOverlayToken_ != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(uiOverlayToken_);
        uiOverlayToken_ = 0;
    }
    for (auto& [id, target] : renderTargets) {
        if (target.srvToken != 0) {
            graphics_backend::diligent_ui::UnregisterExternalTexture(target.srvToken);
            target.srvToken = 0;
        }
    }
    graphics_backend::diligent_ui::ClearContext();
    textureCache.clear();
    whiteTexture_ = nullptr;
    whiteTextureView_ = nullptr;
}

void DiligentBackend::beginFrame() {
    if (!initialized || !context_ || !swapChain_) {
        return;
    }
    // Debug clear so we can see if the swapchain is presenting.
    auto* rtv = swapChain_->GetCurrentBackBufferRTV();
    auto* dsv = swapChain_->GetDepthBufferDSV();
    if (!rtv) {
        spdlog::warn("Graphics(Diligent): swapchain RTV is null in beginFrame");
        return;
    }
    static bool logged = false;
    if (!logged) {
        const auto desc = swapChain_->GetDesc();
        spdlog::info("Graphics(Diligent): swapchain RTV={} DSV={} size={}x{}",
                     static_cast<void*>(rtv), static_cast<void*>(dsv),
                     desc.Width, desc.Height);
        logged = true;
    }
    if (rtv) {
        context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float clearColor[4] = {0.02f, 0.02f, 0.02f, 1.0f};
        context_->ClearRenderTarget(rtv, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    if (dsv) {
        context_->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void DiligentBackend::endFrame() {
    if (!initialized || !swapChain_) {
        return;
    }
    swapChain_->Present();
}

void DiligentBackend::resize(int width, int height) {
    framebufferWidth = std::max(1, width);
    framebufferHeight = std::max(1, height);
    destroySceneTarget();
    updateSwapChain(framebufferWidth, framebufferHeight);
}

graphics::EntityId DiligentBackend::createEntity(graphics::LayerId layer) {
    const graphics::EntityId id = nextEntityId++;
    EntityRecord record;
    record.layer = layer;
    entities[id] = record;
    return id;
}

graphics::EntityId DiligentBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                      graphics::LayerId layer,
                                                      graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId DiligentBackend::createMeshEntity(graphics::MeshId mesh,
                                                     graphics::LayerId layer,
                                                     graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void DiligentBackend::setEntityModel(graphics::EntityId entity,
                                     const std::filesystem::path& modelPath,
                                     graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.modelPath = modelPath;
    it->second.material = materialOverride;

    const std::string pathKey = modelPath.string();
    auto cacheIt = modelMeshCache.find(pathKey);
    if (cacheIt != modelMeshCache.end()) {
        it->second.meshes = cacheIt->second;
        it->second.mesh = cacheIt->second.empty() ? graphics::kInvalidMesh : cacheIt->second.front();
        return;
    }

    const auto resolved = karma::data::Resolve(modelPath);
    MeshLoader::LoadOptions options;
    options.loadTextures = true;
    auto loaded = MeshLoader::loadGLB(resolved.string(), options);
    if (loaded.empty()) {
        return;
    }

    std::vector<graphics::MeshId> modelMeshes;
    modelMeshes.reserve(loaded.size());
    for (const auto& submesh : loaded) {
        graphics::MeshData meshData;
        meshData.vertices = submesh.vertices;
        meshData.indices = submesh.indices;
        meshData.texcoords = submesh.texcoords;
        meshData.normals = submesh.normals;
        const graphics::MeshId meshId = createMesh(meshData);
        if (meshId == graphics::kInvalidMesh) {
            continue;
        }
        modelMeshes.push_back(meshId);
        auto meshIt = meshes.find(meshId);
        if (meshIt != meshes.end()) {
            std::string themeSlot;
            if (isShotModelPath(modelPath)) {
                themeSlot = "shot";
            } else if (isWorldModelPath(modelPath)) {
                themeSlot = themeSlotForWorldSubmesh(submesh);
                if (themeSlot == "grass") {
                    meshIt->second.isWorldGrass = true;
                }
            }

            if (!themeSlot.empty()) {
                const auto themePath = themePathFor(themeName, themeSlot);
                const std::string key = "theme:" + themeName + ":" + themeSlot;
                auto texIt = textureCache.find(key);
                if (texIt == textureCache.end()) {
                    int w = 0;
                    int h = 0;
                    int c = 0;
                    stbi_uc* pixels = stbi_load(themePath.string().c_str(), &w, &h, &c, 4);
                    if (pixels && w > 0 && h > 0) {
                        auto texture = createTextureRGBA8(device_, w, h, pixels, "KARMA Diligent Theme");
                        stbi_image_free(pixels);
                        if (texture) {
                            textureCache.emplace(key, texture);
                            texIt = textureCache.find(key);
                        }
                    } else if (pixels) {
                        stbi_image_free(pixels);
                    }
                }
                if (texIt != textureCache.end()) {
                    meshIt->second.texture = texIt->second;
                    meshIt->second.srv = texIt->second->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                }
            }

            if (!meshIt->second.srv && submesh.albedo && !submesh.albedo->pixels.empty()) {
                const std::string& key = submesh.albedo->key;
                auto texIt = textureCache.find(key);
                if (texIt == textureCache.end()) {
                    auto texture = createTextureRGBA8(device_,
                                                     submesh.albedo->width,
                                                     submesh.albedo->height,
                                                     submesh.albedo->pixels.data(),
                                                     "KARMA Diligent Albedo");
                    if (texture) {
                        textureCache.emplace(key, texture);
                        texIt = textureCache.find(key);
                    }
                }
                if (texIt != textureCache.end()) {
                    meshIt->second.texture = texIt->second;
                    meshIt->second.srv = texIt->second->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                }
            }
            if (!meshIt->second.srv) {
                meshIt->second.texture = whiteTexture_;
                meshIt->second.srv = whiteTextureView_;
            }
        }
    }

    it->second.meshes = modelMeshes;
    it->second.mesh = modelMeshes.empty() ? graphics::kInvalidMesh : modelMeshes.front();
    modelMeshCache.emplace(pathKey, std::move(modelMeshes));
}

void DiligentBackend::setEntityMesh(graphics::EntityId entity,
                                    graphics::MeshId mesh,
                                    graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.mesh = mesh;
    it->second.meshes.clear();
    it->second.material = materialOverride;
}

void DiligentBackend::destroyEntity(graphics::EntityId entity) {
    entities.erase(entity);
}

graphics::MeshId DiligentBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    if (!initialized || !device_) {
        return id;
    }

    MeshRecord record;
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        meshes[id] = record;
        return id;
    }

    std::vector<glm::vec3> normals = mesh.normals;
    if (normals.size() != mesh.vertices.size()) {
        normals.assign(mesh.vertices.size(), glm::vec3(0.0f));
        if (mesh.indices.size() >= 3) {
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const uint32_t i0 = mesh.indices[i];
                const uint32_t i1 = mesh.indices[i + 1];
                const uint32_t i2 = mesh.indices[i + 2];
                if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
                    continue;
                }
                const glm::vec3& v0 = mesh.vertices[i0];
                const glm::vec3& v1 = mesh.vertices[i1];
                const glm::vec3& v2 = mesh.vertices[i2];
                const glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                normals[i0] += n;
                normals[i1] += n;
                normals[i2] += n;
            }
            for (auto& n : normals) {
                if (glm::length(n) > 0.0f) {
                    n = glm::normalize(n);
                } else {
                    n = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
        } else {
            for (auto& n : normals) {
                n = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    std::vector<Vertex> packed;
    packed.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        const auto& n = normals[i];
        const glm::vec2 uv = (i < mesh.texcoords.size()) ? mesh.texcoords[i] : glm::vec2(0.0f);
        packed[i] = {v.x, v.y, v.z, n.x, n.y, n.z, uv.x, uv.y};
    }

    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "KARMA Diligent VB";
    vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vbDesc.Size = static_cast<Diligent::Uint32>(packed.size() * sizeof(Vertex));
    Diligent::BufferData vbData;
    vbData.pData = packed.data();
    vbData.DataSize = vbDesc.Size;
    device_->CreateBuffer(vbDesc, &vbData, &record.vertexBuffer);

    Diligent::BufferDesc ibDesc;
    ibDesc.Name = "KARMA Diligent IB";
    ibDesc.Usage = Diligent::USAGE_IMMUTABLE;
    ibDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    ibDesc.Size = static_cast<Diligent::Uint32>(mesh.indices.size() * sizeof(uint32_t));
    Diligent::BufferData ibData;
    ibData.pData = mesh.indices.data();
    ibData.DataSize = ibDesc.Size;
    device_->CreateBuffer(ibDesc, &ibData, &record.indexBuffer);
    record.indexCount = static_cast<uint32_t>(mesh.indices.size());

    if (!whiteTexture_) {
        const uint32_t white = 0xffffffffu;
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.Name = "KARMA Diligent White Texture";

        Diligent::TextureData initData;
        Diligent::TextureSubResData sub{};
        sub.pData = &white;
        sub.Stride = sizeof(white);
        initData.pSubResources = &sub;
        initData.NumSubresources = 1;
        device_->CreateTexture(desc, &initData, &whiteTexture_);
        if (whiteTexture_) {
            whiteTextureView_ = whiteTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }
    record.texture = whiteTexture_;
    record.srv = whiteTextureView_;

    meshes[id] = std::move(record);
    return id;
}

void DiligentBackend::destroyMesh(graphics::MeshId mesh) {
    meshes.erase(mesh);
}

graphics::MaterialId DiligentBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void DiligentBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials.find(material);
    if (it == materials.end()) {
        return;
    }
    it->second = desc;
}

void DiligentBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void DiligentBackend::setMaterialFloat(graphics::MaterialId, std::string_view, float) {
}

graphics::RenderTargetId DiligentBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const graphics::RenderTargetId id = nextRenderTargetId++;
    RenderTargetRecord record;
    record.desc = desc;
    if (initialized && device_ && desc.width > 0 && desc.height > 0) {
        Diligent::TextureDesc colorDesc;
        colorDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        colorDesc.Width = static_cast<Diligent::Uint32>(desc.width);
        colorDesc.Height = static_cast<Diligent::Uint32>(desc.height);
        colorDesc.MipLevels = 1;
        colorDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        colorDesc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
        colorDesc.Name = "KARMA Diligent RT Color";
        device_->CreateTexture(colorDesc, nullptr, &record.colorTexture);
        if (record.colorTexture) {
            record.rtv = record.colorTexture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
            record.srv = record.colorTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            record.srvToken = graphics_backend::diligent_ui::RegisterExternalTexture(record.srv);
        }

        if (desc.depth) {
            Diligent::TextureDesc depthDesc;
            depthDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
            depthDesc.Width = static_cast<Diligent::Uint32>(desc.width);
            depthDesc.Height = static_cast<Diligent::Uint32>(desc.height);
            depthDesc.MipLevels = 1;
            depthDesc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
            depthDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
            depthDesc.Name = "KARMA Diligent RT Depth";
            device_->CreateTexture(depthDesc, nullptr, &record.depthTexture);
            if (record.depthTexture) {
                record.dsv = record.depthTexture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
            }
        }
    }
    renderTargets[id] = record;
    return id;
}

void DiligentBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return;
    }
    if (it->second.srvToken != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(it->second.srvToken);
        it->second.srvToken = 0;
    }
    renderTargets.erase(it);
}

void DiligentBackend::renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
    if (!initialized || !context_) {
        return;
    }
    ensurePipeline();
    if (!pipeline_ || !pipelineOffscreen_) {
        return;
    }

    const uint64_t revision = karma::config::ConfigStore::Revision();
    if (revision != configRevision_) {
        configRevision_ = revision;
    }

    Diligent::ITextureView* rtv = nullptr;
    Diligent::ITextureView* dsv = nullptr;
    int targetWidth = 0;
    int targetHeight = 0;
    if (target == graphics::kDefaultRenderTarget) {
        if (!swapChain_) {
            return;
        }
        const bool wantsBrightness = std::abs(brightness_ - 1.0f) > 0.0001f;
        if (wantsBrightness) {
            const auto& scDesc = swapChain_->GetDesc();
            ensureSceneTarget(static_cast<int>(scDesc.Width), static_cast<int>(scDesc.Height));
            if (sceneTargetValid_ && sceneTarget_.rtv) {
                rtv = sceneTarget_.rtv;
                dsv = sceneTarget_.dsv;
                targetWidth = static_cast<int>(scDesc.Width);
                targetHeight = static_cast<int>(scDesc.Height);
            } else {
                rtv = swapChain_->GetCurrentBackBufferRTV();
                dsv = swapChain_->GetDepthBufferDSV();
                if (!rtv) {
                    spdlog::warn("Graphics(Diligent): swapchain RTV is null in renderLayer");
                    return;
                }
                targetWidth = static_cast<int>(scDesc.Width);
                targetHeight = static_cast<int>(scDesc.Height);
            }
        } else {
            rtv = swapChain_->GetCurrentBackBufferRTV();
            dsv = swapChain_->GetDepthBufferDSV();
            if (!rtv) {
                spdlog::warn("Graphics(Diligent): swapchain RTV is null in renderLayer");
                return;
            }
            const auto& scDesc = swapChain_->GetDesc();
            targetWidth = static_cast<int>(scDesc.Width);
            targetHeight = static_cast<int>(scDesc.Height);
        }
    } else {
        auto it = renderTargets.find(target);
        if (it == renderTargets.end()) {
            return;
        }
        rtv = it->second.rtv;
        dsv = it->second.dsv;
        targetWidth = it->second.desc.width;
        targetHeight = it->second.desc.height;
    }


    const bool drawSkybox = (target == graphics::kDefaultRenderTarget);
    const bool offscreenPass = (target != graphics::kDefaultRenderTarget && layer != 0);
    renderToTargets(rtv, dsv, layer, targetWidth, targetHeight, drawSkybox, offscreenPass);

    if (target == graphics::kDefaultRenderTarget && std::abs(brightness_ - 1.0f) > 0.0001f
        && sceneTargetValid_ && sceneTarget_.srv) {
        renderBrightnessPass();
    }
}

unsigned int DiligentBackend::getRenderTargetTextureId(graphics::RenderTargetId target) const {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return 0u;
    }
    if (it->second.srvToken == 0) {
        return 0u;
    }
    return static_cast<unsigned int>(it->second.srvToken);
}

void DiligentBackend::setUiOverlayTexture(const graphics::TextureHandle& texture) {
    if (!texture.valid()) {
        uiOverlayToken_ = 0;
        uiOverlayWidth_ = 0;
        uiOverlayHeight_ = 0;
        return;
    }
    uiOverlayToken_ = texture.id;
    uiOverlayWidth_ = texture.width;
    uiOverlayHeight_ = texture.height;
}

void DiligentBackend::setUiOverlayVisible(bool visible) {
    uiOverlayVisible_ = visible;
}

void DiligentBackend::renderUiOverlay() {
    if (!initialized || !context_ || !swapChain_) {
        return;
    }
    if (uiOverlayVisible_ && uiOverlayToken_ == 0) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("Graphics(Diligent): UI overlay visible but token is 0 (size={}x{}).",
                         static_cast<unsigned int>(uiOverlayWidth_),
                         static_cast<unsigned int>(uiOverlayHeight_));
            loggedOnce = true;
        }
        return;
    }
    if (!uiOverlayVisible_ || uiOverlayToken_ == 0) {
        return;
    }

    ensureUiOverlayPipeline();
    if (!uiOverlayPipeline_ || !uiOverlayBinding_ || !uiOverlayVertexBuffer_) {
        return;
    }

    auto* textureView = graphics_backend::diligent_ui::ResolveExternalTexture(uiOverlayToken_);
    if (!textureView) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("Graphics(Diligent): UI overlay texture resolve failed (token={}, size={}x{}).",
                         static_cast<unsigned long long>(uiOverlayToken_),
                         static_cast<unsigned int>(uiOverlayWidth_),
                         static_cast<unsigned int>(uiOverlayHeight_));
            loggedOnce = true;
        }
        return;
    }
    if (auto* var = uiOverlayBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")) {
        var->Set(textureView);
    }

    auto* rtv = swapChain_->GetCurrentBackBufferRTV();
    if (!rtv) {
        return;
    }
    context_->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& scDesc = swapChain_->GetDesc();
    Diligent::Viewport vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(scDesc.Width);
    vp.Height = static_cast<float>(scDesc.Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->SetViewports(1, &vp, 0, 0);

    const Diligent::Uint64 offset = 0;
    Diligent::IBuffer* vbs[] = {uiOverlayVertexBuffer_};
    context_->SetVertexBuffers(0, 1, vbs, &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                               Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

    context_->SetPipelineState(uiOverlayPipeline_);
    context_->CommitShaderResources(uiOverlayBinding_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
    context_->Draw(drawAttrs);
}

void DiligentBackend::setBrightness(float brightness) {
    brightness_ = brightness;
}

void DiligentBackend::initDiligent() {
    if (initialized || !window) {
        return;
    }

    auto* factory = Diligent::GetEngineFactoryVk();
    if (!factory) {
        spdlog::error("Graphics(Diligent): Vulkan factory not available");
        return;
    }
    factory->SetMessageCallback(DiligentMessageCallback);

    Diligent::NativeWindow nativeWindow;
    bool nativeWindowReady = false;

#if defined(KARMA_WINDOW_BACKEND_SDL3)
    auto* sdlWindow = static_cast<SDL_Window*>(window->nativeHandle());
    if (sdlWindow) {
        const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
        if (props != 0) {
            if (void* wlDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
                void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
                if (wlSurface) {
                    Diligent::LinuxNativeWindow linuxWindow{};
                    linuxWindow.pDisplay = wlDisplay;
                    linuxWindow.pWaylandSurface = wlSurface;
                    linuxWindow.WindowId = 0;
                    nativeWindow = Diligent::NativeWindow{linuxWindow};
                    nativeWindowReady = true;
                    spdlog::info("Graphics(Diligent): using Wayland native window");
                }
            }
            if (!nativeWindowReady) {
                if (const Sint64 x11Window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)) {
                    void* x11Display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
                    Diligent::LinuxNativeWindow linuxWindow{};
                    linuxWindow.pDisplay = x11Display;
                    linuxWindow.WindowId = static_cast<uintptr_t>(x11Window);
                    nativeWindow = Diligent::NativeWindow{linuxWindow};
                    nativeWindowReady = true;
                    spdlog::info("Graphics(Diligent): using X11 native window");
                }
            }
        }
    }
#endif

    if (!nativeWindowReady) {
        spdlog::error("Graphics(Diligent): failed to resolve native window for Vulkan swapchain");
        return;
    }

    Diligent::EngineVkCreateInfo engineCI;
    Diligent::SwapChainDesc swapDesc;
    swapDesc.Width = static_cast<Diligent::Uint32>(framebufferWidth);
    swapDesc.Height = static_cast<Diligent::Uint32>(framebufferHeight);
    swapDesc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    swapDesc.DepthBufferFormat = Diligent::TEX_FORMAT_D32_FLOAT;

    Diligent::IRenderDevice* deviceRaw = nullptr;
    Diligent::IDeviceContext* contextRaw = nullptr;
    factory->CreateDeviceAndContextsVk(engineCI, &deviceRaw, &contextRaw);
    device_.Attach(deviceRaw);
    context_.Attach(contextRaw);
    if (!device_ || !context_) {
        spdlog::error("Graphics(Diligent): failed to create Vulkan device/context");
        return;
    }
    factory->CreateSwapChainVk(device_, context_, swapDesc, nativeWindow, &swapChain_);
    if (!swapChain_) {
        spdlog::error("Graphics(Diligent): failed to create Vulkan swapchain");
        return;
    }

    initialized = true;
    graphics_backend::diligent_ui::SetContext(device_, context_, swapChain_, framebufferWidth, framebufferHeight);
#if defined(KARMA_UI_BACKEND_IMGUI)
    if (!uiBridge_) {
        uiBridge_ = std::make_unique<DiligentRenderer>();
    }
#endif
    spdlog::info("Graphics(Diligent): Vulkan initialized");
}

void DiligentBackend::ensurePipeline() {
    if (!initialized || !device_) {
        return;
    }
    if (pipeline_ && pipelineOffscreen_) {
        return;
    }

    const char* vsSource = R"(
cbuffer Constants
{
    float4x4 g_MVP;
    float4 g_Color;
};
struct VSInput
{
    float3 Pos : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    Out.Pos = mul(g_MVP, float4(In.Pos, 1.0));
    Out.UV = In.UV;
    return Out;
}
)";

    const char* psSource = R"(
Texture2D g_Texture;
SamplerState g_Texture_sampler;
cbuffer Constants
{
    float4x4 g_MVP;
    float4 g_Color;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};
float4 main(PSInput In) : SV_Target
{
    float4 texColor = g_Texture.Sample(g_Texture_sampler, In.UV);
    return texColor * g_Color;
}
)";

    Diligent::ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "KARMA Diligent VS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = vsSource;
    device_->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "KARMA Diligent PS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = psSource;
    device_->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("Graphics(Diligent): failed to create shaders");
        return;
    }

    if (!constantBuffer_) {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "KARMA Diligent CB";
        cbDesc.Size = sizeof(Constants);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &constantBuffer_);
    }

    auto buildPipeline = [&](Diligent::TEXTURE_FORMAT rtvFormat,
                             Diligent::TEXTURE_FORMAT dsvFormat,
                             bool depthEnable,
                             bool depthWrite,
                             bool blendEnable,
                             Diligent::RefCntAutoPtr<Diligent::IPipelineState>& pipeline,
                             Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& binding,
                             const char* name) {
        if (pipeline) {
            return;
        }
        Diligent::GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = name;
        psoCI.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        psoCI.pVS = vs;
        psoCI.pPS = ps;
        psoCI.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = depthEnable;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = depthWrite;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;
        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = rtvFormat;
        psoCI.GraphicsPipeline.DSVFormat = dsvFormat;

        Diligent::LayoutElement layout[] = {
            {0, 0, 3, Diligent::VT_FLOAT32, false},
            {1, 0, 3, Diligent::VT_FLOAT32, false},
            {2, 0, 2, Diligent::VT_FLOAT32, false}
        };
        psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
        psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(std::size(layout));

        Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(std::size(vars));

        Diligent::SamplerDesc samplerDesc;
        samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
        samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
        samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
        samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_WRAP;

        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "g_Texture_sampler", samplerDesc}
        };
        psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(std::size(samplers));

        if (blendEnable) {
            auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
            rt0.BlendEnable = true;
            rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
            rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
            rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
            rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
            rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
            rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
        }

        device_->CreatePipelineState(psoCI, &pipeline);
        if (!pipeline) {
            spdlog::error("Graphics(Diligent): failed to create pipeline state '{}'", name);
            return;
        }

        if (constantBuffer_) {
            if (auto* var = pipeline->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
                var->Set(constantBuffer_);
            }
            if (auto* var = pipeline->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
                var->Set(constantBuffer_);
            }
        }

        pipeline->CreateShaderResourceBinding(&binding, true);
    };

    Diligent::TEXTURE_FORMAT swapRtv = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    Diligent::TEXTURE_FORMAT swapDsv = Diligent::TEX_FORMAT_D32_FLOAT;
    if (swapChain_) {
        const auto& scDesc = swapChain_->GetDesc();
        swapRtv = scDesc.ColorBufferFormat;
        swapDsv = scDesc.DepthBufferFormat;
    }

    buildPipeline(swapRtv,
                  swapDsv,
                  true,
                  true,
                  false,
                  pipeline_,
                  shaderBinding_,
                  "KARMA Diligent PSO");
    // Offscreen main scene still needs depth for correct world rendering.
    buildPipeline(Diligent::TEX_FORMAT_RGBA8_UNORM,
                  Diligent::TEX_FORMAT_D32_FLOAT,
                  true,
                  true,
                  false,
                  pipelineOffscreen_,
                  shaderBindingOffscreen_,
                  "KARMA Diligent PSO Offscreen");
    // Overlay entities render without depth.
    buildPipeline(swapRtv,
                  swapDsv,
                  false,
                  false,
                  true,
                  pipelineOverlay_,
                  shaderBindingOverlay_,
                  "KARMA Diligent PSO Overlay");
    buildPipeline(Diligent::TEX_FORMAT_RGBA8_UNORM,
                  Diligent::TEX_FORMAT_D32_FLOAT,
                  false,
                  false,
                  true,
                  pipelineOverlayOffscreen_,
                  shaderBindingOverlayOffscreen_,
                  "KARMA Diligent PSO Overlay Offscreen");
}

void DiligentBackend::buildSkyboxResources() {
    if (!initialized || !device_ || skyboxReady) {
        return;
    }

    const std::string mode = karma::config::ReadRequiredStringConfig("graphics.skybox.Mode");
    spdlog::info("Graphics(Diligent): skybox mode='{}'", mode);
    if (mode != "cubemap") {
        return;
    }

    const std::string name = karma::config::ReadRequiredStringConfig("graphics.skybox.Cubemap.Name");
    spdlog::info("Graphics(Diligent): skybox cubemap='{}'", name);
    const std::array<std::string, 6> faces = {"right", "left", "up", "down", "front", "back"};
    std::array<std::vector<uint8_t>, 6> facePixels{};
    int faceWidth = 0;
    int faceHeight = 0;

    for (size_t i = 0; i < faces.size(); ++i) {
        const std::filesystem::path facePath = skyboxPathFor(name, faces[i]);
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(facePath.string().c_str(), &width, &height, &channels, 4);
        if (!pixels || width <= 0 || height <= 0) {
            spdlog::warn("Graphics(Diligent): failed to load skybox face '{}'", facePath.string());
            if (pixels) {
                stbi_image_free(pixels);
            }
            return;
        }
        if (i == 0) {
            faceWidth = width;
            faceHeight = height;
        } else if (width != faceWidth || height != faceHeight) {
            stbi_image_free(pixels);
            spdlog::warn("Graphics(Diligent): skybox faces have mismatched dimensions");
            return;
        }

        facePixels[i].assign(pixels, pixels + static_cast<size_t>(width * height * 4));
        stbi_image_free(pixels);
    }

    if (!createCubemapRGBA8(device_, faceWidth, faceHeight, facePixels, skyboxTexture_, skyboxSrv_)) {
        spdlog::warn("Graphics(Diligent): failed to create skybox cubemap");
        return;
    }

    const char* vsSource = R"(
cbuffer SkyboxConstants
{
    float4x4 g_ViewProj;
};
struct VSInput
{
    float3 Pos : ATTRIB0;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float3 Dir : TEXCOORD0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    Out.Dir = In.Pos;
    Out.Pos = mul(g_ViewProj, float4(In.Pos, 1.0));
    return Out;
}
)";

    const char* psSource = R"(
TextureCube g_Skybox;
SamplerState g_Skybox_sampler;
float4 main(float3 dir : TEXCOORD0) : SV_Target
{
    return g_Skybox.Sample(g_Skybox_sampler, normalize(dir));
}
)";

    Diligent::ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "KARMA Diligent Skybox VS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = vsSource;
    device_->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "KARMA Diligent Skybox PS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = psSource;
    device_->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("Graphics(Diligent): failed to create skybox shaders");
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "KARMA Diligent Skybox PSO";
    psoCI.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    psoCI.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    if (swapChain_) {
        const auto& scDesc = swapChain_->GetDesc();
        psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
    }

    Diligent::LayoutElement layout[] = {
        {0, 0, 3, Diligent::VT_FLOAT32, false}
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(std::size(layout));

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Skybox", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(std::size(vars));

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;

    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Skybox_sampler", samplerDesc}
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(std::size(samplers));

    device_->CreatePipelineState(psoCI, &skyboxPipeline_);
    if (!skyboxPipeline_) {
        spdlog::error("Graphics(Diligent): failed to create skybox pipeline state");
        return;
    }

    Diligent::BufferDesc cbDesc;
    cbDesc.Name = "KARMA Diligent Skybox CB";
    cbDesc.Size = sizeof(SkyboxConstants);
    cbDesc.Usage = Diligent::USAGE_DYNAMIC;
    cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device_->CreateBuffer(cbDesc, nullptr, &skyboxConstantBuffer_);
    if (skyboxConstantBuffer_) {
        if (auto* var = skyboxPipeline_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "SkyboxConstants")) {
            var->Set(skyboxConstantBuffer_);
        }
    }

    skyboxPipeline_->CreateShaderResourceBinding(&skyboxBinding_, true);

    static const float cubeVerts[] = {
        -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,

        -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,
    };

    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "KARMA Diligent Skybox VB";
    vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(cubeVerts);
    Diligent::BufferData vbData;
    vbData.pData = cubeVerts;
    vbData.DataSize = vbDesc.Size;
    device_->CreateBuffer(vbDesc, &vbData, &skyboxVertexBuffer_);

    if (!skyboxVertexBuffer_ || !skyboxBinding_ || !skyboxSrv_) {
        spdlog::warn("Graphics(Diligent): skybox resources incomplete");
        return;
    }

    if (auto* var = skyboxBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Skybox")) {
        var->Set(skyboxSrv_);
    }

    skyboxReady = true;
    spdlog::info("Graphics(Diligent): skybox ready={}", skyboxReady);
}

void DiligentBackend::ensureUiOverlayPipeline() {
    if (!initialized || !device_) {
        return;
    }
    if (uiOverlayPipeline_) {
        return;
    }

    const char* vsSource = R"(
struct VSInput
{
    float2 Pos : ATTRIB0;
    float2 UV  : ATTRIB1;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    Out.Pos = float4(In.Pos, 0.0, 1.0);
    Out.UV = In.UV;
    return Out;
}
)";

    const char* psSource = R"(
Texture2D g_Texture;
SamplerState g_Texture_sampler;
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    return g_Texture.Sample(g_Texture_sampler, uv);
}
)";

    Diligent::ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.EntryPoint = "main";

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "KARMA Diligent UI Overlay VS";
    shaderCI.Source = vsSource;
    device_->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "KARMA Diligent UI Overlay PS";
    shaderCI.Source = psSource;
    device_->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("Graphics(Diligent): failed to create UI overlay shaders");
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "KARMA Diligent UI Overlay PSO";
    psoCI.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    psoCI.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    if (swapChain_) {
        const auto& scDesc = swapChain_->GetDesc();
        psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
    }

    auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = true;
    rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
    rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;

    Diligent::LayoutElement layout[] = {
        {0, 0, 2, Diligent::VT_FLOAT32, false},
        {1, 0, 2, Diligent::VT_FLOAT32, false}
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(std::size(layout));

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(std::size(vars));

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;

    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture_sampler", samplerDesc}
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(std::size(samplers));

    device_->CreatePipelineState(psoCI, &uiOverlayPipeline_);
    if (!uiOverlayPipeline_) {
        spdlog::error("Graphics(Diligent): failed to create UI overlay pipeline");
        return;
    }

    uiOverlayPipeline_->CreateShaderResourceBinding(&uiOverlayBinding_, true);

    struct UiOverlayVertex {
        float x;
        float y;
        float u;
        float v;
    };
    const UiOverlayVertex verts[] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f, 0.0f},
        { 1.0f,  1.0f, 1.0f, 0.0f}
    };

    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "KARMA Diligent UI Overlay VB";
    vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(verts);
    Diligent::BufferData vbData;
    vbData.pData = verts;
    vbData.DataSize = vbDesc.Size;
    device_->CreateBuffer(vbDesc, &vbData, &uiOverlayVertexBuffer_);
}

void DiligentBackend::ensureBrightnessPipeline() {
    if (!initialized || !device_) {
        return;
    }
    if (brightnessPipeline_) {
        return;
    }

    const char* vsSource = R"(
struct VSInput
{
    float2 Pos : ATTRIB0;
    float2 UV  : ATTRIB1;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    Out.Pos = float4(In.Pos, 0.0, 1.0);
    Out.UV = In.UV;
    return Out;
}
)";

    const char* psSource = R"(
Texture2D g_Texture;
SamplerState g_Texture_sampler;
cbuffer Constants
{
    float4 g_Brightness;
};
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    float4 color = g_Texture.Sample(g_Texture_sampler, uv);
    return float4(color.rgb * g_Brightness.x, 1.0f);
}
)";

    Diligent::ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.EntryPoint = "main";

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "KARMA Diligent Brightness VS";
    shaderCI.Source = vsSource;
    device_->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "KARMA Diligent Brightness PS";
    shaderCI.Source = psSource;
    device_->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("Graphics(Diligent): failed to create brightness shaders");
        return;
    }

    if (!brightnessConstantBuffer_) {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "KARMA Diligent Brightness CB";
        cbDesc.Size = sizeof(float) * 4;
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &brightnessConstantBuffer_);
    }

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "KARMA Diligent Brightness PSO";
    psoCI.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    psoCI.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    if (swapChain_) {
        const auto& scDesc = swapChain_->GetDesc();
        psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
    }

    Diligent::LayoutElement layout[] = {
        {0, 0, 2, Diligent::VT_FLOAT32, false},
        {1, 0, 2, Diligent::VT_FLOAT32, false}
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(std::size(layout));

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_PIXEL, "Constants", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(std::size(vars));

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;

    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture_sampler", samplerDesc}
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(std::size(samplers));

    device_->CreatePipelineState(psoCI, &brightnessPipeline_);
    if (!brightnessPipeline_) {
        spdlog::error("Graphics(Diligent): failed to create brightness pipeline");
        return;
    }

    brightnessPipeline_->CreateShaderResourceBinding(&brightnessBinding_, true);

    struct BrightnessVertex {
        float x;
        float y;
        float u;
        float v;
    };
    const BrightnessVertex verts[] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f, 0.0f},
        { 1.0f,  1.0f, 1.0f, 0.0f}
    };

    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "KARMA Diligent Brightness VB";
    vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(verts);
    Diligent::BufferData vbData;
    vbData.pData = verts;
    vbData.DataSize = vbDesc.Size;
    device_->CreateBuffer(vbDesc, &vbData, &brightnessVertexBuffer_);
}

void DiligentBackend::ensureSceneTarget(int width, int height) {
    if (!initialized || !device_) {
        return;
    }
    if (sceneTargetValid_ && sceneTarget_.desc.width == width && sceneTarget_.desc.height == height) {
        return;
    }
    destroySceneTarget();
    if (width <= 0 || height <= 0) {
        return;
    }

    sceneTarget_.desc.width = width;
    sceneTarget_.desc.height = height;
    sceneTarget_.desc.depth = true;

    Diligent::TextureDesc colorDesc;
    colorDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    colorDesc.Width = static_cast<Diligent::Uint32>(width);
    colorDesc.Height = static_cast<Diligent::Uint32>(height);
    colorDesc.MipLevels = 1;
    if (swapChain_) {
        colorDesc.Format = swapChain_->GetDesc().ColorBufferFormat;
    } else {
        colorDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    }
    colorDesc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    colorDesc.Name = "KARMA Diligent Scene Color";
    device_->CreateTexture(colorDesc, nullptr, &sceneTarget_.colorTexture);
    if (sceneTarget_.colorTexture) {
        sceneTarget_.rtv = sceneTarget_.colorTexture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
        sceneTarget_.srv = sceneTarget_.colorTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }

    Diligent::TextureDesc depthDesc;
    depthDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    depthDesc.Width = static_cast<Diligent::Uint32>(width);
    depthDesc.Height = static_cast<Diligent::Uint32>(height);
    depthDesc.MipLevels = 1;
    if (swapChain_) {
        depthDesc.Format = swapChain_->GetDesc().DepthBufferFormat;
    } else {
        depthDesc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
    }
    depthDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
    depthDesc.Name = "KARMA Diligent Scene Depth";
    device_->CreateTexture(depthDesc, nullptr, &sceneTarget_.depthTexture);
    if (sceneTarget_.depthTexture) {
        sceneTarget_.dsv = sceneTarget_.depthTexture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
    }

    sceneTargetValid_ = (sceneTarget_.rtv != nullptr && sceneTarget_.srv != nullptr);
}

void DiligentBackend::destroySceneTarget() {
    sceneTarget_ = {};
    sceneTargetValid_ = false;
}

void DiligentBackend::renderBrightnessPass() {
    if (!initialized || !context_ || !swapChain_) {
        return;
    }
    if (!sceneTarget_.srv) {
        return;
    }
    ensureBrightnessPipeline();
    if (!brightnessPipeline_ || !brightnessBinding_ || !brightnessVertexBuffer_ || !brightnessConstantBuffer_) {
        return;
    }

    if (auto* var = brightnessBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")) {
        var->Set(sceneTarget_.srv);
    }
    if (auto* var = brightnessBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
        var->Set(brightnessConstantBuffer_);
    }

    Diligent::MapHelper<float> cb(context_, brightnessConstantBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    if (cb) {
        cb[0] = brightness_;
        cb[1] = 0.0f;
        cb[2] = 0.0f;
        cb[3] = 0.0f;
    }

    auto* rtv = swapChain_->GetCurrentBackBufferRTV();
    if (!rtv) {
        return;
    }
    context_->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& scDesc = swapChain_->GetDesc();
    Diligent::Viewport vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(scDesc.Width);
    vp.Height = static_cast<float>(scDesc.Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->SetViewports(1, &vp, 0, 0);

    const Diligent::Uint64 offset = 0;
    Diligent::IBuffer* vbs[] = {brightnessVertexBuffer_};
    context_->SetVertexBuffers(0, 1, vbs, &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                               Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

    context_->SetPipelineState(brightnessPipeline_);
    context_->CommitShaderResources(brightnessBinding_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
    context_->Draw(drawAttrs);
}

void DiligentBackend::updateSwapChain(int width, int height) {
    if (!initialized || !swapChain_) {
        return;
    }
    swapChain_->Resize(static_cast<Diligent::Uint32>(width), static_cast<Diligent::Uint32>(height));
    graphics_backend::diligent_ui::SetContext(device_, context_, swapChain_, width, height);
}

void DiligentBackend::renderToTargets(Diligent::ITextureView* rtv,
                                      Diligent::ITextureView* dsv,
                                      graphics::LayerId layer,
                                      int targetWidth,
                                      int targetHeight,
                                      bool drawSkybox,
                                      bool offscreenPass) {
    if (!context_ || !pipeline_ || !pipelineOffscreen_) {
        return;
    }
    context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (targetWidth > 0 && targetHeight > 0) {
        Diligent::Viewport vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = static_cast<float>(targetWidth);
        vp.Height = static_cast<float>(targetHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context_->SetViewports(1, &vp, targetWidth, targetHeight);

        Diligent::Rect scissor;
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = targetWidth;
        scissor.bottom = targetHeight;
        context_->SetScissorRects(1, &scissor, targetWidth, targetHeight);
    }

    const bool useSwapchain = (swapChain_ && rtv == swapChain_->GetCurrentBackBufferRTV());
    const float clearColor[4] = {
        offscreenPass ? 0.0f : 0.02f,
        offscreenPass ? 0.0f : 0.02f,
        offscreenPass ? 0.0f : 0.02f,
        offscreenPass ? 0.0f : 1.0f
    };
    if (rtv) {
        context_->ClearRenderTarget(rtv, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    if (dsv) {
        context_->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    if (drawSkybox && skyboxReady && skyboxPipeline_ && skyboxVertexBuffer_) {
        const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
        const glm::mat4 viewProj = computeProjectionMatrix() * rotation;
        Diligent::MapHelper<SkyboxConstants> cb(context_, skyboxConstantBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        if (cb) {
            std::memcpy(cb->viewProj.m, glm::value_ptr(viewProj), sizeof(float) * 16);
        }

        context_->SetPipelineState(skyboxPipeline_);
        if (skyboxBinding_) {
            context_->CommitShaderResources(skyboxBinding_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        Diligent::IBuffer* vbs[] = {skyboxVertexBuffer_};
        const Diligent::Uint64 offsets[] = {0};
        context_->SetVertexBuffers(0, 1, vbs, offsets,
                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                   Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

        Diligent::DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 36;
        drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
        context_->Draw(drawAttrs);
    }

    const glm::mat4 viewProj = getViewProjectionMatrix();
    auto renderEntity = [&](const EntityRecord& entity) {
        if (entity.layer != layer || !entity.visible) {
            return;
        }

        auto* pipeline = useSwapchain
            ? (entity.overlay ? pipelineOverlay_.RawPtr() : pipeline_.RawPtr())
            : (entity.overlay ? pipelineOverlayOffscreen_.RawPtr() : pipelineOffscreen_.RawPtr());
        auto* binding = useSwapchain
            ? (entity.overlay ? shaderBindingOverlay_.RawPtr() : shaderBinding_.RawPtr())
            : (entity.overlay ? shaderBindingOverlayOffscreen_.RawPtr() : shaderBindingOffscreen_.RawPtr());
        if (!pipeline || !binding) {
            return;
        }
        context_->SetPipelineState(pipeline);

        const glm::mat4 translate = glm::translate(glm::mat4(1.0f), entity.position);
        const glm::mat4 rotate = glm::mat4_cast(entity.rotation);
        const glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity.scale);
        const glm::mat4 world = translate * rotate * scale;
        const glm::mat4 mvp = viewProj * world;

        graphics::MaterialDesc desc;
        auto matIt = materials.find(entity.material);
        if (matIt != materials.end()) {
            desc = matIt->second;
        }
        const Diligent::float4 baseColor(desc.baseColor.x, desc.baseColor.y, desc.baseColor.z, desc.baseColor.w);

        auto drawMesh = [&](graphics::MeshId meshId) {
            auto meshIt = meshes.find(meshId);
            if (meshIt == meshes.end()) {
                return;
            }
            const MeshRecord& mesh = meshIt->second;
            if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0) {
                return;
            }
            if (offscreenPass && mesh.isWorldGrass) {
                return;
            }

            Diligent::MapHelper<Constants> cb(context_, constantBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
            if (cb) {
                std::memcpy(cb->mvp.m, glm::value_ptr(mvp), sizeof(float) * 16);
                cb->color = baseColor;
            }

            Diligent::IBuffer* vbs[] = {mesh.vertexBuffer};
            const Diligent::Uint64 offsets[] = {0};
            context_->SetVertexBuffers(0, 1, vbs, offsets,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                       Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            context_->SetIndexBuffer(mesh.indexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            if (auto* var = binding->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")) {
                if (mesh.srv) {
                    var->Set(mesh.srv);
                }
            }
            context_->CommitShaderResources(binding, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawIndexedAttribs drawAttrs;
            drawAttrs.IndexType = Diligent::VT_UINT32;
            drawAttrs.NumIndices = mesh.indexCount;
            drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            context_->DrawIndexed(drawAttrs);
        };

        if (!entity.meshes.empty()) {
            for (graphics::MeshId meshId : entity.meshes) {
                drawMesh(meshId);
            }
        } else if (entity.mesh != graphics::kInvalidMesh) {
            drawMesh(entity.mesh);
        }
    };

    for (const auto& [id, entity] : entities) {
        (void)id;
        if (entity.overlay) {
            continue;
        }
        renderEntity(entity);
    }
    for (const auto& [id, entity] : entities) {
        (void)id;
        if (!entity.overlay) {
            continue;
        }
        renderEntity(entity);
    }

    (void)useSwapchain;
}

void DiligentBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.position = position;
}

void DiligentBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.rotation = rotation;
}

void DiligentBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.scale = scale;
}

void DiligentBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.visible = visible;
}

void DiligentBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.transparent = transparency;
}

void DiligentBackend::setOverlay(graphics::EntityId entity, bool overlay) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.overlay = overlay;
}

void DiligentBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
}

void DiligentBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
}

void DiligentBackend::setPerspective(float fovDeg, float aspect, float nearPlaneIn, float farPlaneIn) {
    usePerspective = true;
    fovDegrees = fovDeg;
    aspectRatio = aspect;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

void DiligentBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlaneIn, float farPlaneIn) {
    usePerspective = false;
    orthoLeft = left;
    orthoRight = right;
    orthoTop = top;
    orthoBottom = bottom;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

glm::mat4 DiligentBackend::computeViewMatrix() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPosition);
    return rotation * translation;
}

glm::mat4 DiligentBackend::computeProjectionMatrix() const {
    if (usePerspective) {
        return glm::perspectiveRH_ZO(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    return glm::orthoRH_ZO(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
}

glm::mat4 DiligentBackend::getViewProjectionMatrix() const {
    return computeProjectionMatrix() * computeViewMatrix();
}

glm::mat4 DiligentBackend::getViewMatrix() const {
    return computeViewMatrix();
}

glm::mat4 DiligentBackend::getProjectionMatrix() const {
    return computeProjectionMatrix();
}

glm::vec3 DiligentBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 DiligentBackend::getCameraForward() const {
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

} // namespace graphics_backend

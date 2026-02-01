#include "karma/graphics/backends/bgfx/backend.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/file_utils.hpp"
#include "karma/graphics/backends/bgfx/texture_utils.hpp"
#include "karma/geometry/mesh_loader.hpp"
#include "platform/window.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <stdexcept>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <stb_image.h>
#include <array>
#include <vector>

#if defined(KARMA_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

namespace {
constexpr bgfx::ViewId kUiOverlayView = 253;
constexpr bgfx::ViewId kBrightnessView = 252;

struct NativeWindowInfo {
    void* nwh = nullptr;
    void* ndt = nullptr;
    void* context = nullptr;
    bgfx::NativeWindowHandleType::Enum handleType = bgfx::NativeWindowHandleType::Default;
};

NativeWindowInfo getNativeWindowInfo(platform::Window* window) {
    if (!window) {
        return {};
    }
    void* handle = window->nativeHandle();
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    auto* sdlWindow = static_cast<SDL_Window*>(handle);
    if (!sdlWindow) {
        return {};
    }
    NativeWindowInfo info{};
    const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    if (props == 0) {
        spdlog::warn("Graphics(Bgfx): SDL_GetWindowProperties failed: {}", SDL_GetError());
    }
    if (props != 0) {
        if (void* wlDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
            info.ndt = wlDisplay;
            void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            spdlog::trace("Graphics(Bgfx): Wayland handles display={} surface={}", info.ndt, wlSurface);
            if (wlSurface) {
                info.nwh = wlSurface;
                info.handleType = bgfx::NativeWindowHandleType::Wayland;
                spdlog::trace("Graphics(Bgfx): using Wayland surface handle");
                return info;
            }
            spdlog::trace("Graphics(Bgfx): Wayland display found but no surface");
            return {};
        }
    }
    return {};
#else
    return {handle, nullptr, nullptr};
#endif
}


bgfx::ShaderHandle loadShader(const std::filesystem::path& path) {
    auto bytes = karma::file::ReadFileBytes(path);
    if (bytes.empty()) {
        spdlog::error("Graphics(Bgfx): failed to read shader '{}'", path.string());
        return BGFX_INVALID_HANDLE;
    }
    const bgfx::Memory* mem = bgfx::copy(bytes.data(), static_cast<uint32_t>(bytes.size()));
    return bgfx::createShader(mem);
}

std::filesystem::path getBgfxShaderDir(std::string_view subdir = {}) {
    std::filesystem::path base = karma::data::Resolve("bgfx/shaders/bin");
    base /= "vk";
    if (!subdir.empty()) {
        base /= subdir;
    }
    return base;
}

// helper removed; scene target is created in ensureSceneTarget()

bgfx::TextureHandle createTextureRGBA8(int width, int height, const uint8_t* pixels) {
    if (width <= 0 || height <= 0 || !pixels) {
        return BGFX_INVALID_HANDLE;
    }
    const uint32_t size = static_cast<uint32_t>(width * height * 4);
    const bgfx::Memory* mem = bgfx::copy(pixels, size);
    return bgfx::createTexture2D(static_cast<uint16_t>(width),
                                 static_cast<uint16_t>(height),
                                 false,
                                 1,
                                 bgfx::TextureFormat::RGBA8,
                                 BGFX_SAMPLER_NONE,
                                 mem);
}

bgfx::TextureHandle createCubemapRGBA8(int width, int height, const std::array<std::vector<uint8_t>, 6>& faces) {
    if (width <= 0 || height <= 0) {
        return BGFX_INVALID_HANDLE;
    }
    const uint32_t faceSize = static_cast<uint32_t>(width * height * 4);
    const uint32_t totalSize = faceSize * 6;
    std::vector<uint8_t> combined;
    combined.reserve(totalSize);
    for (const auto& face : faces) {
        if (face.size() != faceSize) {
            return BGFX_INVALID_HANDLE;
        }
        combined.insert(combined.end(), face.begin(), face.end());
    }
    const bgfx::Memory* mem = bgfx::copy(combined.data(), totalSize);
    return bgfx::createTextureCube(static_cast<uint16_t>(width),
                                   false,
                                   1,
                                   bgfx::TextureFormat::RGBA8,
                                   BGFX_SAMPLER_NONE,
                                   mem);
}

bool isWorldModelPath(const std::filesystem::path& path) {
    return path.filename() == "world.glb";
}

bool isShotModelPath(const std::filesystem::path& path) {
    return path.filename() == "shot.glb";
}

std::string getThemeName() {
    const char* env = std::getenv("KARMA_BGFX_THEME");
    if (!env || !*env) {
        return karma::config::ReadRequiredStringConfig("graphics.theme");
    }
    return std::string(env);
}

bgfx::TextureHandle loadTextureFromFile(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return BGFX_INVALID_HANDLE;
    }
    bgfx::TextureHandle handle = createTextureRGBA8(width, height, pixels);
    stbi_image_free(pixels);
    return handle;
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

std::string themePathFor(const std::string& theme, const std::string& slot) {
    return "common/textures/themes/" + theme + "_" + slot + ".png";
}

std::filesystem::path skyboxPathFor(const std::string& name, const std::string& face) {
    return karma::data::Resolve("common/textures/skybox/" + name + "_" + face + ".png");
}

glm::vec3 readVec3ConfigRequired(const char* path) {
    const auto* value = karma::config::ConfigStore::Get(path);
    if (!value || !value->is_array() || value->size() < 3) {
        throw std::runtime_error(std::string("Missing required vec3 config: ") + path);
    }
    glm::vec3 out(0.0f);
    for (size_t i = 0; i < 3; ++i) {
        const auto& v = (*value)[i];
        if (v.is_number_float()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<double>());
        } else if (v.is_number_integer()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<int64_t>());
        } else if (v.is_number_unsigned()) {
            out[static_cast<int>(i)] = static_cast<float>(v.get<uint64_t>());
        } else {
            throw std::runtime_error(std::string("Invalid vec3 config type at: ") + path);
        }
    }
    return out;
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

struct TestVertex {
    float x;
    float y;
    float z;
    uint32_t abgr;
};
} // namespace

namespace graphics_backend {
namespace {
BgfxRendererPreference g_rendererPreference = BgfxRendererPreference::Auto;
}

void SetBgfxRendererPreference(BgfxRendererPreference preference) {
    g_rendererPreference = preference;
}

BgfxBackend::BgfxBackend(platform::Window& windowIn)
    : window(&windowIn) {
    spdlog::trace("Graphics(Bgfx): ctor begin");
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
    spdlog::trace("Graphics(Bgfx): theme = '{}'", themeName);

    const auto nativeInfo = getNativeWindowInfo(window);
    bgfx::PlatformData pd{};
    pd.ndt = nativeInfo.ndt;
    pd.nwh = nativeInfo.nwh;
    pd.context = nativeInfo.context;
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    pd.type = nativeInfo.handleType;
#endif
    spdlog::trace("Graphics(Bgfx): platform nwh={} ndt={} ctx={}", pd.nwh, pd.ndt, pd.context);

    if (!pd.ndt || !pd.nwh) {
        spdlog::error("Graphics(Bgfx): missing native display/window handle (ndt={}, nwh={})", pd.ndt, pd.nwh);
        return;
    }

    bgfx::Init init{};
    switch (g_rendererPreference) {
        case BgfxRendererPreference::Vulkan:
            init.type = bgfx::RendererType::Vulkan;
            break;
        case BgfxRendererPreference::Auto:
        default:
            init.type = bgfx::RendererType::Vulkan;
            break;
    }
    spdlog::trace("Graphics(Bgfx): requested renderer {}", static_cast<int>(init.type));
    init.vendorId = BGFX_PCI_ID_NONE;
    init.platformData = pd;
    init.resolution.width = static_cast<uint32_t>(framebufferWidth);
    init.resolution.height = static_cast<uint32_t>(framebufferHeight);
    init.resolution.reset = BGFX_RESET_VSYNC;
    initialized = bgfx::init(init);

    spdlog::trace("Graphics(Bgfx): init result={} size={}x{}", initialized, framebufferWidth, framebufferHeight);
    if (initialized) {
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x0d1620ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(framebufferWidth), static_cast<uint16_t>(framebufferHeight));
        bgfx::setViewTransform(0, nullptr, nullptr);
        buildTestResources();
        buildSkyboxResources();
        spdlog::trace("Graphics(Bgfx): init ok renderer={} testReady={}",
                     static_cast<int>(bgfx::getRendererType()),
                     testReady);
    }
}

BgfxBackend::~BgfxBackend() {
    if (initialized) {
        if (bgfx::isValid(testVertexBuffer)) {
            bgfx::destroy(testVertexBuffer);
        }
        if (bgfx::isValid(testIndexBuffer)) {
            bgfx::destroy(testIndexBuffer);
        }
        if (bgfx::isValid(testProgram)) {
            bgfx::destroy(testProgram);
        }
        if (bgfx::isValid(meshProgram)) {
            bgfx::destroy(meshProgram);
        }
        if (bgfx::isValid(meshColorUniform)) {
            bgfx::destroy(meshColorUniform);
        }
        if (bgfx::isValid(meshSamplerUniform)) {
            bgfx::destroy(meshSamplerUniform);
        }
        if (bgfx::isValid(meshLightDirUniform)) {
            bgfx::destroy(meshLightDirUniform);
        }
        if (bgfx::isValid(meshLightColorUniform)) {
            bgfx::destroy(meshLightColorUniform);
        }
        if (bgfx::isValid(meshAmbientColorUniform)) {
            bgfx::destroy(meshAmbientColorUniform);
        }
        if (bgfx::isValid(meshUnlitUniform)) {
            bgfx::destroy(meshUnlitUniform);
        }
        if (bgfx::isValid(skyboxVertexBuffer)) {
            bgfx::destroy(skyboxVertexBuffer);
        }
        if (bgfx::isValid(skyboxProgram)) {
            bgfx::destroy(skyboxProgram);
        }
        if (bgfx::isValid(skyboxSamplerUniform)) {
            bgfx::destroy(skyboxSamplerUniform);
        }
        if (bgfx::isValid(skyboxTexture)) {
            bgfx::destroy(skyboxTexture);
        }
        if (bgfx::isValid(uiOverlayProgram)) {
            bgfx::destroy(uiOverlayProgram);
        }
        if (bgfx::isValid(uiOverlaySampler)) {
            bgfx::destroy(uiOverlaySampler);
        }
        if (bgfx::isValid(uiOverlayScaleBias)) {
            bgfx::destroy(uiOverlayScaleBias);
        }
        if (bgfx::isValid(brightnessProgram)) {
            bgfx::destroy(brightnessProgram);
        }
        if (bgfx::isValid(brightnessSampler)) {
            bgfx::destroy(brightnessSampler);
        }
        if (bgfx::isValid(brightnessScaleBias)) {
            bgfx::destroy(brightnessScaleBias);
        }
        if (bgfx::isValid(brightnessValue)) {
            bgfx::destroy(brightnessValue);
        }
        destroySceneTarget();
        for (auto& [id, mesh] : meshes) {
            if (bgfx::isValid(mesh.vertexBuffer)) {
                bgfx::destroy(mesh.vertexBuffer);
            }
            if (bgfx::isValid(mesh.indexBuffer)) {
                bgfx::destroy(mesh.indexBuffer);
            }
        }
        for (auto& [key, texture] : textureCache) {
            if (bgfx::isValid(texture)) {
                bgfx::destroy(texture);
            }
        }
        if (bgfx::isValid(whiteTexture)) {
            bgfx::destroy(whiteTexture);
        }
        // Flush any queued work before shutdown to avoid backend teardown races.
        bgfx::frame();
        bgfx::shutdown();
        initialized = false;
        testReady = false;
    }
}

void BgfxBackend::beginFrame() {
    if (!initialized) {
        return;
    }
    if (testReady) {
        bgfx::setViewTransform(0, nullptr, nullptr);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexBuffer(0, testVertexBuffer);
        bgfx::setIndexBuffer(testIndexBuffer);
        bgfx::submit(0, testProgram);
    }
    bgfx::touch(0);
}

void BgfxBackend::endFrame() {
    if (!initialized) {
        return;
    }
    bgfx::frame();
}

void BgfxBackend::resize(int width, int height) {
    framebufferWidth = std::max(1, width);
    framebufferHeight = std::max(1, height);
    destroySceneTarget();
    if (initialized) {
        bgfx::reset(static_cast<uint32_t>(framebufferWidth),
                    static_cast<uint32_t>(framebufferHeight),
                    BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(framebufferWidth), static_cast<uint16_t>(framebufferHeight));
    }
}

graphics::EntityId BgfxBackend::createEntity(graphics::LayerId layer) {
    const graphics::EntityId id = nextEntityId++;
    EntityRecord record;
    record.layer = layer;
    entities[id] = record;
    return id;
}

graphics::EntityId BgfxBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                  graphics::LayerId layer,
                                                  graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId BgfxBackend::createMeshEntity(graphics::MeshId mesh,
                                                 graphics::LayerId layer,
                                                 graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void BgfxBackend::setEntityModel(graphics::EntityId entity,
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
    const bool useTheme = !themeName.empty() && themeName != "none";
    auto loadThemeTexture = [&](const std::string& slot) -> bgfx::TextureHandle {
        if (!useTheme) {
            return BGFX_INVALID_HANDLE;
        }
        const std::string themeKey = "theme:" + slot + ":" + themeName;
        auto themeIt = textureCache.find(themeKey);
        if (themeIt != textureCache.end()) {
            return themeIt->second;
        }
        const std::filesystem::path themePath = karma::data::Resolve(themePathFor(themeName, slot));
        if (std::filesystem::exists(themePath)) {
            bgfx::TextureHandle handle = loadTextureFromFile(themePath);
            if (bgfx::isValid(handle)) {
                textureCache.emplace(themeKey, handle);
                spdlog::trace("Graphics(Bgfx): loaded theme texture '{}' -> {}", themeKey, themePath.string());
                return handle;
            }
            spdlog::warn("Graphics(Bgfx): failed to load theme texture '{}'", themePath.string());
        } else {
            spdlog::warn("Graphics(Bgfx): theme '{}' not found at '{}'", themeKey, themePath.string());
        }
        return BGFX_INVALID_HANDLE;
    };
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

        if (isShotModelPath(modelPath)) {
            bgfx::TextureHandle themed = loadThemeTexture("shot");
            if (bgfx::isValid(themed)) {
                auto meshIt = meshes.find(meshId);
                if (meshIt != meshes.end()) {
                    meshIt->second.texture = themed;
                }
            }
        }

        if (submesh.albedo) {
            auto meshIt = meshes.find(meshId);
            if (meshIt != meshes.end()) {
                bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
                std::string slot;
                if (isShotModelPath(modelPath)) {
                    slot = "shot";
                } else if (isWorldModelPath(modelPath)) {
                    const bool isEmbeddedGrass = submesh.albedo->key.find("embedded:0") != std::string::npos;
                    const bool isEmbeddedBuildingTop = submesh.albedo->key.find("embedded:2") != std::string::npos;
                    const bool isGrass = isEmbeddedGrass || isLikelyGrass(*submesh.albedo);
                    slot = isGrass ? "grass" : (isEmbeddedBuildingTop ? "building-top" : "building");
                    spdlog::trace("Graphics(Bgfx): submesh tex='{}' grass={} theme='{}' slot='{}'",
                                 submesh.albedo->key, isGrass, themeName, slot);
                    if (isGrass) {
                        meshIt->second.isWorldGrass = true;
                    }
                }
                if (!slot.empty()) {
                    handle = loadThemeTexture(slot);
                }

                if (!bgfx::isValid(handle)) {
                    auto texIt = textureCache.find(submesh.albedo->key);
                    if (texIt != textureCache.end()) {
                        handle = texIt->second;
                    } else {
                        handle = createTextureRGBA8(submesh.albedo->width,
                                                    submesh.albedo->height,
                                                    submesh.albedo->pixels.data());
                        if (bgfx::isValid(handle)) {
                            textureCache.emplace(submesh.albedo->key, handle);
                        }
                    }
                }

                if (bgfx::isValid(handle)) {
                    meshIt->second.texture = handle;
                }
            }
        }
    }

    it->second.meshes = modelMeshes;
    it->second.mesh = modelMeshes.empty() ? graphics::kInvalidMesh : modelMeshes.front();
    modelMeshCache.emplace(pathKey, std::move(modelMeshes));
}

void BgfxBackend::setEntityMesh(graphics::EntityId entity,
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

void BgfxBackend::destroyEntity(graphics::EntityId entity) {
    entities.erase(entity);
}

graphics::MeshId BgfxBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    if (!initialized) {
        return id;
    }
    if (!meshReady) {
        buildMeshResources();
    }
    if (mesh.vertices.empty()) {
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

    std::vector<float> verts;
    verts.reserve(mesh.vertices.size() * 8);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        const auto& n = normals[i];
        verts.push_back(v.x);
        verts.push_back(v.y);
        verts.push_back(v.z);
        verts.push_back(n.x);
        verts.push_back(n.y);
        verts.push_back(n.z);
        if (i < mesh.texcoords.size()) {
            const auto& uv = mesh.texcoords[i];
            verts.push_back(uv.x);
            verts.push_back(uv.y);
        } else {
            verts.push_back(0.0f);
            verts.push_back(0.0f);
        }
    }

    MeshRecord record;
    record.vertexBuffer = bgfx::createVertexBuffer(
        bgfx::copy(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float))),
        meshLayout);

    if (!mesh.indices.empty()) {
        record.indexBuffer = bgfx::createIndexBuffer(
            bgfx::copy(mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32);
        record.indexCount = static_cast<uint32_t>(mesh.indices.size());
    }

    record.texture = whiteTexture;
    meshes[id] = record;
    return id;
}

void BgfxBackend::destroyMesh(graphics::MeshId mesh) {
    auto it = meshes.find(mesh);
    if (it == meshes.end()) {
        return;
    }
    if (bgfx::isValid(it->second.vertexBuffer)) {
        bgfx::destroy(it->second.vertexBuffer);
    }
    if (bgfx::isValid(it->second.indexBuffer)) {
        bgfx::destroy(it->second.indexBuffer);
    }
    meshes.erase(it);
}

graphics::MaterialId BgfxBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void BgfxBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials.find(material);
    if (it == materials.end()) {
        return;
    }
    it->second = desc;
}

void BgfxBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void BgfxBackend::setMaterialFloat(graphics::MaterialId, std::string_view, float) {
}

graphics::RenderTargetId BgfxBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const graphics::RenderTargetId id = nextRenderTargetId++;
    RenderTargetRecord record;
    record.desc = desc;
    if (!initialized) {
        spdlog::trace("Graphics(Bgfx): created render target {} size={}x{} fb=false color=false depth=false (bgfx not initialized)",
                      id, desc.width, desc.height);
        renderTargets[id] = record;
        return id;
    }
    if (desc.width > 0 && desc.height > 0) {
        const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        record.colorTexture = bgfx::createTexture2D(
            static_cast<uint16_t>(desc.width),
            static_cast<uint16_t>(desc.height),
            false,
            1,
            bgfx::TextureFormat::RGBA8,
            colorFlags);

        if (desc.depth || desc.stencil) {
            bgfx::TextureFormat::Enum depthFormat = desc.stencil ? bgfx::TextureFormat::D24S8 : bgfx::TextureFormat::D24;
            const uint64_t depthFlags = BGFX_TEXTURE_RT;
            record.depthTexture = bgfx::createTexture2D(
                static_cast<uint16_t>(desc.width),
                static_cast<uint16_t>(desc.height),
                false,
                1,
                depthFormat,
                depthFlags);
        }

        bgfx::Attachment attachments[2];
        uint8_t attachmentCount = 0;
        if (bgfx::isValid(record.colorTexture)) {
            attachments[attachmentCount++].init(record.colorTexture);
        }
        if (bgfx::isValid(record.depthTexture)) {
            attachments[attachmentCount++].init(record.depthTexture);
        }
        if (attachmentCount > 0) {
            record.frameBuffer = bgfx::createFrameBuffer(attachmentCount, attachments, false);
        }
    }
    spdlog::trace("Graphics(Bgfx): created render target {} size={}x{} fb={} color={} depth={}",
                  id, desc.width, desc.height,
                  bgfx::isValid(record.frameBuffer),
                  bgfx::isValid(record.colorTexture),
                  bgfx::isValid(record.depthTexture));
    renderTargets[id] = record;
    return id;
}

void BgfxBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return;
    }
    RenderTargetRecord& record = it->second;
    if (!initialized) {
        renderTargets.erase(it);
        return;
    }
    if (bgfx::isValid(record.frameBuffer)) {
        bgfx::destroy(record.frameBuffer);
    }
    if (bgfx::isValid(record.colorTexture)) {
        bgfx::destroy(record.colorTexture);
    }
    if (bgfx::isValid(record.depthTexture)) {
        bgfx::destroy(record.depthTexture);
    }
    renderTargets.erase(it);
}

void BgfxBackend::renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
    if (!initialized) {
        return;
    }
    RenderTargetRecord* targetRecord = nullptr;
    const bool wantsBrightness = (target == graphics::kDefaultRenderTarget && std::abs(brightness - 1.0f) > 0.0001f);
    if (wantsBrightness) {
        ensureSceneTarget(framebufferWidth, framebufferHeight);
        if (sceneTargetValid && bgfx::isValid(sceneTarget.frameBuffer)) {
            targetRecord = &sceneTarget;
        }
    }
    if (target != graphics::kDefaultRenderTarget) {
        auto targetIt = renderTargets.find(target);
        if (targetIt == renderTargets.end()) {
            return;
        }
        targetRecord = &targetIt->second;
        if (!bgfx::isValid(targetRecord->frameBuffer)) {
            return;
        }
    }
    if (!meshReady) {
        buildMeshResources();
    }
    if (!meshReady) {
        return;
    }

    const glm::mat4 view = getViewMatrix();
    const glm::mat4 proj = getProjectionMatrix();
    const uint16_t viewId = static_cast<uint16_t>(layer);
    bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(proj));
    if (targetRecord) {
        bgfx::setViewFrameBuffer(viewId, targetRecord->frameBuffer);
        bgfx::setViewRect(viewId, 0, 0,
                          static_cast<uint16_t>(targetRecord->desc.width),
                          static_cast<uint16_t>(targetRecord->desc.height));
    } else {
        bgfx::setViewRect(viewId, 0, 0,
                          static_cast<uint16_t>(framebufferWidth),
                          static_cast<uint16_t>(framebufferHeight));
    }
    const bool offscreenPass = (target != graphics::kDefaultRenderTarget && layer != 0);
    const bool renderSkybox = (target == graphics::kDefaultRenderTarget);
    const uint32_t clearColor = renderSkybox
        ? 0x0d1620ff
        : (offscreenPass ? 0x00000000 : 0xff000000);
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::touch(viewId);

    if (renderSkybox && skyboxReady && bgfx::isValid(skyboxProgram) && bgfx::isValid(skyboxVertexBuffer) && bgfx::isValid(skyboxTexture)) {
        bgfx::setViewTransform(viewId, nullptr, nullptr);
        bgfx::setTransform(glm::value_ptr(glm::mat4(1.0f)));
        bgfx::setVertexBuffer(0, skyboxVertexBuffer);
        if (bgfx::isValid(skyboxSamplerUniform)) {
            bgfx::setTexture(0, skyboxSamplerUniform, skyboxTexture);
        }
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(viewId, skyboxProgram);
        bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(proj));
    }

    const uint64_t revision = karma::config::ConfigStore::Revision();
    if (revision != configRevision) {
        configRevision = revision;
        const glm::vec3 defaultSunDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));
        cachedSunDirection = glm::normalize(readVec3ConfigRequired("graphics.lighting.SunDirection"));
        cachedAmbientColor = readVec3ConfigRequired("graphics.lighting.AmbientColor");
        cachedSunColor = readVec3ConfigRequired("graphics.lighting.SunColor");
    }

    auto renderEntity = [&](const EntityRecord& entity) {
        if (entity.layer != layer || !entity.visible) {
            return;
        }
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), entity.position) *
            glm::mat4_cast(entity.rotation) *
            glm::scale(glm::mat4(1.0f), entity.scale);

        bgfx::setTransform(glm::value_ptr(model));
        const bool isShot = isShotModelPath(entity.modelPath);
        const bool transparent = entity.transparent ||
                                 (entity.material != graphics::kInvalidMaterial &&
                                  materials.count(entity.material) &&
                                  materials.at(entity.material).transparent);
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS;
        if (offscreenPass) {
            state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
            if (transparent) {
                state |= BGFX_STATE_BLEND_ALPHA;
            }
        } else if (isShot) {
            state |= BGFX_STATE_BLEND_ADD;
        } else if (transparent) {
            state |= BGFX_STATE_BLEND_ALPHA;
        } else {
            state |= BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW;
        }

        const glm::vec4 viewLightDir = glm::vec4(glm::mat3(view) * (-cachedSunDirection), 0.0f);
        if (bgfx::isValid(meshLightDirUniform)) {
            bgfx::setUniform(meshLightDirUniform, glm::value_ptr(viewLightDir));
        }
        if (bgfx::isValid(meshAmbientColorUniform)) {
            const glm::vec4 amb(cachedAmbientColor, 1.0f);
            bgfx::setUniform(meshAmbientColorUniform, glm::value_ptr(amb));
        }
        if (bgfx::isValid(meshLightColorUniform)) {
            const glm::vec4 col(cachedSunColor, 1.0f);
            bgfx::setUniform(meshLightColorUniform, glm::value_ptr(col));
        }

        auto drawMesh = [&](graphics::MeshId meshId) {
            if (meshId == graphics::kInvalidMesh) {
                return;
            }
            auto meshIt = meshes.find(meshId);
            if (meshIt == meshes.end()) {
                return;
            }
            const MeshRecord& mesh = meshIt->second;
            if (!bgfx::isValid(mesh.vertexBuffer)) {
                return;
            }
            if (offscreenPass && mesh.isWorldGrass) {
                return;
            }

            bgfx::setVertexBuffer(0, mesh.vertexBuffer);
            if (bgfx::isValid(mesh.indexBuffer) && mesh.indexCount > 0) {
                bgfx::setIndexBuffer(mesh.indexBuffer, 0, mesh.indexCount);
            }

            glm::vec4 color(1.0f);
            if (entity.material != graphics::kInvalidMaterial) {
                auto matIt = materials.find(entity.material);
                if (matIt != materials.end()) {
                    color = matIt->second.baseColor;
                }
            }
            if (isShot) {
                color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
            }
            bgfx::setUniform(meshColorUniform, glm::value_ptr(color));
            bgfx::TextureHandle tex = whiteTexture;
            if (bgfx::isValid(mesh.texture)) {
                tex = mesh.texture;
            }
            if (bgfx::isValid(meshSamplerUniform) && bgfx::isValid(tex)) {
                bgfx::setTexture(0, meshSamplerUniform, tex);
            }
            if (bgfx::isValid(meshUnlitUniform)) {
                const glm::vec4 unlit = (isShot || offscreenPass) ? glm::vec4(1.0f) : glm::vec4(0.0f);
                bgfx::setUniform(meshUnlitUniform, glm::value_ptr(unlit));
            }

            bgfx::setState(state);
            bgfx::submit(viewId, meshProgram);
        };

        if (!entity.meshes.empty()) {
            for (const auto meshId : entity.meshes) {
                drawMesh(meshId);
            }
        } else {
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

    if (wantsBrightness && targetRecord == &sceneTarget) {
        ensureBrightnessResources();
        if (bgfx::isValid(brightnessProgram) && bgfx::isValid(brightnessSampler)
            && bgfx::isValid(brightnessScaleBias) && bgfx::isValid(brightnessValue)) {
            const int width = framebufferWidth > 0 ? framebufferWidth : 1;
            const int height = framebufferHeight > 0 ? framebufferHeight : 1;
            const float scaleBias[4] = { 2.0f / static_cast<float>(width), -2.0f / static_cast<float>(height), -1.0f, 1.0f };
            const float brightnessData[4] = { brightness, 0.0f, 0.0f, 0.0f };
            bgfx::setViewMode(kBrightnessView, bgfx::ViewMode::Sequential);
            bgfx::setViewTransform(kBrightnessView, nullptr, nullptr);
            bgfx::setViewRect(kBrightnessView, 0, 0,
                              static_cast<uint16_t>(width),
                              static_cast<uint16_t>(height));
            bgfx::setViewClear(kBrightnessView, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
            bgfx::setUniform(brightnessScaleBias, scaleBias);
            bgfx::setUniform(brightnessValue, brightnessData);
            bgfx::setTexture(0, brightnessSampler, sceneTarget.colorTexture);

            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer tib;
            if (bgfx::getAvailTransientVertexBuffer(4, brightnessLayout) < 4
                || bgfx::getAvailTransientIndexBuffer(6, false) < 6) {
                return;
            }
            bgfx::allocTransientVertexBuffer(&tvb, 4, brightnessLayout);
            bgfx::allocTransientIndexBuffer(&tib, 6, false);

            struct BrightnessVertex {
                float x;
                float y;
                float u;
                float v;
            };
            auto* verts = reinterpret_cast<BrightnessVertex*>(tvb.data);
            verts[0] = {0.0f, 0.0f, 0.0f, 0.0f};
            verts[1] = {static_cast<float>(width), 0.0f, 1.0f, 0.0f};
            verts[2] = {static_cast<float>(width), static_cast<float>(height), 1.0f, 1.0f};
            verts[3] = {0.0f, static_cast<float>(height), 0.0f, 1.0f};

            uint16_t* indices = reinterpret_cast<uint16_t*>(tib.data);
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 0;
            indices[4] = 2;
            indices[5] = 3;

            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            bgfx::submit(kBrightnessView, brightnessProgram);
        }
    }
}

unsigned int BgfxBackend::getRenderTargetTextureId(graphics::RenderTargetId target) const {
    if (!initialized) {
        return 0u;
    }
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return 0u;
    }
    const RenderTargetRecord& record = it->second;
    if (!bgfx::isValid(record.colorTexture)) {
        return 0u;
    }
    static std::unordered_map<graphics::RenderTargetId, uint16_t> lastTextureIds;
    const uint16_t idx = record.colorTexture.idx;
    auto lastIt = lastTextureIds.find(target);
    if (lastIt == lastTextureIds.end() || lastIt->second != idx) {
        spdlog::trace("Graphics(Bgfx): render target {} texture idx={}", target, idx);
        lastTextureIds[target] = idx;
    }
    return static_cast<unsigned int>(idx + 1);
}

void BgfxBackend::setUiOverlayTexture(const graphics::TextureHandle& texture) {
    if (!initialized) {
        return;
    }
    if (!texture.valid()) {
        uiOverlayTexture = BGFX_INVALID_HANDLE;
        uiOverlayWidth = 0;
        uiOverlayHeight = 0;
        return;
    }
    bgfx::TextureHandle handle;
    handle.idx = bgfx_utils::ToBgfxTextureHandle(texture.id);
    uiOverlayTexture = handle;
    uiOverlayWidth = texture.width;
    uiOverlayHeight = texture.height;
}

void BgfxBackend::setUiOverlayVisible(bool visible) {
    uiOverlayVisible = visible;
}

void BgfxBackend::renderUiOverlay() {
    if (!initialized) {
        return;
    }
    if (uiOverlayVisible && !bgfx::isValid(uiOverlayTexture)) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("Graphics(Bgfx): UI overlay visible but texture invalid (id={}, size={}x{}).",
                         static_cast<uint64_t>(uiOverlayTexture.idx),
                         static_cast<uint32_t>(uiOverlayWidth),
                         static_cast<uint32_t>(uiOverlayHeight));
            loggedOnce = true;
        }
        return;
    }
    if (!uiOverlayVisible || !bgfx::isValid(uiOverlayTexture)) {
        return;
    }
    ensureUiOverlayResources();
    if (!bgfx::isValid(uiOverlayProgram) || !bgfx::isValid(uiOverlaySampler)
        || !bgfx::isValid(uiOverlayScaleBias)) {
        return;
    }

    const int width = framebufferWidth > 0 ? framebufferWidth : 1;
    const int height = framebufferHeight > 0 ? framebufferHeight : 1;

    const float scaleBias[4] = { 2.0f / static_cast<float>(width), -2.0f / static_cast<float>(height), -1.0f, 1.0f };
    bgfx::setViewMode(kUiOverlayView, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(kUiOverlayView, nullptr, nullptr);
    bgfx::setViewRect(kUiOverlayView, 0, 0,
                      static_cast<uint16_t>(width),
                      static_cast<uint16_t>(height));
    bgfx::setUniform(uiOverlayScaleBias, scaleBias);
    bgfx::setTexture(0, uiOverlaySampler, uiOverlayTexture);

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientVertexBuffer(4, uiOverlayLayout) < 4
        || bgfx::getAvailTransientIndexBuffer(6, false) < 6) {
        return;
    }
    bgfx::allocTransientVertexBuffer(&tvb, 4, uiOverlayLayout);
    bgfx::allocTransientIndexBuffer(&tib, 6, false);

    struct UiOverlayVertex {
        float x;
        float y;
        float u;
        float v;
        uint32_t abgr;
    };

    auto* verts = reinterpret_cast<UiOverlayVertex*>(tvb.data);
    const uint32_t color = 0xffffffff;
    verts[0] = {0.0f, 0.0f, 0.0f, 0.0f, color};
    verts[1] = {static_cast<float>(width), 0.0f, 1.0f, 0.0f, color};
    verts[2] = {static_cast<float>(width), static_cast<float>(height), 1.0f, 1.0f, color};
    verts[3] = {0.0f, static_cast<float>(height), 0.0f, 1.0f, color};

    uint16_t* indices = reinterpret_cast<uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::submit(kUiOverlayView, uiOverlayProgram);
}

void BgfxBackend::setBrightness(float brightness) {
    this->brightness = brightness;
}

void BgfxBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.position = position;
}

void BgfxBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.rotation = rotation;
}

void BgfxBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.scale = scale;
}

void BgfxBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.visible = visible;
}

void BgfxBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.transparent = transparency;
}

void BgfxBackend::setOverlay(graphics::EntityId entity, bool overlay) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.overlay = overlay;
}

void BgfxBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
}

void BgfxBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
}

void BgfxBackend::setPerspective(float fovDeg, float aspect, float nearPlaneIn, float farPlaneIn) {
    usePerspective = true;
    fovDegrees = fovDeg;
    aspectRatio = aspect;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

void BgfxBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlaneIn, float farPlaneIn) {
    usePerspective = false;
    orthoLeft = left;
    orthoRight = right;
    orthoTop = top;
    orthoBottom = bottom;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

glm::mat4 BgfxBackend::computeViewMatrix() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPosition);
    return rotation * translation;
}

glm::mat4 BgfxBackend::computeProjectionMatrix() const {
    if (usePerspective) {
        return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    return glm::orthoRH_ZO(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
}

glm::mat4 BgfxBackend::getViewProjectionMatrix() const {
    return computeProjectionMatrix() * computeViewMatrix();
}

glm::mat4 BgfxBackend::getViewMatrix() const {
    return computeViewMatrix();
}

glm::mat4 BgfxBackend::getProjectionMatrix() const {
    return computeProjectionMatrix();
}

glm::vec3 BgfxBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 BgfxBackend::getCameraForward() const {
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

void BgfxBackend::buildTestResources() {
    if (!initialized || testReady) {
        return;
    }

    const std::filesystem::path shaderDir = getBgfxShaderDir();
    const std::filesystem::path vsPath = shaderDir / "vs_triangle.bin";
    const std::filesystem::path fsPath = shaderDir / "fs_triangle.bin";

    if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(fsPath)) {
        spdlog::error("Graphics(Bgfx): missing shader binaries '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) {
            bgfx::destroy(vsh);
        }
        if (bgfx::isValid(fsh)) {
            bgfx::destroy(fsh);
        }
        return;
    }
    testProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(testProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create shader program");
        return;
    }

    testLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    static const TestVertex verts[] = {
        {-0.6f, -0.4f, 0.0f, 0xff0000ff},
        {0.6f, -0.4f, 0.0f, 0xff00ff00},
        {0.0f, 0.6f, 0.0f, 0xffff0000},
    };
    static const uint16_t indices[] = {0, 1, 2};

    testVertexBuffer = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), testLayout);
    testIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices, sizeof(indices)));

    testReady = bgfx::isValid(testVertexBuffer) && bgfx::isValid(testIndexBuffer);
    if (!testReady) {
        spdlog::error("Graphics(Bgfx): failed to create test geometry");
    }
}

void BgfxBackend::buildMeshResources() {
    if (!initialized || meshReady) {
        return;
    }

    const std::filesystem::path shaderDir = getBgfxShaderDir("mesh");
    const std::filesystem::path vsPath = shaderDir / "vs_mesh.bin";
    const std::filesystem::path fsPath = shaderDir / "fs_mesh.bin";
    if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(fsPath)) {
        spdlog::error("Graphics(Bgfx): missing mesh shader binaries '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) {
            bgfx::destroy(vsh);
        }
        if (bgfx::isValid(fsh)) {
            bgfx::destroy(fsh);
        }
        return;
    }

    meshProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(meshProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create mesh shader program");
        return;
    }

    meshColorUniform = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
    meshSamplerUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    meshLightDirUniform = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    meshLightColorUniform = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    meshAmbientColorUniform = bgfx::createUniform("u_ambientColor", bgfx::UniformType::Vec4);
    meshUnlitUniform = bgfx::createUniform("u_unlit", bgfx::UniformType::Vec4);
    meshLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    const uint32_t whitePixel = 0xffffffffu;
    whiteTexture = createTextureRGBA8(1, 1, reinterpret_cast<const uint8_t*>(&whitePixel));

    meshReady = true;
}

void BgfxBackend::buildSkyboxResources() {
    if (!initialized || skyboxReady) {
        return;
    }

    const std::string mode = karma::config::ReadRequiredStringConfig("graphics.skybox.Mode");
    spdlog::trace("Graphics(Bgfx): skybox mode='{}'", mode);
    if (mode != "cubemap") {
        return;
    }

    const std::string name = karma::config::ReadRequiredStringConfig("graphics.skybox.Cubemap.Name");
    spdlog::trace("Graphics(Bgfx): skybox cubemap='{}'", name);
    const std::array<std::string, 6> faces = {"right", "left", "up", "down", "front", "back"};
    std::array<std::vector<uint8_t>, 6> facePixels{};
    int faceWidth = 0;
    int faceHeight = 0;

    for (size_t i = 0; i < faces.size(); ++i) {
        const std::filesystem::path facePath = skyboxPathFor(name, faces[i]);
        spdlog::trace("Graphics(Bgfx): loading skybox face '{}'", facePath.string());
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(facePath.string().c_str(), &width, &height, &channels, 4);
        if (!pixels || width <= 0 || height <= 0) {
            spdlog::warn("Graphics(Bgfx): failed to load skybox face '{}'", facePath.string());
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
            spdlog::warn("Graphics(Bgfx): skybox faces have mismatched dimensions");
            return;
        }

        facePixels[i].assign(pixels, pixels + static_cast<size_t>(width * height * 4));
        stbi_image_free(pixels);
    }

    skyboxTexture = createCubemapRGBA8(faceWidth, faceHeight, facePixels);
    if (!bgfx::isValid(skyboxTexture)) {
        spdlog::warn("Graphics(Bgfx): failed to create skybox cubemap");
        return;
    }
    spdlog::trace("Graphics(Bgfx): skybox cubemap created {}x{}", faceWidth, faceHeight);

    const std::filesystem::path shaderDir = getBgfxShaderDir("skybox");
    const std::filesystem::path vsPath = shaderDir / "vs_skybox.bin";
    const std::filesystem::path fsPath = shaderDir / "fs_skybox.bin";
    if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(fsPath)) {
        spdlog::error("Graphics(Bgfx): missing skybox shader binaries '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) {
            bgfx::destroy(vsh);
        }
        if (bgfx::isValid(fsh)) {
            bgfx::destroy(fsh);
        }
        return;
    }

    skyboxProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(skyboxProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create skybox shader program");
        return;
    }

    skyboxSamplerUniform = bgfx::createUniform("s_skybox", bgfx::UniformType::Sampler);
    skyboxLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

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

    skyboxVertexBuffer = bgfx::createVertexBuffer(bgfx::copy(cubeVerts, sizeof(cubeVerts)), skyboxLayout);
    skyboxReady = bgfx::isValid(skyboxVertexBuffer) && bgfx::isValid(skyboxProgram);
    spdlog::trace("Graphics(Bgfx): skybox ready={}", skyboxReady);
}

void BgfxBackend::ensureUiOverlayResources() {
    if (!initialized || bgfx::isValid(uiOverlayProgram)) {
        return;
    }
    const std::filesystem::path shaderDir = karma::data::Resolve("bgfx/shaders/bin/vk/imgui");
    const auto vsPath = shaderDir / "vs_imgui.bin";
    const auto fsPath = shaderDir / "fs_imgui.bin";
    auto vsBytes = karma::file::ReadFileBytes(vsPath);
    auto fsBytes = karma::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Bgfx): missing UI overlay shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }
    const bgfx::Memory* vsMem = bgfx::copy(vsBytes.data(), static_cast<uint32_t>(vsBytes.size()));
    const bgfx::Memory* fsMem = bgfx::copy(fsBytes.data(), static_cast<uint32_t>(fsBytes.size()));
    bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
    uiOverlayProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(uiOverlayProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create UI overlay shader program");
        return;
    }

    uiOverlaySampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    uiOverlayScaleBias = bgfx::createUniform("u_scaleBias", bgfx::UniformType::Vec4);
    uiOverlayLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
}

void BgfxBackend::ensureBrightnessResources() {
    if (!initialized || bgfx::isValid(brightnessProgram)) {
        return;
    }
    const std::filesystem::path shaderDir = getBgfxShaderDir("brightness");
    const auto vsPath = shaderDir / "vs_brightness.bin";
    const auto fsPath = shaderDir / "fs_brightness.bin";
    auto vsBytes = karma::file::ReadFileBytes(vsPath);
    auto fsBytes = karma::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Bgfx): missing brightness shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }
    const bgfx::Memory* vsMem = bgfx::copy(vsBytes.data(), static_cast<uint32_t>(vsBytes.size()));
    const bgfx::Memory* fsMem = bgfx::copy(fsBytes.data(), static_cast<uint32_t>(fsBytes.size()));
    bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
    brightnessProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(brightnessProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create brightness shader program");
        return;
    }

    brightnessSampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    brightnessScaleBias = bgfx::createUniform("u_scaleBias", bgfx::UniformType::Vec4);
    brightnessValue = bgfx::createUniform("u_brightness", bgfx::UniformType::Vec4);
    brightnessLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
}

void BgfxBackend::ensureSceneTarget(int width, int height) {
    if (!initialized) {
        return;
    }
    if (sceneTargetValid && sceneTarget.desc.width == width && sceneTarget.desc.height == height) {
        return;
    }
    destroySceneTarget();
    sceneTarget.desc.width = width;
    sceneTarget.desc.height = height;
    sceneTarget.desc.depth = true;
    if (width <= 0 || height <= 0) {
        sceneTargetValid = false;
        return;
    }
    const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    sceneTarget.colorTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        colorFlags);

    const uint64_t depthFlags = BGFX_TEXTURE_RT;
    sceneTarget.depthTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::D24,
        depthFlags);

    bgfx::Attachment attachments[2];
    uint8_t attachmentCount = 0;
    if (bgfx::isValid(sceneTarget.colorTexture)) {
        attachments[attachmentCount++].init(sceneTarget.colorTexture);
    }
    if (bgfx::isValid(sceneTarget.depthTexture)) {
        attachments[attachmentCount++].init(sceneTarget.depthTexture);
    }
    if (attachmentCount > 0) {
        sceneTarget.frameBuffer = bgfx::createFrameBuffer(attachmentCount, attachments, false);
    }
    sceneTargetValid = bgfx::isValid(sceneTarget.frameBuffer);
}

void BgfxBackend::destroySceneTarget() {
    if (!sceneTargetValid) {
        return;
    }
    if (bgfx::isValid(sceneTarget.frameBuffer)) {
        bgfx::destroy(sceneTarget.frameBuffer);
    }
    if (bgfx::isValid(sceneTarget.colorTexture)) {
        bgfx::destroy(sceneTarget.colorTexture);
    }
    if (bgfx::isValid(sceneTarget.depthTexture)) {
        bgfx::destroy(sceneTarget.depthTexture);
    }
    sceneTarget = {};
    sceneTargetValid = false;
}

} // namespace graphics_backend

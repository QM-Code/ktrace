#pragma once

#include "karma/graphics/backend.hpp"
#include <bgfx/bgfx.h>
#include <unordered_map>

namespace graphics_backend {

enum class BgfxRendererPreference {
    Auto,
    Vulkan,
};

void SetBgfxRendererPreference(BgfxRendererPreference preference);

class BgfxBackend final : public Backend {
public:
    explicit BgfxBackend(platform::Window& window);
    ~BgfxBackend() override;

    void beginFrame() override;
    void endFrame() override;
    void resize(int width, int height) override;

    graphics::EntityId createEntity(graphics::LayerId layer) override;
    graphics::EntityId createModelEntity(const std::filesystem::path& modelPath,
                                         graphics::LayerId layer,
                                         graphics::MaterialId materialOverride) override;
    graphics::EntityId createMeshEntity(graphics::MeshId mesh,
                                        graphics::LayerId layer,
                                        graphics::MaterialId materialOverride) override;
    void setEntityModel(graphics::EntityId entity,
                        const std::filesystem::path& modelPath,
                        graphics::MaterialId materialOverride) override;
    void setEntityMesh(graphics::EntityId entity,
                       graphics::MeshId mesh,
                       graphics::MaterialId materialOverride) override;
    void destroyEntity(graphics::EntityId entity) override;

    graphics::MeshId createMesh(const graphics::MeshData& mesh) override;
    void destroyMesh(graphics::MeshId mesh) override;

    graphics::MaterialId createMaterial(const graphics::MaterialDesc& material) override;
    void updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) override;
    void destroyMaterial(graphics::MaterialId material) override;
    void setMaterialFloat(graphics::MaterialId material, std::string_view name, float value) override;

    graphics::RenderTargetId createRenderTarget(const graphics::RenderTargetDesc& desc) override;
    void destroyRenderTarget(graphics::RenderTargetId target) override;

    void renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) override;

    unsigned int getRenderTargetTextureId(graphics::RenderTargetId target) const override;
    void setUiOverlayTexture(const graphics::TextureHandle& texture) override;
    void setUiOverlayVisible(bool visible) override;
    void renderUiOverlay() override;
    void setBrightness(float brightness) override;

    void setPosition(graphics::EntityId entity, const glm::vec3& position) override;
    void setRotation(graphics::EntityId entity, const glm::quat& rotation) override;
    void setScale(graphics::EntityId entity, const glm::vec3& scale) override;
    void setVisible(graphics::EntityId entity, bool visible) override;
    void setTransparency(graphics::EntityId entity, bool transparency) override;
    void setOverlay(graphics::EntityId entity, bool overlay) override;

    void setCameraPosition(const glm::vec3& position) override;
    void setCameraRotation(const glm::quat& rotation) override;
    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) override;
    void setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) override;

    glm::mat4 getViewProjectionMatrix() const override;
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    glm::vec3 getCameraPosition() const override;
    glm::vec3 getCameraForward() const override;
private:
    struct EntityRecord {
        graphics::LayerId layer = 0;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        bool visible = true;
        bool transparent = false;
        bool overlay = false;
        graphics::MeshId mesh = graphics::kInvalidMesh;
        std::vector<graphics::MeshId> meshes;
        graphics::MaterialId material = graphics::kInvalidMaterial;
        std::filesystem::path modelPath;
    };

    struct MeshRecord {
        bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
        uint32_t indexCount = 0;
        bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
        bool isWorldGrass = false;
    };

    struct RenderTargetRecord {
        graphics::RenderTargetDesc desc{};
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
    };

    platform::Window* window = nullptr;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    bool initialized = false;

    graphics::EntityId nextEntityId = 1;
    graphics::MeshId nextMeshId = 1;
    graphics::MaterialId nextMaterialId = 1;
    graphics::RenderTargetId nextRenderTargetId = 1;

    std::unordered_map<graphics::EntityId, EntityRecord> entities;
    std::unordered_map<graphics::MeshId, MeshRecord> meshes;
    std::unordered_map<graphics::MaterialId, graphics::MaterialDesc> materials;
    std::unordered_map<graphics::RenderTargetId, RenderTargetRecord> renderTargets;
    std::unordered_map<std::string, std::vector<graphics::MeshId>> modelMeshCache;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool usePerspective = true;
    float fovDegrees = 60.0f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float orthoLeft = -1.0f;
    float orthoRight = 1.0f;
    float orthoTop = 1.0f;
    float orthoBottom = -1.0f;

    bgfx::VertexLayout testLayout{};
    bgfx::VertexBufferHandle testVertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle testIndexBuffer = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle testProgram = BGFX_INVALID_HANDLE;
    bool testReady = false;

    bgfx::VertexLayout meshLayout{};
    bgfx::ProgramHandle meshProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshSamplerUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshLightDirUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshLightColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshAmbientColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle meshUnlitUniform = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle whiteTexture = BGFX_INVALID_HANDLE;
    std::unordered_map<std::string, bgfx::TextureHandle> textureCache;
    bool meshReady = false;
    std::string themeName;

    bgfx::VertexLayout skyboxLayout{};
    bgfx::VertexBufferHandle skyboxVertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skyboxProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle skyboxSamplerUniform = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle skyboxTexture = BGFX_INVALID_HANDLE;
    bool skyboxReady = false;
    bgfx::ProgramHandle uiOverlayProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uiOverlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uiOverlayScaleBias = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout uiOverlayLayout{};
    bgfx::TextureHandle uiOverlayTexture = BGFX_INVALID_HANDLE;
    uint32_t uiOverlayWidth = 0;
    uint32_t uiOverlayHeight = 0;
    bool uiOverlayVisible = false;

    uint64_t configRevision = 0;
    glm::vec3 cachedSunDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 cachedAmbientColor = glm::vec3(0.2f);
    glm::vec3 cachedSunColor = glm::vec3(1.0f);

    float brightness = 1.0f;
    RenderTargetRecord sceneTarget{};
    bool sceneTargetValid = false;
    bgfx::ProgramHandle brightnessProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle brightnessSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle brightnessScaleBias = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle brightnessValue = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout brightnessLayout{};

    glm::mat4 computeViewMatrix() const;
    glm::mat4 computeProjectionMatrix() const;
    void buildTestResources();
    void buildMeshResources();
    void buildSkyboxResources();
    void ensureUiOverlayResources();
    void ensureBrightnessResources();
    void ensureSceneTarget(int width, int height);
    void destroySceneTarget();

};

} // namespace graphics_backend

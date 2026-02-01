#pragma once

#include "karma/graphics/backend.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <unordered_map>

namespace Diligent {
class IRenderDevice;
class IDeviceContext;
class ISwapChain;
class IPipelineState;
class IShaderResourceBinding;
class IBuffer;
class ITexture;
class ITextureView;
} // namespace Diligent

namespace graphics_backend {

class DiligentBackend final : public Backend {
public:
    explicit DiligentBackend(platform::Window& window);
    ~DiligentBackend() override;

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
        graphics::MaterialId material = graphics::kInvalidMaterial;
        std::filesystem::path modelPath;
        std::vector<graphics::MeshId> meshes;
    };

    struct MeshRecord {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
        uint32_t indexCount = 0;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::ITextureView* srv = nullptr;
        bool isWorldGrass = false;
    };

    struct RenderTargetRecord {
        graphics::RenderTargetDesc desc{};
        Diligent::RefCntAutoPtr<Diligent::ITexture> colorTexture;
        Diligent::RefCntAutoPtr<Diligent::ITexture> depthTexture;
        Diligent::ITextureView* rtv = nullptr;
        Diligent::ITextureView* dsv = nullptr;
        Diligent::ITextureView* srv = nullptr;
        uint64_t srvToken = 0;
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
    std::unordered_map<std::string, Diligent::RefCntAutoPtr<Diligent::ITexture>> textureCache;
    Diligent::RefCntAutoPtr<Diligent::ITexture> whiteTexture_;
    Diligent::ITextureView* whiteTextureView_ = nullptr;
    std::string themeName;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapChain_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBinding_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipelineOffscreen_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBindingOffscreen_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipelineOverlay_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBindingOverlay_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipelineOverlayOffscreen_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBindingOverlayOffscreen_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constantBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> skyboxPipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> skyboxBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> skyboxConstantBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> skyboxVertexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> skyboxTexture_;
    Diligent::ITextureView* skyboxSrv_ = nullptr;
    bool skyboxReady = false;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> uiOverlayPipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> uiOverlayBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> uiOverlayVertexBuffer_;
    uint64_t uiOverlayToken_ = 0;
    uint32_t uiOverlayWidth_ = 0;
    uint32_t uiOverlayHeight_ = 0;
    bool uiOverlayVisible_ = false;

    float brightness_ = 1.0f;
    RenderTargetRecord sceneTarget_;
    bool sceneTargetValid_ = false;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> brightnessPipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> brightnessBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> brightnessVertexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> brightnessConstantBuffer_;


    uint64_t configRevision_ = 0;

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

    void initDiligent();
    void ensurePipeline();
    void updateSwapChain(int width, int height);
    void buildSkyboxResources();
    void ensureUiOverlayPipeline();
    void ensureBrightnessPipeline();
    void ensureSceneTarget(int width, int height);
    void destroySceneTarget();
    void renderBrightnessPass();
    void renderToTargets(Diligent::ITextureView* rtv,
                         Diligent::ITextureView* dsv,
                         graphics::LayerId layer,
                         int targetWidth,
                         int targetHeight,
                         bool drawSkybox,
                         bool offscreenPass);

    glm::mat4 computeViewMatrix() const;
    glm::mat4 computeProjectionMatrix() const;
};

} // namespace graphics_backend

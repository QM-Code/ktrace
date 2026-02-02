#pragma once

#include "karma/graphics/backend.hpp"
#include <functional>

namespace graphics {

class GraphicsDevice {

public:
    explicit GraphicsDevice(platform::Window& window);
    ~GraphicsDevice();

    void beginFrame();
    void endFrame();
    void resize(int width, int height);

    EntityId createEntity(LayerId layer = 0);
    EntityId createModelEntity(const std::filesystem::path& modelPath,
                               LayerId layer = 0,
                               MaterialId materialOverride = kInvalidMaterial);
    EntityId createMeshEntity(MeshId mesh,
                              LayerId layer = 0,
                              MaterialId materialOverride = kInvalidMaterial);
    void setEntityModel(EntityId entity,
                        const std::filesystem::path& modelPath,
                        MaterialId materialOverride = kInvalidMaterial);
    void setEntityMesh(EntityId entity,
                       MeshId mesh,
                       MaterialId materialOverride = kInvalidMaterial);
    void destroyEntity(EntityId entity);

    MeshId createMesh(const MeshData& mesh);
    void destroyMesh(MeshId mesh);

    MaterialId createMaterial(const MaterialDesc& material);
    void updateMaterial(MaterialId material, const MaterialDesc& desc);
    void destroyMaterial(MaterialId material);
    void setMaterialFloat(MaterialId material, std::string_view name, float value);

    RenderTargetId createRenderTarget(const RenderTargetDesc& desc);
    void destroyRenderTarget(RenderTargetId target);

    void renderLayer(LayerId layer, RenderTargetId target = kDefaultRenderTarget);

    unsigned int getRenderTargetTextureId(RenderTargetId target) const;

    void setUiOverlayTexture(const TextureHandle& texture);
    void setUiOverlayVisible(bool visible);
    void renderUiOverlay();
    void renderUiDrawData(const karma::app::UIDrawData& drawData,
                          const std::function<bool(karma::app::UITextureHandle, graphics::TextureHandle&)>& resolveTexture,
                          int viewportW,
                          int viewportH,
                          float dpiScale);
    void setBrightness(float brightness);

    void setPosition(EntityId entity, const glm::vec3& position);
    void setRotation(EntityId entity, const glm::quat& rotation);
    void setScale(EntityId entity, const glm::vec3& scale);
    void setVisible(EntityId entity, bool visible);
    void setTransparency(EntityId entity, bool transparency);
    void setOverlay(EntityId entity, bool overlay);

    void setCameraPosition(const glm::vec3& position);
    void setCameraRotation(const glm::quat& rotation);
    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    void setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane);

    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPosition() const;
    glm::vec3 getCameraForward() const;

private:
    std::unique_ptr<graphics_backend::Backend> backend_;
};

} // namespace graphics

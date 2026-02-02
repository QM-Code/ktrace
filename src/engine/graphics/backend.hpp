#pragma once

#include "karma/graphics/texture_handle.hpp"
#include "karma/graphics/types.hpp"
#include "karma/app/ui_draw_data.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace platform {
class Window;
}

namespace graphics_backend {

class Backend {
public:
    virtual ~Backend() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void resize(int width, int height) = 0;

    virtual graphics::EntityId createEntity(graphics::LayerId layer) = 0;
    virtual graphics::EntityId createModelEntity(const std::filesystem::path& modelPath,
                                                 graphics::LayerId layer,
                                                 graphics::MaterialId materialOverride) = 0;
    virtual graphics::EntityId createMeshEntity(graphics::MeshId mesh,
                                                graphics::LayerId layer,
                                                graphics::MaterialId materialOverride) = 0;
    virtual void setEntityModel(graphics::EntityId entity,
                                const std::filesystem::path& modelPath,
                                graphics::MaterialId materialOverride) = 0;
    virtual void setEntityMesh(graphics::EntityId entity,
                               graphics::MeshId mesh,
                               graphics::MaterialId materialOverride) = 0;
    virtual void destroyEntity(graphics::EntityId entity) = 0;

    virtual graphics::MeshId createMesh(const graphics::MeshData& mesh) = 0;
    virtual void destroyMesh(graphics::MeshId mesh) = 0;

    virtual graphics::MaterialId createMaterial(const graphics::MaterialDesc& material) = 0;
    virtual void updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) = 0;
    virtual void destroyMaterial(graphics::MaterialId material) = 0;
    virtual void setMaterialFloat(graphics::MaterialId material, std::string_view name, float value) = 0;

    virtual graphics::RenderTargetId createRenderTarget(const graphics::RenderTargetDesc& desc) = 0;
    virtual void destroyRenderTarget(graphics::RenderTargetId target) = 0;

    virtual void renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) = 0;

    virtual unsigned int getRenderTargetTextureId(graphics::RenderTargetId target) const = 0;

    virtual void setUiOverlayTexture(const graphics::TextureHandle& texture) { (void)texture; }
    virtual void setUiOverlayVisible(bool visible) { (void)visible; }
    virtual void renderUiOverlay() {}
    virtual void renderUiDrawData(const karma::app::UIDrawData& drawData,
                                  const std::function<bool(karma::app::UITextureHandle, graphics::TextureHandle&)>& resolveTexture,
                                  int viewportW,
                                  int viewportH,
                                  float dpiScale) {
        (void)drawData;
        (void)resolveTexture;
        (void)viewportW;
        (void)viewportH;
        (void)dpiScale;
    }
    virtual void setBrightness(float brightness) { (void)brightness; }

    virtual void setPosition(graphics::EntityId entity, const glm::vec3& position) = 0;
    virtual void setRotation(graphics::EntityId entity, const glm::quat& rotation) = 0;
    virtual void setScale(graphics::EntityId entity, const glm::vec3& scale) = 0;
    virtual void setVisible(graphics::EntityId entity, bool visible) = 0;
    virtual void setTransparency(graphics::EntityId entity, bool transparency) = 0;
    virtual void setOverlay(graphics::EntityId entity, bool overlay) = 0;

    virtual void setCameraPosition(const glm::vec3& position) = 0;
    virtual void setCameraRotation(const glm::quat& rotation) = 0;
    virtual void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) = 0;
    virtual void setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) = 0;

    virtual glm::mat4 getViewProjectionMatrix() const = 0;
    virtual glm::mat4 getViewMatrix() const = 0;
    virtual glm::mat4 getProjectionMatrix() const = 0;
    virtual glm::vec3 getCameraPosition() const = 0;
    virtual glm::vec3 getCameraForward() const = 0;
};

std::unique_ptr<Backend> CreateGraphicsBackend(platform::Window& window);

} // namespace graphics_backend

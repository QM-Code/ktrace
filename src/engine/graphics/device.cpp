#include "karma/graphics/device.hpp"

#include <algorithm>

namespace graphics {

GraphicsDevice::GraphicsDevice(platform::Window& window) {
    backend_ = graphics_backend::CreateGraphicsBackend(window);
}

GraphicsDevice::~GraphicsDevice() = default;

void GraphicsDevice::beginFrame() {
    if (backend_) {
        backend_->beginFrame();
    }
}

void GraphicsDevice::endFrame() {
    if (backend_) {
        backend_->endFrame();
    }
}

void GraphicsDevice::resize(int width, int height) {
    if (backend_) {
        backend_->resize(width, height);
    }
}

EntityId GraphicsDevice::createEntity(LayerId layer) {
    return backend_ ? backend_->createEntity(layer) : kInvalidEntity;
}

EntityId GraphicsDevice::createModelEntity(const std::filesystem::path& modelPath,
                                           LayerId layer,
                                           MaterialId materialOverride) {
    return backend_ ? backend_->createModelEntity(modelPath, layer, materialOverride) : kInvalidEntity;
}

EntityId GraphicsDevice::createMeshEntity(MeshId mesh, LayerId layer, MaterialId materialOverride) {
    return backend_ ? backend_->createMeshEntity(mesh, layer, materialOverride) : kInvalidEntity;
}

void GraphicsDevice::setEntityModel(EntityId entity,
                                    const std::filesystem::path& modelPath,
                                    MaterialId materialOverride) {
    if (backend_) {
        backend_->setEntityModel(entity, modelPath, materialOverride);
    }
}

void GraphicsDevice::setEntityMesh(EntityId entity, MeshId mesh, MaterialId materialOverride) {
    if (backend_) {
        backend_->setEntityMesh(entity, mesh, materialOverride);
    }
}

void GraphicsDevice::destroyEntity(EntityId entity) {
    if (backend_) {
        backend_->destroyEntity(entity);
    }
}

MeshId GraphicsDevice::createMesh(const MeshData& mesh) {
    return backend_ ? backend_->createMesh(mesh) : kInvalidMesh;
}

void GraphicsDevice::destroyMesh(MeshId mesh) {
    if (backend_) {
        backend_->destroyMesh(mesh);
    }
}

MaterialId GraphicsDevice::createMaterial(const MaterialDesc& material) {
    return backend_ ? backend_->createMaterial(material) : kInvalidMaterial;
}

void GraphicsDevice::updateMaterial(MaterialId material, const MaterialDesc& desc) {
    if (backend_) {
        backend_->updateMaterial(material, desc);
    }
}

void GraphicsDevice::destroyMaterial(MaterialId material) {
    if (backend_) {
        backend_->destroyMaterial(material);
    }
}

void GraphicsDevice::setMaterialFloat(MaterialId material, std::string_view name, float value) {
    if (backend_) {
        backend_->setMaterialFloat(material, name, value);
    }
}

RenderTargetId GraphicsDevice::createRenderTarget(const RenderTargetDesc& desc) {
    return backend_ ? backend_->createRenderTarget(desc) : kDefaultRenderTarget;
}

void GraphicsDevice::destroyRenderTarget(RenderTargetId target) {
    if (backend_) {
        backend_->destroyRenderTarget(target);
    }
}

void GraphicsDevice::renderLayer(LayerId layer, RenderTargetId target) {
    if (backend_) {
        backend_->renderLayer(layer, target);
    }
}

unsigned int GraphicsDevice::getRenderTargetTextureId(RenderTargetId target) const {
    return backend_ ? backend_->getRenderTargetTextureId(target) : 0u;
}

void GraphicsDevice::setUiOverlayTexture(const TextureHandle& texture) {
    if (backend_) {
        backend_->setUiOverlayTexture(texture);
    }
}

void GraphicsDevice::setUiOverlayVisible(bool visible) {
    if (backend_) {
        backend_->setUiOverlayVisible(visible);
    }
}

void GraphicsDevice::renderUiOverlay() {
    if (backend_) {
        backend_->renderUiOverlay();
    }
}

void GraphicsDevice::renderUiDrawData(const karma::app::UIDrawData& drawData,
                                      const std::function<bool(karma::app::UITextureHandle, graphics::TextureHandle&)>& resolveTexture,
                                      int viewportW,
                                      int viewportH,
                                      float dpiScale) {
    if (backend_) {
        backend_->renderUiDrawData(drawData, resolveTexture, viewportW, viewportH, dpiScale);
    }
}

void GraphicsDevice::setBrightness(float brightness) {
    if (backend_) {
        const float clamped = std::clamp(brightness, 0.2f, 3.0f);
        backend_->setBrightness(clamped);
    }
}

void GraphicsDevice::setPosition(EntityId entity, const glm::vec3& position) {
    if (backend_) {
        backend_->setPosition(entity, position);
    }
}

void GraphicsDevice::setRotation(EntityId entity, const glm::quat& rotation) {
    if (backend_) {
        backend_->setRotation(entity, rotation);
    }
}

void GraphicsDevice::setScale(EntityId entity, const glm::vec3& scale) {
    if (backend_) {
        backend_->setScale(entity, scale);
    }
}

void GraphicsDevice::setVisible(EntityId entity, bool visible) {
    if (backend_) {
        backend_->setVisible(entity, visible);
    }
}

void GraphicsDevice::setTransparency(EntityId entity, bool transparency) {
    if (backend_) {
        backend_->setTransparency(entity, transparency);
    }
}

void GraphicsDevice::setOverlay(EntityId entity, bool overlay) {
    if (backend_) {
        backend_->setOverlay(entity, overlay);
    }
}

void GraphicsDevice::setCameraPosition(const glm::vec3& position) {
    if (backend_) {
        backend_->setCameraPosition(position);
    }
}

void GraphicsDevice::setCameraRotation(const glm::quat& rotation) {
    if (backend_) {
        backend_->setCameraRotation(rotation);
    }
}

void GraphicsDevice::setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    if (backend_) {
        backend_->setPerspective(fovDegrees, aspect, nearPlane, farPlane);
    }
}

void GraphicsDevice::setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) {
    if (backend_) {
        backend_->setOrthographic(left, right, top, bottom, nearPlane, farPlane);
    }
}

glm::mat4 GraphicsDevice::getViewProjectionMatrix() const {
    return backend_ ? backend_->getViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 GraphicsDevice::getViewMatrix() const {
    return backend_ ? backend_->getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 GraphicsDevice::getProjectionMatrix() const {
    return backend_ ? backend_->getProjectionMatrix() : glm::mat4(1.0f);
}

glm::vec3 GraphicsDevice::getCameraPosition() const {
    return backend_ ? backend_->getCameraPosition() : glm::vec3(0.0f);
}

glm::vec3 GraphicsDevice::getCameraForward() const {
    return backend_ ? backend_->getCameraForward() : glm::vec3(0.0f, 0.0f, -1.0f);
}

} // namespace graphics

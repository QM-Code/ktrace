#pragma once

#include "karma/graphics/device.hpp"
#include "karma/graphics/texture_handle.hpp"
#include "karma/renderer/renderer_context.hpp"
#include "karma/app/ui_context.h"
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::renderer {

class SceneRenderer {
public:
    explicit SceneRenderer(graphics::GraphicsDevice &device) : device_(&device) {}

    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) {
        device_->setPerspective(fov, aspect, nearPlane, farPlane);
    }

    void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
        device_->setOrthographic(left, right, bottom, top, nearPlane, farPlane);
    }

    void setCameraPosition(const glm::vec3 &position) {
        device_->setCameraPosition(position);
    }

    void setCameraRotation(const glm::quat &rotation) {
        device_->setCameraRotation(rotation);
    }

    void renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
        device_->renderLayer(layer, target);
    }

    void renderMain(const RendererContext &context) {
        setPerspective(context.fov, context.aspect, context.nearPlane, context.farPlane);
        setCameraPosition(context.cameraPosition);
        setCameraRotation(context.cameraRotation);
        renderLayer(context.mainLayer, context.mainTarget);
    }

    glm::mat4 getViewProjectionMatrix() const { return device_->getViewProjectionMatrix(); }
    glm::mat4 getViewMatrix() const { return device_->getViewMatrix(); }
    glm::mat4 getProjectionMatrix() const { return device_->getProjectionMatrix(); }
    glm::vec3 getCameraPosition() const { return device_->getCameraPosition(); }
    glm::vec3 getCameraForward() const { return device_->getCameraForward(); }

    void beginFrame() {
        device_->beginFrame();
    }

    void endFrame() {
        device_->endFrame();
    }

    void resize(int width, int height) {
        device_->resize(width, height);
    }

    void setUiOverlayTexture(const graphics::TextureHandle &texture, bool visible) {
        device_->setUiOverlayTexture(texture);
        device_->setUiOverlayVisible(visible);
    }

    void renderUiOverlay() {
        device_->renderUiOverlay();
    }

    void renderUi(const karma::app::UIContext &ui_context) {
        const auto &draw_data = ui_context.drawData();
        if (draw_data.commands.empty()) {
            return;
        }
        const auto resolve = [&ui_context](karma::app::UITextureHandle handle,
                                           graphics::TextureHandle &out) {
            return ui_context.resolveExternalTexture(handle, out);
        };
        device_->renderUiDrawData(draw_data,
                                  resolve,
                                  ui_context.frame().viewport_w,
                                  ui_context.frame().viewport_h,
                                  ui_context.frame().dpi_scale);
    }

    void setBrightness(float brightness) {
        device_->setBrightness(brightness);
    }

private:
    graphics::GraphicsDevice *device_;
};

} // namespace engine::renderer

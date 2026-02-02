#pragma once

#include "karma/graphics/device.hpp"
#include "karma/graphics/types.hpp"
#include "karma/renderer/scene_renderer.hpp"
#include "karma/core/types.hpp"
#include <filesystem>
#include <unordered_map>

namespace game::renderer {

struct RadarConfig {
    std::filesystem::path shaderVertex;
    std::filesystem::path shaderFragment;
    float fovDegrees = 60.0f;
};

class RadarRenderer {
public:
    RadarRenderer(graphics::GraphicsDevice &device, engine::renderer::SceneRenderer &scene);

    void configure(const RadarConfig& config);
    void ensureResources();
    void setFovDegrees(float fovDegrees);
    void updateFovLines(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation, float fovDegrees);
    void render(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation);

    void setModel(render_id id, const std::filesystem::path &modelPath, bool addToRadar);
    void setRadarCircleGraphic(render_id id, float radius);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void destroy(render_id id);

    graphics::TextureHandle getRadarTexture() const;

private:
    graphics::GraphicsDevice *device_;
    engine::renderer::SceneRenderer *scene_;

    graphics::LayerId radarLayer_ = 1;
    graphics::RenderTargetId radarTarget_ = graphics::kDefaultRenderTarget;
    graphics::MaterialId radarMaterial_ = graphics::kInvalidMaterial;
    graphics::MaterialId radarLineMaterial_ = graphics::kInvalidMaterial;
    graphics::MeshId radarCircleMesh_ = graphics::kInvalidMesh;
    graphics::MeshId radarBeamMesh_ = graphics::kInvalidMesh;
    graphics::EntityId radarFovLeft_ = graphics::kInvalidEntity;
    graphics::EntityId radarFovRight_ = graphics::kInvalidEntity;

    std::unordered_map<render_id, graphics::EntityId> radarEntities_;
    std::unordered_map<render_id, graphics::EntityId> radarCircles_;
    std::unordered_map<render_id, std::filesystem::path> modelPaths_;

    float radarFovDegrees_ = 60.0f;
    RadarConfig config_{};
    bool hasConfig_ = false;

    glm::quat computeRadarCameraRotation(const glm::vec3 &radarCamPos,
                                         const glm::vec3 &cameraPosition,
                                         const glm::quat &cameraRotation) const;
};

} // namespace game::renderer

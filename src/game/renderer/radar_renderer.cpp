#include "renderer/radar_renderer.hpp"

#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
constexpr float kRadarOrthoHalfSize = 40.0f;
constexpr float kRadarNear = 0.1f;
constexpr float kRadarFar = 500.0f;
constexpr float kRadarHeightAbovePlayer = 60.0f;
constexpr float kRadarBeamLength = 80.0f;
constexpr float kRadarBeamWidth = 0.3f;
constexpr int kRadarTexSize = 512 * 2;

graphics::MeshData makeDiskMesh(int segments = 64, float radius = 1.0f) {
    graphics::MeshData mesh;
    mesh.vertices.reserve(segments + 1);
    mesh.indices.reserve(segments * 3);

    mesh.vertices.emplace_back(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * static_cast<float>(M_PI);
        const float ct = std::cos(t);
        const float st = std::sin(t);
        mesh.vertices.emplace_back(ct * radius, 0.0f, st * radius);
    }

    for (int i = 0; i < segments; ++i) {
        const uint32_t center = 0;
        const uint32_t a = static_cast<uint32_t>(i + 1);
        const uint32_t b = static_cast<uint32_t>(((i + 1) % segments) + 1);
        mesh.indices.insert(mesh.indices.end(), {center, a, b});
    }

    return mesh;
}

graphics::MeshData makeBeamMesh() {
    graphics::MeshData mesh;
    mesh.vertices = {
        {-kRadarBeamWidth * 0.5f, 0.0f, 0.0f},
        { kRadarBeamWidth * 0.5f, 0.0f, 0.0f},
        { kRadarBeamWidth * 0.5f, 0.0f, -1.0f},
        {-kRadarBeamWidth * 0.5f, 0.0f, -1.0f}
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

} // namespace

namespace game::renderer {

RadarRenderer::RadarRenderer(graphics::GraphicsDevice &device, engine::renderer::SceneRenderer &scene)
    : device_(&device), scene_(&scene) {}

void RadarRenderer::configure(const RadarConfig& config) {
    config_ = config;
    hasConfig_ = true;
    radarFovDegrees_ = config.fovDegrees;
    ensureResources();
    if (radarMaterial_ != graphics::kInvalidMaterial && device_) {
        graphics::MaterialDesc desc;
        desc.vertexShaderPath = config_.shaderVertex;
        desc.fragmentShaderPath = config_.shaderFragment;
        desc.transparent = true;
        desc.depthTest = true;
        desc.depthWrite = false;
        desc.doubleSided = true;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        device_->updateMaterial(radarMaterial_, desc);
        device_->setMaterialFloat(radarMaterial_, "jumpHeight", 5.0f);
        for (const auto &entry : radarEntities_) {
            const auto &id = entry.first;
            const auto &entity = entry.second;
            auto pathIt = modelPaths_.find(id);
            if (pathIt != modelPaths_.end()) {
                device_->setEntityModel(entity, pathIt->second, radarMaterial_);
            }
        }
    }
}

void RadarRenderer::ensureResources() {
    if (!device_) {
        return;
    }

    const unsigned int existingTexId =
        (radarTarget_ == graphics::kDefaultRenderTarget) ? 0u : device_->getRenderTargetTextureId(radarTarget_);
    const bool needsRadarTarget = (radarTarget_ == graphics::kDefaultRenderTarget || existingTexId == 0u);
    if (needsRadarTarget) {
        if (radarTarget_ != graphics::kDefaultRenderTarget) {
            spdlog::warn("Radar RT invalid (target={} texId=0). Recreating.", radarTarget_);
        }
        if (radarTarget_ != graphics::kDefaultRenderTarget) {
            device_->destroyRenderTarget(radarTarget_);
        }
        graphics::RenderTargetDesc desc;
        desc.width = kRadarTexSize;
        desc.height = kRadarTexSize;
        desc.depth = true;
        desc.stencil = false;
        radarTarget_ = device_->createRenderTarget(desc);
        const unsigned int newTexId = device_->getRenderTargetTextureId(radarTarget_);
        if (newTexId == 0u) {
            spdlog::warn("Radar RT creation returned texId=0 (target={})", radarTarget_);
        }
    }

    if (radarMaterial_ == graphics::kInvalidMaterial) {
        graphics::MaterialDesc desc;
        if (hasConfig_) {
            desc.vertexShaderPath = config_.shaderVertex;
            desc.fragmentShaderPath = config_.shaderFragment;
        }
        desc.transparent = true;
        desc.depthTest = true;
        desc.depthWrite = false;
        desc.doubleSided = true;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        radarMaterial_ = device_->createMaterial(desc);
        device_->setMaterialFloat(radarMaterial_, "jumpHeight", 5.0f);
    }

    if (radarLineMaterial_ == graphics::kInvalidMaterial) {
        graphics::MaterialDesc desc;
        desc.unlit = true;
        desc.transparent = true;
        desc.depthTest = false;
        desc.depthWrite = false;
        desc.doubleSided = true;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        radarLineMaterial_ = device_->createMaterial(desc);
    }

    if (radarCircleMesh_ == graphics::kInvalidMesh) {
        radarCircleMesh_ = device_->createMesh(makeDiskMesh());
    }

    if (radarBeamMesh_ == graphics::kInvalidMesh) {
        radarBeamMesh_ = device_->createMesh(makeBeamMesh());
    }
}

void RadarRenderer::setFovDegrees(float fovDegrees) {
    radarFovDegrees_ = fovDegrees;
}

void RadarRenderer::updateFovLines(const glm::vec3 &cameraPosition,
                                   const glm::quat &cameraRotation,
                                   float fovDegrees) {
    if (!device_) {
        return;
    }

    radarFovDegrees_ = fovDegrees;
    ensureResources();

    if (radarFovLeft_ == graphics::kInvalidEntity) {
        radarFovLeft_ = device_->createMeshEntity(radarBeamMesh_, radarLayer_, radarLineMaterial_);
    }
    if (radarFovRight_ == graphics::kInvalidEntity) {
        radarFovRight_ = device_->createMeshEntity(radarBeamMesh_, radarLayer_, radarLineMaterial_);
    }
    device_->setOverlay(radarFovLeft_, true);
    device_->setOverlay(radarFovRight_, true);

    const float halfVertRad = glm::radians(radarFovDegrees_ * 0.5f);
    const float lineLength = kRadarBeamLength / std::max(0.05f, std::cos(halfVertRad));
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 flatForward(forward.x, 0.0f, forward.z);
    const float flatLen2 = glm::dot(flatForward, flatForward);
    if (flatLen2 < 0.0001f) {
        flatForward = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        flatForward = glm::normalize(flatForward);
    }
    const float yaw = std::atan2(flatForward.x, -flatForward.z);
    // Apply inverse yaw so the radar FOV lines stay fixed "up" in radar space as the player rotates.
    const glm::quat yawRot = glm::angleAxis(-yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat leftRot = yawRot * glm::angleAxis(-halfVertRad, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat rightRot = yawRot * glm::angleAxis(halfVertRad, glm::vec3(0.0f, 1.0f, 0.0f));

    device_->setRotation(radarFovLeft_, leftRot);
    device_->setPosition(radarFovLeft_, cameraPosition);
    device_->setScale(radarFovLeft_, glm::vec3(1.0f, 1.0f, lineLength));

    device_->setRotation(radarFovRight_, rightRot);
    device_->setPosition(radarFovRight_, cameraPosition);
    device_->setScale(radarFovRight_, glm::vec3(1.0f, 1.0f, lineLength));

}

glm::quat RadarRenderer::computeRadarCameraRotation(const glm::vec3 &radarCamPos,
                                                    const glm::vec3 &cameraPosition,
                                                    const glm::quat &cameraRotation) const {
    // Player-forward-up: rotate radar so player forward is "up" on the radar.
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 up = -forward;
    const glm::mat4 view = glm::lookAt(radarCamPos, cameraPosition, up);
    return glm::quat_cast(glm::inverse(view));
}

void RadarRenderer::render(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation) {
    ensureResources();
    const glm::vec3 radarCamPos = cameraPosition + glm::vec3(0.0f, kRadarHeightAbovePlayer, 0.0f);
    const glm::quat radarRotation = computeRadarCameraRotation(radarCamPos, cameraPosition, cameraRotation);
    updateFovLines(cameraPosition, cameraRotation, radarFovDegrees_);

    scene_->setOrthographic(kRadarOrthoHalfSize, -kRadarOrthoHalfSize,
                            kRadarOrthoHalfSize, -kRadarOrthoHalfSize,
                            kRadarNear, kRadarFar);
    scene_->setCameraPosition(radarCamPos);
    scene_->setCameraRotation(radarRotation);
    device_->setMaterialFloat(radarMaterial_, "playerY", cameraPosition.y);
    scene_->renderLayer(radarLayer_, radarTarget_);
}

void RadarRenderer::setModel(render_id id, const std::filesystem::path &modelPath, bool addToRadar) {
    modelPaths_[id] = modelPath;
    if (!addToRadar) {
        if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
            device_->destroyEntity(it->second);
            radarEntities_.erase(it);
        }
        return;
    }
    ensureResources();
    auto it = radarEntities_.find(id);
    if (it == radarEntities_.end()) {
        radarEntities_[id] = device_->createModelEntity(modelPath, radarLayer_, radarMaterial_);
    } else {
        device_->setEntityModel(it->second, modelPath, radarMaterial_);
    }
}

void RadarRenderer::setRadarCircleGraphic(render_id id, float radius) {
    ensureResources();
    auto it = radarCircles_.find(id);
    if (it == radarCircles_.end()) {
        radarCircles_[id] = device_->createMeshEntity(radarCircleMesh_, radarLayer_, radarLineMaterial_);
        it = radarCircles_.find(id);
    }
    device_->setOverlay(it->second, true);
    device_->setScale(it->second, glm::vec3(radius, 1.0f, radius));
}

void RadarRenderer::setPosition(render_id id, const glm::vec3 &position) {
    if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
        device_->setPosition(it->second, position);
    }
    if (auto it = radarCircles_.find(id); it != radarCircles_.end()) {
        device_->setPosition(it->second, position);
    }
    if (radarFovLeft_ != graphics::kInvalidEntity && radarFovRight_ != graphics::kInvalidEntity) {
        device_->setPosition(radarFovLeft_, position);
        device_->setPosition(radarFovRight_, position);
    }
}

void RadarRenderer::setRotation(render_id id, const glm::quat &rotation) {
    if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
        device_->setRotation(it->second, rotation);
    }
    if (auto it = radarCircles_.find(id); it != radarCircles_.end()) {
        device_->setRotation(it->second, rotation);
    }
}

void RadarRenderer::setScale(render_id id, const glm::vec3 &scale) {
    if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
        device_->setScale(it->second, scale);
    }
}

void RadarRenderer::setVisible(render_id id, bool visible) {
    if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
        device_->setVisible(it->second, visible);
    }
    if (auto it = radarCircles_.find(id); it != radarCircles_.end()) {
        device_->setVisible(it->second, visible);
    }
}

void RadarRenderer::destroy(render_id id) {
    if (auto it = radarEntities_.find(id); it != radarEntities_.end()) {
        device_->destroyEntity(it->second);
        radarEntities_.erase(it);
    }
    if (auto it = radarCircles_.find(id); it != radarCircles_.end()) {
        device_->destroyEntity(it->second);
        radarCircles_.erase(it);
    }
    modelPaths_.erase(id);
}

graphics::TextureHandle RadarRenderer::getRadarTexture() const {
    graphics::TextureHandle handle{};
    if (!device_) {
        return handle;
    }
    const unsigned int textureId = device_->getRenderTargetTextureId(radarTarget_);
    if (textureId == 0u && radarTarget_ != graphics::kDefaultRenderTarget) {
        static bool loggedMissing = false;
        if (!loggedMissing) {
            spdlog::warn("Radar RT texture id is 0 (target={}); radar will appear blank.", radarTarget_);
            loggedMissing = true;
        }
    }
    handle.id = static_cast<uint64_t>(textureId);
    handle.width = static_cast<uint32_t>(kRadarTexSize);
    handle.height = static_cast<uint32_t>(kRadarTexSize);
    return handle;
}

} // namespace game::renderer

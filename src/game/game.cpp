#include "game.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "karma/renderer/device.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace bz3 {

void Game::onStart() {
    if (!graphics) {
        return;
    }

    const auto tank_path = karma::data::Resolve("common/models/shot.glb");
    KARMA_TRACE("render.mesh", "Game: loading tank mesh '{}'", tank_path.string());
    tank_mesh_ = graphics->createMeshFromFile(tank_path);
    if (tank_mesh_ == karma::renderer::kInvalidMesh) {
        spdlog::error("Failed to load tank mesh from {}", tank_path.string());
    }

    karma::renderer::MaterialDesc material{};
    material.base_color = {0.8f, 0.8f, 0.8f, 1.0f};
    tank_material_ = graphics->createMaterial(material);

    karma::renderer::CameraData camera{};
    camera.position = {0.0f, 6.0f, 12.0f};
    camera.target = {0.0f, 1.0f, 0.0f};
    graphics->setCamera(camera);

    KARMA_TRACE("render.frame", "Game started; tank mesh={}, material={}", tank_mesh_, tank_material_);
}

void Game::onUpdate(float dt) {
    (void)dt;
    if (!graphics || tank_mesh_ == karma::renderer::kInvalidMesh) {
        return;
    }

    karma::renderer::DrawItem item{};
    item.mesh = tank_mesh_;
    item.material = tank_material_;
    item.transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    graphics->submit(item);
}

void Game::onShutdown() {
    if (!graphics) {
        return;
    }
    if (tank_mesh_ != karma::renderer::kInvalidMesh) {
        graphics->destroyMesh(tank_mesh_);
    }
    if (tank_material_ != karma::renderer::kInvalidMaterial) {
        graphics->destroyMaterial(tank_material_);
    }
}

} // namespace bz3

#include "game.hpp"

#include "geometry/mesh_loader.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "karma/input/input_system.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"

#include <stdexcept>


namespace bz3 {

Game::Game(std::string model_key) : model_key_(std::move(model_key)) {}

void Game::onStart() {
    if (!graphics || !render || !scene) {
        return;
    }

    if (model_key_.empty()) {
        throw std::runtime_error("Missing required render.model config");
    }
    const auto model_path = karma::data::Resolve(model_key_);
    KARMA_TRACE("render.mesh", "Game: loading model scene '{}'", model_path.string());
    std::vector<karma::geometry::SceneMesh> scene_meshes;
    if (!karma::geometry::LoadScene(model_path, scene_meshes)) {
        spdlog::error("Failed to load model scene from {}", model_path.string());
        return;
    }

    karma::renderer::MaterialDesc material{};
    material.base_color = {0.8f, 0.8f, 0.8f, 1.0f};
    model_material_ = graphics->createMaterial(material);

    model_entities_.clear();
    model_meshes_.clear();
    model_entities_.reserve(scene_meshes.size());
    model_meshes_.reserve(scene_meshes.size());

    for (const auto& entry : scene_meshes) {
        const auto mesh_id = graphics->createMesh(entry.mesh);
        if (mesh_id == karma::renderer::kInvalidMesh) {
            spdlog::error("Failed to create mesh for model {}", model_path.string());
            continue;
        }
        const auto entity = scene->createEntity();
        scene->setMesh(entity, mesh_id);
        scene->setMaterial(entity, model_material_);
        scene->setLayer(entity, karma::renderer::kLayerWorld);
        scene->setTransform(entity, entry.transform);
        model_meshes_.push_back(mesh_id);
        model_entities_.push_back(entity);
    }

    if (input) {
        input->setMode(karma::input::InputMode::Roaming);
    }

    KARMA_TRACE("render.frame", "Game started; model meshes={}, material={}",
                model_meshes_.size(), model_material_);
}

void Game::onUpdate(float dt) {
    if (!render || !input) {
        return;
    }
    (void)dt;
}

void Game::onShutdown() {
    if (!graphics) {
        return;
    }
    if (scene) {
        for (const auto entity : model_entities_) {
            scene->destroyEntity(entity);
        }
    }
    for (const auto mesh_id : model_meshes_) {
        if (mesh_id != karma::renderer::kInvalidMesh) {
            graphics->destroyMesh(mesh_id);
        }
    }
    model_meshes_.clear();
    if (model_material_ != karma::renderer::kInvalidMaterial) {
        graphics->destroyMaterial(model_material_);
        model_material_ = karma::renderer::kInvalidMaterial;
    }
    model_entities_.clear();
}

} // namespace bz3

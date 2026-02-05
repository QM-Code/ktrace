#pragma once

#include "karma/app/game_interface.hpp"
#include "karma/renderer/types.hpp"

namespace bz3 {

class Game final : public karma::app::GameInterface {
 public:
    void onStart() override;
    void onUpdate(float dt) override;
    void onShutdown() override;

 private:
    karma::renderer::MeshId tank_mesh_ = karma::renderer::kInvalidMesh;
    karma::renderer::MaterialId tank_material_ = karma::renderer::kInvalidMaterial;
};

} // namespace bz3

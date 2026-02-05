#pragma once

#include "karma/renderer/device.hpp"

#include <map>

namespace karma::ecs { class World; }

namespace karma::renderer {

class RenderSystem {
 public:
    explicit RenderSystem(GraphicsDevice& graphics);

    void beginFrame(int width, int height, float dt);
    void submit(const DrawItem& item);
    void setCamera(const CameraData& camera);
    void setDirectionalLight(const DirectionalLightData& light);
    void setWorld(karma::ecs::World* world);
    void renderFrame();
    void endFrame();

 private:
    GraphicsDevice& graphics_;
    std::map<LayerId, std::vector<DrawItem>> queues_;
    CameraData camera_{};
    DirectionalLightData light_{};
    karma::ecs::World* world_ = nullptr;
};

} // namespace karma::renderer

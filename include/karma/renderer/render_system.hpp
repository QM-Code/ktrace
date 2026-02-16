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
    void submitDebugLine(const DebugLineItem& line);
    void setCamera(const CameraData& camera);
    const CameraData& camera() const;
    void setDirectionalLight(const DirectionalLightData& light);
    void setLights(const std::vector<LightData>& lights);
    void setEnvironmentLighting(const EnvironmentLightingData& environment);
    void setWorld(karma::ecs::World* world);
    void renderFrame();
    void endFrame();

 private:
    GraphicsDevice& graphics_;
    std::map<LayerId, std::vector<DrawItem>> queues_;
    std::map<LayerId, std::vector<DebugLineItem>> debug_line_queues_;
    CameraData camera_{};
    DirectionalLightData light_{};
    std::vector<LightData> local_lights_{};
    EnvironmentLightingData environment_{};
    karma::ecs::World* world_ = nullptr;
};

} // namespace karma::renderer

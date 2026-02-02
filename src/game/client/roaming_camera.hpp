#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "karma/core/types.hpp"
#include "karma/ecs/entity.h"
#include "karma/ecs/world.h"
#include "karma/platform/events.hpp"

class Input;

namespace platform {
class Window;
}

namespace game_client {

struct RoamingCameraSettings {
    float moveSpeed = 8.0f;
    float fastMultiplier = 3.0f;
    float lookSensitivity = 0.002f;
    bool invertY = false;
    float startYawOffsetDeg = 0.0f;
};

class RoamingCameraController {
public:
    void syncFromEcs(const karma::ecs::World &world, karma::ecs::Entity entity);
    void setPose(const glm::vec3 &position, const glm::vec3 &target, float yawOffsetDeg);
    void resetMouse();
    void update(TimeUtils::duration deltaTime,
                const Input &input,
                const std::vector<platform::Event> &events,
                const RoamingCameraSettings &settings,
                bool allowInput);
    void applyToEcs(karma::ecs::World &world, karma::ecs::Entity entity) const;

private:
    void updateRotation();

    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};
    bool hasMouse_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

} // namespace game_client

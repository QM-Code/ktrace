#include "client/roaming_camera.hpp"

#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "game/input/actions.hpp"
#include "karma/input/input.hpp"
#include "karma/ecs/world.h"
#include "karma/components/transform.h"

namespace game_client {
namespace components = karma::components;
namespace {

float clampPitch(float pitch) {
    const float limit = glm::half_pi<float>() - 0.01f;
    return std::clamp(pitch, -limit, limit);
}

}

void RoamingCameraController::syncFromEcs(const karma::ecs::World &world, karma::ecs::Entity entity) {
    if (!entity.isValid()) {
        return;
    }
    if (world.has<components::TransformComponent>(entity)) {
        const auto &transform = world.get<components::TransformComponent>(entity);
        position_ = transform.position;
        glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        if (glm::length(forward) < 0.0001f) {
            forward = glm::vec3(0.0f, 0.0f, -1.0f);
        } else {
            forward = glm::normalize(forward);
        }
        pitch_ = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
        yaw_ = std::atan2(forward.x, -forward.z);
        updateRotation();
        resetMouse();
    }
}

void RoamingCameraController::setPose(const glm::vec3 &position, const glm::vec3 &target, float yawOffsetDeg) {
    position_ = position;
    glm::vec3 forward = target - position;
    if (glm::length(forward) < 0.0001f) {
        forward = glm::vec3(0.0f, -1.0f, -1.0f);
    } else {
        forward = glm::normalize(forward);
    }

    pitch_ = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
    yaw_ = std::atan2(forward.x, -forward.z) + glm::radians(yawOffsetDeg);
    updateRotation();
    resetMouse();
}

void RoamingCameraController::resetMouse() {
    hasMouse_ = false;
}

void RoamingCameraController::update(TimeUtils::duration deltaTime,
                                     const Input &input,
                                     const std::vector<platform::Event> &events,
                                     const RoamingCameraSettings &settings,
                                     bool allowInput) {
    double mouseX = lastMouseX_;
    double mouseY = lastMouseY_;
    bool sawMouse = false;
    for (const auto &event : events) {
        if (event.type == platform::EventType::MouseMove) {
            mouseX = event.x;
            mouseY = event.y;
            sawMouse = true;
        }
    }

    double deltaX = 0.0;
    double deltaY = 0.0;
    if (sawMouse) {
        if (hasMouse_) {
            deltaX = mouseX - lastMouseX_;
            deltaY = mouseY - lastMouseY_;
        }
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
        hasMouse_ = true;
    }

    if (allowInput && input.actionDown(game_input::kActionRoamLook)) {
        const float invert = settings.invertY ? -1.0f : 1.0f;
        yaw_ -= static_cast<float>(deltaX) * settings.lookSensitivity;
        pitch_ -= static_cast<float>(deltaY) * settings.lookSensitivity * invert;
        pitch_ = clampPitch(pitch_);
        updateRotation();
    }

    if (!allowInput) {
        return;
    }

    float forwardInput = 0.0f;
    float rightInput = 0.0f;
    float upInput = 0.0f;

    if (input.actionDown(game_input::kActionRoamMoveForward)) {
        forwardInput += 1.0f;
    }
    if (input.actionDown(game_input::kActionRoamMoveBackward)) {
        forwardInput -= 1.0f;
    }
    if (input.actionDown(game_input::kActionRoamMoveRight)) {
        rightInput += 1.0f;
    }
    if (input.actionDown(game_input::kActionRoamMoveLeft)) {
        rightInput -= 1.0f;
    }
    if (input.actionDown(game_input::kActionRoamMoveUp)) {
        upInput += 1.0f;
    }
    if (input.actionDown(game_input::kActionRoamMoveDown)) {
        upInput -= 1.0f;
    }

    glm::vec3 forward = rotation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right = rotation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 move = forward * forwardInput + right * rightInput + up * upInput;
    if (glm::length(move) > 0.001f) {
        move = glm::normalize(move);
        float speed = settings.moveSpeed;
        if (input.actionDown(game_input::kActionRoamMoveFast)) {
            speed *= settings.fastMultiplier;
        }
        position_ += move * speed * deltaTime;
    }
}

void RoamingCameraController::applyToEcs(karma::ecs::World &world, karma::ecs::Entity entity) const {
    if (!entity.isValid()) {
        return;
    }
    if (world.has<components::TransformComponent>(entity)) {
        auto &transform = world.get<components::TransformComponent>(entity);
        transform.position = position_;
        transform.rotation = rotation_;
    }
}

void RoamingCameraController::updateRotation() {
    const glm::quat yawRot = glm::angleAxis(yaw_, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitchRot = glm::angleAxis(pitch_, glm::vec3(1.0f, 0.0f, 0.0f));
    rotation_ = yawRot * pitchRot;
}

} // namespace game_client

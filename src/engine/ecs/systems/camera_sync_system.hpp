#pragma once

#include "karma/renderer/renderer_context.hpp"
#include "karma/components/camera.h"
#include "karma/components/transform.h"
#include "karma/ecs/world.h"

namespace karma::ecs {

class CameraSyncSystem {
public:
    void update(karma::ecs::World &world, engine::renderer::RendererContext &context) {
        auto &cameras = world.storage<karma::components::CameraComponent>();
        if (cameras.denseEntities().empty()) {
            return;
        }
        auto &transforms = world.storage<karma::components::TransformComponent>();

        const karma::components::CameraComponent *selected = nullptr;
        karma::ecs::Entity selectedEntity{};
        for (const auto entity : cameras.denseEntities()) {
            const auto &camera = cameras.get(entity);
            if (camera.is_primary) {
                selected = &camera;
                selectedEntity = entity;
                break;
            }
        }
        if (!selected) {
            const auto &entities = cameras.denseEntities();
            if (!entities.empty()) {
                selectedEntity = entities.front();
                selected = &cameras.get(selectedEntity);
            }
        }
        if (!selected) {
            return;
        }
        if (!transforms.has(selectedEntity)) {
            return;
        }
        const karma::components::TransformComponent &transform = transforms.get(selectedEntity);
        context.cameraPosition = transform.position;
        context.cameraRotation = transform.rotation;
        context.fov = selected->fov_y_degrees;
        context.nearPlane = selected->near_clip;
        context.farPlane = selected->far_clip;
    }
};

} // namespace karma::ecs

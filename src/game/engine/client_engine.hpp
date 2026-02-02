#pragma once
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "game/net/client_network.hpp"
#include "renderer/renderer.hpp"
#include "karma/physics/physics_world.hpp"
#include "karma/input/input.hpp"
#include "game/input/state.hpp"
#include "ui/core/system.hpp"
#include "client/roaming_camera.hpp"
#include "karma/ecs/world.h"
#include "karma/audio/audio.hpp"
#include "karma/platform/window.hpp"
#include <string>
#include <memory>
#include <vector>

namespace ui {
class RendererBridge;
}

class ClientEngine {
private:
    std::unique_ptr<ui::RendererBridge> uiRenderBridge;
    std::string lastLanguage;
    bool roamingMode = false;
    bool roamingModeInitialized = false;
    game_client::RoamingCameraController roamingCamera;

public:
    ClientNetwork *network;
    Renderer *render;
    PhysicsWorld *physics;
    Input *input;
    game_input::InputState inputState;
    UiSystem *ui;
    std::unique_ptr<karma::app::UiLayer> uiLayer;
    Audio *audio;
    karma::ecs::World *ecsWorld = nullptr;
    karma::ecs::Entity cameraEntity{};
    game_client::RoamingCameraController &roamingCameraController() { return roamingCamera; }

    void setRoamingModeSession(bool enabled);
    bool isRoamingModeSession() const { return roamingMode; }

    ClientEngine(platform::Window &window);
    ~ClientEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void step(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
    void updateRoamingCamera(TimeUtils::duration deltaTime, bool allowInput);
    void handleGlobalUiInput();

    const game_input::InputState& getInputState() const { return inputState; }
};

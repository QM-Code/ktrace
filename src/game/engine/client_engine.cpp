#include "game/engine/client_engine.hpp"
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "game/input/bindings.hpp"
#include "game/input/state.hpp"
#include "spdlog/spdlog.h"
#include "karma_extras/ui/bridges/renderer_bridge.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/i18n.hpp"
#include "karma/components/transform.h"
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <utility>

namespace components = karma::components;

namespace {

class RendererBridgeImpl final : public ui::RendererBridge {
public:
    explicit RendererBridgeImpl(Renderer *renderIn) : render(renderIn) {}

    graphics::TextureHandle getRadarTexture() const override {
        return render ? render->getRadarTexture() : graphics::TextureHandle{};
    }
    ui::UiRenderTargetBridge* getUiRenderTargetBridge() const override {
        return render ? render->getUiRenderTargetBridge() : nullptr;
    }

private:
    Renderer *render = nullptr;
};

game_client::RoamingCameraSettings ReadRoamingCameraSettings() {
    game_client::RoamingCameraSettings settings{};
    settings.moveSpeed = karma::config::ReadRequiredFloatConfig("game.roamingCamera.MoveSpeed");
    settings.fastMultiplier = karma::config::ReadRequiredFloatConfig("game.roamingCamera.FastMultiplier");
    settings.lookSensitivity = karma::config::ReadRequiredFloatConfig("game.roamingCamera.LookSensitivity");
    settings.invertY = karma::config::ReadRequiredBoolConfig("game.roamingCamera.InvertY");
    settings.startYawOffsetDeg = karma::config::ReadRequiredFloatConfig("game.roamingCamera.StartYawOffsetDeg");
    return settings;
}

glm::vec3 ReadRequiredVec3Config(const char *path) {
    auto value = karma::config::ConfigStore::GetCopy(path);
    if (!value || !value->is_array() || value->size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must be an array of 3 numbers");
    }
    glm::vec3 result(0.0f);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto &entry = (*value)[i];
        if (!entry.is_number()) {
            throw std::runtime_error(std::string("Config '") + path + "' entries must be numbers");
        }
        result[static_cast<int>(i)] = entry.get<float>();
    }
    return result;
}

} // namespace

ClientEngine::ClientEngine(platform::Window &window) {
    network = new ClientNetwork();
    spdlog::trace("ClientEngine: ClientNetwork initialized successfully");
    spdlog::trace("ClientEngine: Renderer initializing");
    render = new Renderer(window);
    spdlog::trace("ClientEngine: Renderer initialized successfully");
    auto* rendererBridgeImpl = new RendererBridgeImpl(render);
    uiRenderBridge.reset(rendererBridgeImpl);
    physics = new PhysicsWorld();
    spdlog::trace("ClientEngine: Physics initialized successfully");
    input = new Input(window, game_input::DefaultKeybindings());
    spdlog::trace("ClientEngine: Input initialized successfully");
    ui = new UiSystem(window);
    ui->setRendererBridge(rendererBridgeImpl);
    spdlog::trace("ClientEngine: UiSystem initialized successfully");
    lastLanguage = karma::i18n::Get().language();
    ui->setDialogText(game_input::SpawnHintText(*input));
    audio = new Audio();
    spdlog::trace("ClientEngine: Audio initialized successfully");
    ecsWorld = nullptr;
}

ClientEngine::~ClientEngine() {
    delete network;
    delete ui;
    delete render;
    delete physics;
    delete input;
    delete audio;
}

void ClientEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    const bool allowGameplayInput = !ui || ui->isGameplayInputEnabled();
    inputState = game_input::BuildInputState(*input);
    if (!allowGameplayInput) {
        inputState.fire = false;
        inputState.spawn = false;
        inputState.jump = false;
        inputState.movement = {};
    }
    network->update();
}

void ClientEngine::step(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
}

void ClientEngine::lateUpdate(TimeUtils::duration deltaTime) {
    if (cameraEntity.isValid() && ecsWorld && isRoamingModeSession()) {
        const bool consoleVisible = ui && ui->console().isVisible();
        const bool allowInput = !consoleVisible && (!ui || ui->isGameplayInputEnabled());
        updateRoamingCamera(deltaTime, allowInput);
        roamingCamera.applyToEcs(*ecsWorld, cameraEntity);
    }
    if (render) {
        glm::vec3 camPos(0.0f);
        glm::quat camRot(1.0f, 0.0f, 0.0f, 0.0f);
        if (ecsWorld && cameraEntity.isValid() &&
            ecsWorld->has<components::TransformComponent>(cameraEntity)) {
            const auto &transform = ecsWorld->get<components::TransformComponent>(cameraEntity);
            camPos = transform.position;
            camRot = transform.rotation;
        }
        render->renderRadar(camPos, camRot);
    }
    const std::string currentLanguage = karma::i18n::Get().language();
    if (currentLanguage != lastLanguage) {
        lastLanguage = currentLanguage;
        if (input) {
            ui->setDialogText(game_input::SpawnHintText(*input));
        }
    }
    if (ui->consumeKeybindingsReloadRequest()) {
        input->reloadKeyBindings();
        ui->setDialogText(game_input::SpawnHintText(*input));
    }
    handleGlobalUiInput();
    network->flushPeekedMessages();
}

void ClientEngine::updateRoamingCamera(TimeUtils::duration deltaTime, bool allowInput) {
    if (!roamingMode || !render || !input) {
        return;
    }
    const auto settings = ReadRoamingCameraSettings();
    const std::vector<platform::Event> emptyEvents;
    const auto &events = input ? input->events() : emptyEvents;
    roamingCamera.update(deltaTime, *input, events, settings, allowInput);
}

void ClientEngine::handleGlobalUiInput() {
    if (!ui || !input) {
        return;
    }
    const bool consoleVisible = ui->console().isVisible();
    if (consoleVisible && inputState.escape) {
        ui->console().hide();
        return;
    }
    if (inputState.escape) {
        if (ui->isQuickMenuVisible()) {
            ui->setQuickMenuVisible(false);
        } else if (!consoleVisible) {
            ui->setQuickMenuVisible(true);
        }
    }
}

void ClientEngine::setRoamingModeSession(bool enabled) {
    roamingModeInitialized = true;
    roamingMode = enabled;
    if (roamingMode && render) {
        const glm::vec3 startPosition = ReadRequiredVec3Config("game.roamingCamera.StartPosition");
        const glm::vec3 startTarget = ReadRequiredVec3Config("game.roamingCamera.StartTarget");
        const auto settings = ReadRoamingCameraSettings();
        roamingCamera.setPose(startPosition, startTarget, settings.startYawOffsetDeg);
    } else {
        roamingCamera.resetMouse();
    }
}

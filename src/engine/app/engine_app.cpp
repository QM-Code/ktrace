#include "karma/app/engine_app.hpp"
#include "karma/core/types.hpp"
#include "karma/renderer/renderer_core.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/input/input.hpp"
#include <cstdlib>
#include <vector>

namespace karma::app {
EngineApp::EngineApp() {
    context_.ecsWorld = &ecsWorld_;
    context_.rendererContext.fov = karma::config::ReadRequiredFloatConfig("graphics.Camera.FovDegrees");
    context_.rendererContext.nearPlane = karma::config::ReadRequiredFloatConfig("graphics.Camera.NearPlane");
    context_.rendererContext.farPlane = karma::config::ReadRequiredFloatConfig("graphics.Camera.FarPlane");
}

EngineApp::~EngineApp() = default;

void EngineApp::setGame(GameInterface *game) {
    game_ = game;
}

void EngineApp::setConfig(const EngineConfig &config) {
    config_ = config;
}

EngineConfig &EngineApp::config() {
    return config_;
}

const EngineConfig &EngineApp::config() const {
    return config_;
}

void EngineApp::setUi(std::unique_ptr<UiLayer> ui) {
    ui_layer_ = std::move(ui);
}

bool EngineApp::start(GameInterface &game, const EngineConfig &config) {
    if (started_) {
        return false;
    }
    started_ = true;
    running_ = true;
    config_ = config;
    game_ = &game;
    game_->context_ = &context_;
    if (context_.window) {
        context_.window->setCursorVisible(config_.cursor_visible);
    }
#ifndef KARMA_SERVER
    if (!context_.graphics && context_.rendererCore) {
        context_.graphics = &context_.rendererCore->device();
    }
    if (context_.graphics) {
        resources_ = std::make_unique<graphics::ResourceRegistry>(*context_.graphics);
        context_.resources = resources_.get();
        context_.defaultMaterial = context_.resources->getDefaultMaterial();
        rendererSystem_.setDefaultMaterial(context_.defaultMaterial);
    }
    if (context_.rendererCore) {
        context_.rendererContext = context_.rendererCore->context();
    }
#endif
    game_->onStart();
    last_tick_time_ = TimeUtils::GetCurrentTime();
    fixed_accumulator_ = 0.0f;
    return true;
}

void EngineApp::tick() {
    if (!running_ || !game_) {
        return;
    }
    const TimeUtils::time now = TimeUtils::GetCurrentTime();
    const float dt = TimeUtils::GetElapsedTime(last_tick_time_, now);
    last_tick_time_ = now;
    if (config_.enable_fixed_update && config_.fixed_timestep > 0.0f) {
        fixed_accumulator_ += dt;
        while (fixed_accumulator_ >= config_.fixed_timestep) {
            game_->onFixedUpdate(config_.fixed_timestep);
            fixed_accumulator_ -= config_.fixed_timestep;
        }
    }
#ifndef KARMA_SERVER
    if (context_.rendererCore) {
        context_.rendererContext = context_.rendererCore->context();
    }
    if (context_.rendererCore && context_.window) {
        int width = 0;
        int height = 0;
        context_.window->getFramebufferSize(width, height);
        if (height <= 0) {
            height = 1;
        }
        if (width <= 0) {
            width = 1;
        }
        if (width != lastFramebufferWidth_ || height != lastFramebufferHeight_) {
            lastFramebufferWidth_ = width;
            lastFramebufferHeight_ = height;
            context_.rendererCore->scene().resize(width, height);
        }
        context_.rendererCore->scene().beginFrame();
        context_.rendererContext.aspect = static_cast<float>(width) / static_cast<float>(height);
    }
#endif
    if (context_.window) {
        context_.window->pollEvents();
    }
    const std::vector<platform::Event> emptyEvents;
    const auto &events = context_.window ? context_.window->events() : emptyEvents;
#ifndef KARMA_SERVER
    if (context_.input && context_.window) {
        context_.input->pumpEvents(events);
    }
#endif
    if (ui_layer_) {
        for (const auto &event : events) {
            ui_layer_->onEvent(event);
        }
    }
    game_->onUpdate(dt);
    if (ui_layer_) {
        UIFrameInfo frame{};
        frame.dt = dt;
        frame.viewport_w = lastFramebufferWidth_;
        frame.viewport_h = lastFramebufferHeight_;
        if (context_.window) {
            frame.dpi_scale = context_.window->getContentScale();
        }
        ui_context_.setFrameInfo(frame);
        ui_context_.setInput(context_.input);
        ui_context_.clearFrame();
        ui_layer_->onFrame(ui_context_);
    }
    if (context_.window) {
        context_.window->clearEvents();
    }
#ifndef KARMA_SERVER
    if (config_.enable_ecs_camera_sync) {
        cameraSyncSystem_.update(ecsWorld_, context_.rendererContext);
    }
    if (context_.rendererCore) {
        context_.rendererCore->context() = context_.rendererContext;
    }
#endif
#ifndef KARMA_SERVER
    if (config_.enable_ecs_render_sync) {
        renderSyncSystem_.update(ecsWorld_, context_.resources, context_.defaultMaterial);
        proceduralMeshSyncSystem_.update(ecsWorld_, context_.graphics);
    }
#endif
    if (config_.enable_ecs_physics_sync) {
        physicsSyncSystem_.update(ecsWorld_, context_.physics);
    }
#ifndef KARMA_SERVER
    if (config_.enable_ecs_audio_sync) {
        audioSyncSystem_.update(ecsWorld_, context_.audio);
    }
#endif
    systemGraph_.update(ecsWorld_, dt);
    karma::config::ConfigStore::Tick();
#ifndef KARMA_SERVER
    rendererSystem_.update(ecsWorld_, context_.graphics, dt);
#endif
#ifndef KARMA_SERVER
    if (context_.rendererCore) {
        context_.rendererCore->scene().renderMain(context_.rendererContext);
        if (ui_layer_) {
            if (!std::getenv("KARMA_DISABLE_UI_OVERLAY")) {
                context_.rendererCore->scene().renderUi(ui_context_);
            }
            context_.rendererCore->scene().setBrightness(ui_context_.brightness());
        }
        context_.rendererCore->scene().endFrame();
    }
#endif
    if (game_->shouldQuit()) {
        running_ = false;
        if (ui_layer_) {
            ui_layer_->onShutdown();
        }
        game_->onShutdown();
    }
    if (context_.window && context_.window->shouldClose()) {
        running_ = false;
        if (ui_layer_) {
            ui_layer_->onShutdown();
        }
        game_->onShutdown();
    }
}

bool EngineApp::isRunning() const {
    if (!running_) {
        return false;
    }
    if (context_.window && context_.window->shouldClose()) {
        return false;
    }
    if (game_ && game_->shouldQuit()) {
        return false;
    }
    return true;
}

EngineContext &EngineApp::context() {
    return context_;
}

const EngineContext &EngineApp::context() const {
    return context_;
}

} // namespace karma::app

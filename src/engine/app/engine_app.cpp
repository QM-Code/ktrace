#include "karma/app/engine_app.hpp"

#include <chrono>
#include <string>

#include "karma/common/logging.hpp"
#include "karma/renderer/render_system.hpp"

namespace karma::app {

EngineApp::EngineApp() : scene_(world_) {}

EngineApp::~EngineApp() {
    if (game_ && running_) {
        game_->onUiShutdown();
        game_->onShutdown();
    }
    shutdownSubsystems();
}

void EngineApp::initSubsystems() {
    KARMA_TRACE("engine.app", "EngineApp: creating window");
    window_ = platform::CreateWindow(config_.window);
    if (!window_) {
        KARMA_TRACE("engine.app", "EngineApp: window creation failed");
        return;
    }
    window_->setVsync(config_.vsync);
    if (config_.window.fullscreen) {
        window_->setFullscreen(true);
    }
    input_system_.setWindow(window_.get());
    input::LoadBindingsFromConfig(input_system_);
    KARMA_TRACE("engine.app", "EngineApp: creating graphics device");
    graphics_ = std::make_unique<renderer::GraphicsDevice>(*window_);
    if (graphics_ && graphics_->isValid()) {
        KARMA_TRACE("engine.app", "EngineApp: graphics device ready");
    } else {
        spdlog::error("EngineApp: graphics device failed to initialize");
        graphics_.reset();
    }
    if (graphics_) {
        render_system_ = std::make_unique<renderer::RenderSystem>(*graphics_);
        render_system_->setCamera(config_.default_camera);
        render_system_->setDirectionalLight(config_.default_light);
        render_system_->setWorld(&world_);
        roaming_camera_.loadFromConfig();
        roaming_camera_.initialize(*render_system_);
        ui_system_.init(*graphics_);
    }
}

void EngineApp::shutdownSubsystems() {
    if (graphics_) {
        ui_system_.shutdown(*graphics_);
    }
    if (graphics_) {
        scene::ReleaseStartupSceneResources(*graphics_, world_, startup_scene_resources_);
    }
    render_system_.reset();
    graphics_.reset();
    window_.reset();
    running_ = false;
}

void EngineApp::start(GameInterface& game, const EngineConfig& config) {
    if (running_) {
        return;
    }
    config_ = config;
    initSubsystems();
    if (!window_ || !graphics_ || !render_system_) {
        return;
    }

    if (!scene::PopulateStartupWorld(*graphics_,
                                     world_,
                                     startup_scene_resources_)) {
        spdlog::error("EngineApp: startup scene bootstrap failed, aborting start");
        shutdownSubsystems();
        return;
    }

    game_ = &game;
    game_->bind(*window_, *graphics_, *render_system_, world_, scene_, input_system_);
    game_->onStart();
    game_->onUiStart();
    KARMA_TRACE("ecs.world", "EngineApp: world ready entities={}", world_.entities().size());
    running_ = true;
}

void EngineApp::requestStop() {
    running_ = false;
}

void EngineApp::tick() {
    if (!running_ || !game_) {
        return;
    }

    static auto last_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_time).count();
    last_time = now;

    window_->pollEvents();
    if (window_->shouldClose()) {
        requestStop();
    }

    input_system_.update(window_->events());
    KARMA_TRACE_CHANGED("ecs.world",
                        std::to_string(world_.entities().size()),
                        "EngineApp: world entities={}",
                        world_.entities().size());

    game_->onUpdate(dt);
    ui_system_.beginFrame(dt);
    game_->onUiUpdate(dt);

    int fb_w = 0;
    int fb_h = 0;
    window_->getFramebufferSize(fb_w, fb_h);
    if (render_system_) {
        roaming_camera_.setActive(input_system_.mode() == input::InputMode::Roaming);
        roaming_camera_.update(dt, input_system_, *render_system_);
        scene_.updateWorldTransforms();
        ui_system_.update(*render_system_);
        ui_system_.endFrame();
        render_system_->beginFrame(fb_w, fb_h, dt);
        render_system_->renderFrame();
        render_system_->endFrame();
    }

    window_->clearEvents();

    if (!running_) {
        game_->onUiShutdown();
        game_->onShutdown();
        shutdownSubsystems();
        game_ = nullptr;
    }
}

} // namespace karma::app

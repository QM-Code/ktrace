#include "karma/app/engine_app.hpp"

#include <chrono>

#include "karma/common/logging.hpp"
#include "karma/renderer/render_system.hpp"

namespace karma::app {

EngineApp::~EngineApp() {
    if (game_ && running_) {
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
    }
}

void EngineApp::shutdownSubsystems() {
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
    game_ = &game;
    game_->bind(*window_, *graphics_, *render_system_, input_system_);
    game_->onStart();
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

    game_->onUpdate(dt);

    int fb_w = 0;
    int fb_h = 0;
    window_->getFramebufferSize(fb_w, fb_h);
    if (render_system_) {
        render_system_->beginFrame(fb_w, fb_h, dt);
        render_system_->renderFrame();
        render_system_->endFrame();
    }

    window_->clearEvents();

    if (!running_) {
        game_->onShutdown();
        shutdownSubsystems();
        game_ = nullptr;
    }
}

} // namespace karma::app

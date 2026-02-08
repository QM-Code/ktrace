#include "karma/app/engine_app.hpp"

#include <chrono>
#include <sstream>
#include <string>

#include "karma/common/logging.hpp"
#include "karma/renderer/render_system.hpp"
#include <glm/glm.hpp>

namespace karma::app {
namespace {

std::string CompiledBackendList() {
    const auto compiled = renderer_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }
    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << renderer_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

std::string CompiledPhysicsBackendList() {
    const auto compiled = physics_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }
    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << physics_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

std::string CompiledAudioBackendList() {
    const auto compiled = audio_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }
    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << audio_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

} // namespace

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
    const float fixed_dt = (config_.simulation_fixed_hz > 1e-6f) ? (1.0f / config_.simulation_fixed_hz) : (1.0f / 60.0f);
    simulation_clock_.configure(fixed_dt, config_.simulation_max_frame_dt, config_.simulation_max_steps);
    KARMA_TRACE("engine.sim",
                "EngineApp: simulation clock fixed_dt={:.4f} max_frame_dt={:.3f} max_steps={}",
                simulation_clock_.fixedDeltaTime(),
                config_.simulation_max_frame_dt,
                config_.simulation_max_steps);
    physics_system_.setBackend(config_.physics_backend);
    KARMA_TRACE("engine.app",
                "EngineApp: creating physics backend (requested='{}', compiled='{}')",
                physics_backend::BackendKindName(config_.physics_backend),
                CompiledPhysicsBackendList());
    physics_system_.init();
    if (!physics_system_.isInitialized()) {
        spdlog::error("EngineApp: physics backend failed to initialize (requested='{}', compiled='{}')",
                      physics_backend::BackendKindName(config_.physics_backend),
                      CompiledPhysicsBackendList());
        return;
    }
    KARMA_TRACE("engine.app",
                "EngineApp: physics backend ready backend='{}'",
                physics_system_.selectedBackendName());
    if (config_.enable_audio) {
        audio_system_.setBackend(config_.audio_backend);
        KARMA_TRACE("engine.app",
                    "EngineApp: creating audio backend (requested='{}', compiled='{}')",
                    audio_backend::BackendKindName(config_.audio_backend),
                    CompiledAudioBackendList());
        audio_system_.init();
        if (!audio_system_.isInitialized()) {
            spdlog::error("EngineApp: audio backend failed to initialize (requested='{}', compiled='{}')",
                          audio_backend::BackendKindName(config_.audio_backend),
                          CompiledAudioBackendList());
            return;
        }
        KARMA_TRACE("engine.app",
                    "EngineApp: audio backend ready backend='{}'",
                    audio_system_.selectedBackendName());
    } else {
        KARMA_TRACE("engine.app", "EngineApp: audio disabled by config");
    }
    KARMA_TRACE("engine.app",
                "EngineApp: creating graphics device (requested='{}', compiled='{}')",
                renderer_backend::BackendKindName(config_.render_backend),
                CompiledBackendList());
    graphics_ = std::make_unique<renderer::GraphicsDevice>(*window_, config_.render_backend);
    if (graphics_ && graphics_->isValid()) {
        KARMA_TRACE("engine.app",
                    "EngineApp: graphics device ready backend='{}'",
                    graphics_->backendName());
    } else {
        spdlog::error("EngineApp: graphics device failed to initialize (requested='{}', compiled='{}')",
                      renderer_backend::BackendKindName(config_.render_backend),
                      CompiledBackendList());
        graphics_.reset();
    }
    if (graphics_) {
        render_system_ = std::make_unique<renderer::RenderSystem>(*graphics_);
        render_system_->setCamera(config_.default_camera);
        render_system_->setDirectionalLight(config_.default_light);
        render_system_->setWorld(&world_);
        roaming_camera_.loadFromConfig();
        roaming_camera_.initialize(*render_system_);
        if (config_.ui_backend_override.has_value()) {
            ui_system_.setBackend(*config_.ui_backend_override);
        }
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
    audio_system_.shutdown();
    physics_system_.shutdown();
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
    const float raw_dt = std::chrono::duration<float>(now - last_time).count();
    float dt = raw_dt;
    last_time = now;

    window_->pollEvents();
    if (window_->shouldClose()) {
        requestStop();
    }

    ui_system_.beginFrame(dt, window_->events());
    input_system_.update(window_->events());
    KARMA_TRACE_CHANGED("ecs.world",
                        std::to_string(world_.entities().size()),
                        "EngineApp: world entities={}",
                        world_.entities().size());

    game_->onUpdate(dt);
    physics_system_.beginFrame(dt);
    const int physics_steps = simulation_clock_.beginFrame(dt);
    for (int i = 0; i < physics_steps; ++i) {
        physics_system_.simulateFixedStep(simulation_clock_.fixedDeltaTime());
    }
    if (logging::ShouldTraceChannel("engine.sim.frames")) {
        KARMA_TRACE("engine.sim.frames",
                    "EngineApp: dt_raw={:.4f} steps={} fixed_dt={:.4f} accumulator={:.4f}",
                    raw_dt,
                    physics_steps,
                    simulation_clock_.fixedDeltaTime(),
                    simulation_clock_.accumulator());
    }
    if (config_.simulation_max_frame_dt > 0.0f &&
        raw_dt > config_.simulation_max_frame_dt + 1e-6f) {
        KARMA_TRACE("engine.sim",
                    "EngineApp: frame dt clamped raw={:.4f} max={:.4f}",
                    raw_dt,
                    config_.simulation_max_frame_dt);
    }
    if (physics_steps != 1) {
        KARMA_TRACE("engine.sim",
                    "EngineApp: simulation catch-up steps={} fixed_dt={:.4f} accumulator={:.4f}",
                    physics_steps,
                    simulation_clock_.fixedDeltaTime(),
                    simulation_clock_.accumulator());
    }
    physics_system_.endFrame();

    if (config_.enable_audio) {
        audio_system_.beginFrame(dt);
        if (render_system_) {
            const renderer::CameraData& camera = render_system_->camera();
            glm::vec3 forward = camera.target - camera.position;
            const float forward_length = glm::length(forward);
            if (forward_length > 1e-6f) {
                forward /= forward_length;
            } else {
                forward = {0.0f, 0.0f, -1.0f};
            }
            audio_backend::ListenerState listener{};
            listener.position = camera.position;
            listener.forward = forward;
            listener.up = {0.0f, 1.0f, 0.0f};
            if (has_last_listener_position_ && dt > 1e-6f) {
                listener.velocity = (camera.position - last_listener_position_) / dt;
            }
            last_listener_position_ = camera.position;
            has_last_listener_position_ = true;
            audio_system_.setListener(listener);
        }
        audio_system_.update(dt);
        audio_system_.endFrame();
    }

    game_->onUiUpdate(dt, ui_system_.drawContext());

    int fb_w = 0;
    int fb_h = 0;
    window_->getFramebufferSize(fb_w, fb_h);
    if (render_system_) {
        const bool ui_capturing =
            ui_system_.wantsMouseCapture() || ui_system_.wantsKeyboardCapture();
        roaming_camera_.setActive(input_system_.mode() == input::InputMode::Roaming && !ui_capturing);
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

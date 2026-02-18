#include "karma/app/server/engine.hpp"

#include "karma/common/logging/logging.hpp"
#include "physics/sync/engine_fixed_step_sync.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>

namespace karma::app::server {
namespace {

std::string CompiledPhysicsBackendList() {
    const auto compiled = physics::backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }
    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << physics::backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

std::string CompiledAudioBackendList() {
    const auto compiled = audio::backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }
    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << audio::backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

} // namespace

Engine::Engine() = default;

Engine::~Engine() {
    shutdown();
}

void Engine::start(GameInterface& game, const EngineConfig& config) {
    if (running_) {
        return;
    }

    game_ = &game;
    config_ = config;
    const float fixed_dt = (config_.target_tick_hz > 1e-6f) ? (1.0f / config_.target_tick_hz) : (1.0f / 60.0f);
    simulation_clock_.configure(fixed_dt, config_.max_delta_time, config_.max_substeps);
    KARMA_TRACE("engine.sim",
                "Engine: simulation clock fixed_dt={:.4f} max_frame_dt={:.3f} max_steps={}",
                simulation_clock_.fixedDeltaTime(),
                config_.max_delta_time,
                config_.max_substeps);
    physics_system_.setBackend(config_.physics_backend);
    KARMA_TRACE("engine.server",
                "Engine: creating physics backend (requested='{}', compiled='{}')",
                physics::backend::BackendKindName(config_.physics_backend),
                CompiledPhysicsBackendList());
    physics_system_.init();
    if (!physics_system_.isInitialized()) {
        spdlog::error("Engine: physics backend failed to initialize (requested='{}', compiled='{}')",
                      physics::backend::BackendKindName(config_.physics_backend),
                      CompiledPhysicsBackendList());
        game_ = nullptr;
        running_ = false;
        return;
    }
    KARMA_TRACE("engine.server",
                "Engine: physics backend ready backend='{}'",
                physics_system_.selectedBackendName());
    physics_sync_system_ = physics::detail::CreateEngineSyncIfPhysicsInitialized(physics_system_);
    if (config_.enable_audio) {
        audio_system_.setBackend(config_.audio_backend);
        KARMA_TRACE("engine.server",
                    "Engine: creating audio backend (requested='{}', compiled='{}')",
                    audio::backend::BackendKindName(config_.audio_backend),
                    CompiledAudioBackendList());
        audio_system_.init();
        if (!audio_system_.isInitialized()) {
            spdlog::error("Engine: audio backend failed to initialize (requested='{}', compiled='{}')",
                          audio::backend::BackendKindName(config_.audio_backend),
                          CompiledAudioBackendList());
            physics::detail::ResetEngineSyncBeforePhysicsShutdown(physics_sync_system_, &physics_system_);
            physics_system_.shutdown();
            game_ = nullptr;
            running_ = false;
            return;
        }
        KARMA_TRACE("engine.server",
                    "Engine: audio backend ready backend='{}'",
                    audio_system_.selectedBackendName());
    } else {
        KARMA_TRACE("engine.server", "Engine: audio disabled (headless default)");
    }
    game_->bind(world_);
    last_tick_time_ = std::chrono::steady_clock::now();
    running_ = true;

    KARMA_TRACE("engine.server",
                "Engine: start target_hz={:.2f} max_dt={:.3f} max_substeps={}",
                config_.target_tick_hz,
                config_.max_delta_time,
                config_.max_substeps);
    game_->onStart();
}

void Engine::requestStop() {
    running_ = false;
}

void Engine::tick() {
    if (!running_ || !game_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    const float min_dt = (config_.target_tick_hz > 0.0f) ? (1.0f / config_.target_tick_hz) : 0.0f;
    if (min_dt > 0.0f) {
        const float elapsed = std::chrono::duration<float>(now - last_tick_time_).count();
        if (elapsed < min_dt) {
            std::this_thread::sleep_for(std::chrono::duration<float>(min_dt - elapsed));
            now = std::chrono::steady_clock::now();
        }
    }

    const float raw_dt = std::chrono::duration<float>(now - last_tick_time_).count();
    float dt = raw_dt;
    last_tick_time_ = now;
    if (config_.max_delta_time > 0.0f) {
        dt = std::min(dt, config_.max_delta_time);
    }

    KARMA_TRACE_CHANGED("ecs.world",
                        std::to_string(world_.entities().size()),
                        "Engine: world entities={}",
                        world_.entities().size());

    const int sim_steps = simulation_clock_.beginFrame(dt);
    if (common::logging::ShouldTraceChannel("engine.sim.frames")) {
        const float frame_ms = raw_dt * 1000.0f;
        const float fps = (raw_dt > 1e-6f) ? (1.0f / raw_dt) : 0.0f;
        KARMA_TRACE("engine.sim.frames",
                    "Engine: dt_raw={:.4f}s frame_ms={:.2f} fps={:.1f} dt={:.4f} steps={} fixed_dt={:.4f} accumulator={:.4f}",
                    raw_dt,
                    frame_ms,
                    fps,
                    dt,
                    sim_steps,
                    simulation_clock_.fixedDeltaTime(),
                    simulation_clock_.accumulator());
    }
    if (config_.max_delta_time > 0.0f &&
        raw_dt > config_.max_delta_time + 1e-6f) {
        KARMA_TRACE("engine.sim",
                    "Engine: frame dt clamped raw={:.4f} max={:.4f}",
                    raw_dt,
                    config_.max_delta_time);
    }
    if (sim_steps != 1) {
        KARMA_TRACE("engine.sim",
                    "Engine: simulation catch-up steps={} fixed_dt={:.4f} accumulator={:.4f}",
                    sim_steps,
                    simulation_clock_.fixedDeltaTime(),
                    simulation_clock_.accumulator());
    }

    physics_system_.beginFrame(dt);
    physics::detail::SimulateFixedStepsWithSync(
        physics_system_, physics_sync_system_.get(), world_, sim_steps, simulation_clock_.fixedDeltaTime());
    physics_system_.endFrame();
    if (config_.enable_audio) {
        audio_system_.beginFrame(dt);
        audio_system_.update(dt);
        audio_system_.endFrame();
    }
    for (int i = 0; i < sim_steps; ++i) {
        game_->onTick(simulation_clock_.fixedDeltaTime());
    }

    if (!running_) {
        shutdown();
    }
}

void Engine::shutdown() {
    if (!game_) {
        running_ = false;
        return;
    }

    game_->onShutdown();
    if (config_.enable_audio) {
        audio_system_.shutdown();
    }
    physics::detail::ResetEngineSyncBeforePhysicsShutdown(physics_sync_system_, &physics_system_);
    physics_system_.shutdown();
    KARMA_TRACE("engine.server",
                "Engine: shutdown world_entities={}",
                world_.entities().size());

    game_ = nullptr;
    running_ = false;
}

} // namespace karma::app::server

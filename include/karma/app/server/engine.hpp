#pragma once

#include <chrono>

#include "karma/app/server/game_interface.hpp"
#include "karma/audio/audio_system.hpp"
#include "karma/audio/backend.hpp"
#include "karma/app/shared/simulation_clock.hpp"
#include "karma/ecs/world.hpp"
#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"

namespace karma::app::server {

struct EngineConfig {
    float target_tick_hz = 60.0f;
    float max_delta_time = 0.25f;
    int max_substeps = 4;
    physics_backend::BackendKind physics_backend = physics_backend::BackendKind::Auto;
    audio_backend::BackendKind audio_backend = audio_backend::BackendKind::Auto;
    bool enable_audio = false;
};

class Engine {
 public:
    Engine() = default;
    ~Engine();

    void start(GameInterface& game, const EngineConfig& config = {});
    void tick();
    bool isRunning() const { return running_; }
    void requestStop();

 private:
    void shutdown();

    GameInterface* game_ = nullptr;
    EngineConfig config_{};
    ecs::World world_{};
    audio::AudioSystem audio_system_{};
    physics::PhysicsSystem physics_system_{};
    shared::SimulationClock simulation_clock_{};
    bool running_ = false;
    std::chrono::steady_clock::time_point last_tick_time_{};
};

} // namespace karma::app::server

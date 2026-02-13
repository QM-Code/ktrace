#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "server/domain/shot_types.hpp"

namespace bz3::server::domain {

class ShotSystem {
 public:
    using Clock = std::chrono::steady_clock;

    ShotSystem() = default;

    void setLifetime(std::chrono::milliseconds lifetime);
    void clear();

    void addShot(uint32_t source_client_id,
                 uint32_t global_shot_id,
                 float pos_x,
                 float pos_y,
                 float pos_z,
                 float vel_x,
                 float vel_y,
                 float vel_z,
                 Clock::time_point now = Clock::now());

    std::vector<ExpiredShot> update(Clock::time_point now, float dt_seconds);

    size_t activeShotCount() const { return shots_.size(); }
    std::chrono::milliseconds lifetime() const { return lifetime_; }

 private:
    std::chrono::milliseconds lifetime_{std::chrono::milliseconds(5000)};
    std::vector<ShotState> shots_{};
};

} // namespace bz3::server::domain

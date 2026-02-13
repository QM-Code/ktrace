#pragma once

#include <chrono>
#include <cstdint>

namespace bz3::server::domain {

struct ShotState {
    uint32_t source_client_id = 0;
    uint32_t global_shot_id = 0;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;
    float vel_x = 0.0f;
    float vel_y = 0.0f;
    float vel_z = 0.0f;
    std::chrono::steady_clock::time_point created_at{};
};

struct ExpiredShot {
    uint32_t source_client_id = 0;
    uint32_t global_shot_id = 0;
};

} // namespace bz3::server::domain

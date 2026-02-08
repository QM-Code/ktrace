#include "karma/common/simulation_clock.hpp"

#include <algorithm>
#include <cmath>

namespace karma::common {

void SimulationClock::configure(float fixed_dt, float max_frame_dt, int max_steps) {
    if (fixed_dt > 1e-6f) {
        fixed_dt_ = fixed_dt;
    }
    if (max_frame_dt > 0.0f) {
        max_frame_dt_ = max_frame_dt;
    }
    if (max_steps > 0) {
        max_steps_ = max_steps;
    }
    accumulator_ = 0.0f;
}

int SimulationClock::beginFrame(float frame_dt) {
    const float clamped_dt = std::clamp(frame_dt, 0.0f, max_frame_dt_);
    accumulator_ += clamped_dt;
    const int steps = std::min(max_steps_, static_cast<int>(std::floor(accumulator_ / fixed_dt_)));
    if (steps > 0) {
        accumulator_ -= static_cast<float>(steps) * fixed_dt_;
    }
    return steps;
}

} // namespace karma::common


#pragma once

namespace karma::common {

class SimulationClock {
 public:
    void configure(float fixed_dt, float max_frame_dt, int max_steps);
    int beginFrame(float frame_dt);

    float fixedDeltaTime() const { return fixed_dt_; }
    float accumulator() const { return accumulator_; }
    int maxSteps() const { return max_steps_; }

 private:
    float fixed_dt_ = 1.0f / 60.0f;
    float max_frame_dt_ = 0.25f;
    int max_steps_ = 4;
    float accumulator_ = 0.0f;
};

} // namespace karma::common


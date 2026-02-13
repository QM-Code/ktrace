#include "server/domain/shot_system.hpp"

#include <algorithm>

namespace bz3::server::domain {

void ShotSystem::setLifetime(std::chrono::milliseconds lifetime) {
    lifetime_ = std::max(std::chrono::milliseconds(1), lifetime);
}

void ShotSystem::clear() {
    shots_.clear();
}

void ShotSystem::addShot(uint32_t source_client_id,
                         uint32_t global_shot_id,
                         float pos_x,
                         float pos_y,
                         float pos_z,
                         float vel_x,
                         float vel_y,
                         float vel_z,
                         Clock::time_point now) {
    auto existing = std::find_if(
        shots_.begin(),
        shots_.end(),
        [global_shot_id](const ShotState& shot) { return shot.global_shot_id == global_shot_id; });
    if (existing != shots_.end()) {
        *existing = ShotState{
            source_client_id,
            global_shot_id,
            pos_x,
            pos_y,
            pos_z,
            vel_x,
            vel_y,
            vel_z,
            now};
        return;
    }

    shots_.push_back(ShotState{
        source_client_id,
        global_shot_id,
        pos_x,
        pos_y,
        pos_z,
        vel_x,
        vel_y,
        vel_z,
        now});
}

std::vector<ExpiredShot> ShotSystem::update(Clock::time_point now, float dt_seconds) {
    if (shots_.empty()) {
        return {};
    }

    const float clamped_dt = std::max(0.0f, dt_seconds);
    if (clamped_dt > 0.0f) {
        for (auto& shot : shots_) {
            shot.pos_x += shot.vel_x * clamped_dt;
            shot.pos_y += shot.vel_y * clamped_dt;
            shot.pos_z += shot.vel_z * clamped_dt;
        }
    }

    std::vector<ExpiredShot> expired{};
    expired.reserve(shots_.size());

    auto end_it = std::remove_if(
        shots_.begin(),
        shots_.end(),
        [this, now, &expired](const ShotState& shot) {
            if (now - shot.created_at < lifetime_) {
                return false;
            }
            expired.push_back(ExpiredShot{shot.source_client_id, shot.global_shot_id});
            return true;
        });
    shots_.erase(end_it, shots_.end());

    return expired;
}

} // namespace bz3::server::domain

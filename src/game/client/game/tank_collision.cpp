#include "game.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"
#include "karma/geometry/mesh_loader.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <vector>

namespace bz3 {

namespace {

constexpr float kMinObstacleHeight = 0.8f;
constexpr float kMinObstacleArea = 0.7f;
constexpr float kHugeGroundArea = 1500.0f;

} // namespace

void Game::buildTankCollisionCache() {
    tank_collision_rects_.clear();
    tank_collision_ready_ = false;
    tank_world_bounds_ready_ = false;

    const auto world_path = karma::config::ConfigStore::ResolveAssetPath("assets.models.world", {});
    if (world_path.empty()) {
        KARMA_TRACE("game.client", "Game: collision cache skipped (world asset unresolved)");
        return;
    }

    std::vector<karma::geometry::SceneMesh> scene_meshes{};
    if (!karma::geometry::LoadScene(world_path, scene_meshes) || scene_meshes.empty()) {
        KARMA_TRACE("game.client",
                    "Game: collision cache skipped (LoadScene failed path='{}')",
                    world_path.string());
        return;
    }

    bool has_world_bounds = false;
    glm::vec2 world_min{};
    glm::vec2 world_max{};

    for (const auto& scene_mesh : scene_meshes) {
        if (scene_mesh.mesh.positions.empty()) {
            continue;
        }

        glm::vec3 minv = scene_mesh.mesh.positions[0];
        glm::vec3 maxv = scene_mesh.mesh.positions[0];
        for (const auto& p : scene_mesh.mesh.positions) {
            minv.x = std::min(minv.x, p.x);
            minv.y = std::min(minv.y, p.y);
            minv.z = std::min(minv.z, p.z);
            maxv.x = std::max(maxv.x, p.x);
            maxv.y = std::max(maxv.y, p.y);
            maxv.z = std::max(maxv.z, p.z);
        }

        if (!has_world_bounds) {
            world_min = glm::vec2(minv.x, minv.z);
            world_max = glm::vec2(maxv.x, maxv.z);
            has_world_bounds = true;
        } else {
            world_min.x = std::min(world_min.x, minv.x);
            world_min.y = std::min(world_min.y, minv.z);
            world_max.x = std::max(world_max.x, maxv.x);
            world_max.y = std::max(world_max.y, maxv.z);
        }

        const float height = maxv.y - minv.y;
        const float area_xz = (maxv.x - minv.x) * (maxv.z - minv.z);
        if (height < kMinObstacleHeight || area_xz < kMinObstacleArea) {
            continue;
        }
        if (area_xz > kHugeGroundArea && height < 3.0f) {
            continue;
        }

        tank_collision_rects_.push_back(TankObstacleRect{
            .min_x = minv.x,
            .max_x = maxv.x,
            .min_z = minv.z,
            .max_z = maxv.z,
            .min_y = minv.y,
            .max_y = maxv.y});
    }

    if (has_world_bounds) {
        tank_world_bounds_ready_ = true;
        tank_world_min_ = world_min;
        tank_world_max_ = world_max;
    }
    tank_collision_ready_ = tank_world_bounds_ready_ || !tank_collision_rects_.empty();
}

glm::vec3 Game::resolveTankCollision(const glm::vec3& previous_position,
                                     const glm::vec3& candidate_position) const {
    auto clamp_to_world = [this](glm::vec3 pos) {
        if (!tank_world_bounds_ready_) {
            return pos;
        }
        const float min_x = tank_world_min_.x + tank_collision_radius_;
        const float max_x = tank_world_max_.x - tank_collision_radius_;
        const float min_z = tank_world_min_.y + tank_collision_radius_;
        const float max_z = tank_world_max_.y - tank_collision_radius_;
        if (min_x < max_x) {
            pos.x = std::clamp(pos.x, min_x, max_x);
        }
        if (min_z < max_z) {
            pos.z = std::clamp(pos.z, min_z, max_z);
        }
        return pos;
    };

    glm::vec3 candidate = clamp_to_world(candidate_position);
    if (!positionBlockedByCollision(candidate)) {
        return candidate;
    }

    glm::vec3 x_only = clamp_to_world(glm::vec3{candidate.x, previous_position.y, previous_position.z});
    if (!positionBlockedByCollision(x_only)) {
        return x_only;
    }

    glm::vec3 z_only = clamp_to_world(glm::vec3{previous_position.x, previous_position.y, candidate.z});
    if (!positionBlockedByCollision(z_only)) {
        return z_only;
    }

    return clamp_to_world(previous_position);
}

bool Game::positionBlockedByCollision(const glm::vec3& position) const {
    if (!tank_collision_ready_) {
        return false;
    }

    for (const auto& obstacle : tank_collision_rects_) {
        if ((position.y + 0.25f) < obstacle.min_y || (position.y - 1.5f) > obstacle.max_y) {
            continue;
        }
        const float closest_x = std::clamp(position.x, obstacle.min_x, obstacle.max_x);
        const float closest_z = std::clamp(position.z, obstacle.min_z, obstacle.max_z);
        const float dx = position.x - closest_x;
        const float dz = position.z - closest_z;
        if ((dx * dx) + (dz * dz) <= (tank_collision_radius_ * tank_collision_radius_)) {
            return true;
        }
    }
    return false;
}

} // namespace bz3

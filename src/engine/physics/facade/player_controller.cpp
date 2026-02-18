#include "karma/physics/player_controller.hpp"

#include "physics/facade/facade_state.hpp"

#include <cmath>

namespace karma::physics {

class PlayerController::Impl {
 public:
    std::weak_ptr<detail::WorldState> state{};
    detail::BodyRuntimeHandle handle{};
    glm::vec3 half_extents{0.5f, 0.9f, 0.5f};
    glm::vec3 center{0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};
};

PlayerController::PlayerController() = default;
PlayerController::~PlayerController() = default;

PlayerController::PlayerController(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

PlayerController PlayerController::CreateFacadeHandle(std::shared_ptr<void> world_state,
                                                      uint64_t generation,
                                                      uint64_t body,
                                                      const glm::vec3& half_extents) {
    auto impl = std::make_unique<Impl>();
    impl->state = std::static_pointer_cast<detail::WorldState>(std::move(world_state));
    impl->handle.body = static_cast<physics::backend::BodyId>(body);
    impl->handle.generation = generation;
    if (half_extents.x > 0.0f && half_extents.y > 0.0f && half_extents.z > 0.0f) {
        impl->half_extents = half_extents;
    }
    return PlayerController(std::move(impl));
}

PlayerController::PlayerController(PlayerController&& other) noexcept = default;
PlayerController& PlayerController::operator=(PlayerController&& other) noexcept = default;

bool PlayerController::isValid() const {
    return impl_ && detail::IsHandleAlive(impl_->state, impl_->handle);
}

glm::vec3 PlayerController::getPosition() const {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    const auto state = detail::LockState(impl_->state);
    const auto* system = detail::ResolveSystem(state);
    if (!system) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    physics::backend::BodyTransform transform{};
    if (!system->getBodyTransform(impl_->handle.body, transform)) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return transform.position;
}

glm::quat PlayerController::getRotation() const {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const auto state = detail::LockState(impl_->state);
    const auto* system = detail::ResolveSystem(state);
    if (!system) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    physics::backend::BodyTransform transform{};
    if (!system->getBodyTransform(impl_->handle.body, transform)) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    return transform.rotation;
}

glm::vec3 PlayerController::getVelocity() const {
    if (!impl_) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return impl_->velocity;
}

glm::vec3 PlayerController::getForwardVector() const {
    const glm::quat rotation = getRotation();
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

glm::vec3 PlayerController::getCenter() const {
    if (!impl_) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return impl_->center;
}

glm::vec3 PlayerController::getHalfExtents() const {
    if (!impl_) {
        return glm::vec3(0.5f, 0.9f, 0.5f);
    }
    return impl_->half_extents;
}

void PlayerController::setHalfExtents(const glm::vec3& half_extents) {
    if (!impl_) {
        return;
    }
    if (!std::isfinite(half_extents.x) || !std::isfinite(half_extents.y) || !std::isfinite(half_extents.z)) {
        return;
    }
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
        return;
    }
    impl_->half_extents = half_extents;
}

void PlayerController::setCenter(const glm::vec3& center) {
    if (!impl_) {
        return;
    }
    impl_->center = center;
}

void PlayerController::update(float dt) {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return;
    }
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    const glm::vec3 position = getPosition();
    setPosition(position + impl_->velocity * dt);
}

void PlayerController::setPosition(const glm::vec3& position) {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return;
    }

    const auto state = detail::LockState(impl_->state);
    auto* system = detail::ResolveSystem(state);
    if (!system) {
        return;
    }

    physics::backend::BodyTransform transform{};
    if (!system->getBodyTransform(impl_->handle.body, transform)) {
        return;
    }
    transform.position = position;
    (void)system->setBodyTransform(impl_->handle.body, transform);
}

void PlayerController::setRotation(const glm::quat& rotation) {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return;
    }

    const auto state = detail::LockState(impl_->state);
    auto* system = detail::ResolveSystem(state);
    if (!system) {
        return;
    }

    physics::backend::BodyTransform transform{};
    if (!system->getBodyTransform(impl_->handle.body, transform)) {
        return;
    }
    transform.rotation = rotation;
    (void)system->setBodyTransform(impl_->handle.body, transform);
}

void PlayerController::setVelocity(const glm::vec3& velocity) {
    if (!impl_) {
        return;
    }
    impl_->velocity = velocity;
}

bool PlayerController::isGrounded() const {
    // Grounded semantics require a dedicated controller backend path and are intentionally deferred.
    return false;
}

void PlayerController::destroy() {
    if (!impl_) {
        return;
    }

    if (detail::IsHandleAlive(impl_->state, impl_->handle)) {
        const auto state = detail::LockState(impl_->state);
        auto* system = detail::ResolveSystem(state);
        if (system) {
            system->destroyBody(impl_->handle.body);
        }
    }

    detail::InvalidateHandle(impl_->handle);
}

} // namespace karma::physics

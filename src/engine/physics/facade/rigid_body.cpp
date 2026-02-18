#include "karma/physics/rigid_body.hpp"

#include "physics/facade/facade_state.hpp"

namespace karma::physics {

class RigidBody::Impl {
 public:
    std::weak_ptr<detail::WorldState> state{};
    detail::BodyRuntimeHandle handle{};
};

RigidBody::RigidBody() = default;
RigidBody::~RigidBody() = default;

RigidBody::RigidBody(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

RigidBody RigidBody::CreateFacadeHandle(std::shared_ptr<void> world_state,
                                        uint64_t generation,
                                        uint64_t body) {
    auto impl = std::make_unique<Impl>();
    impl->state = std::static_pointer_cast<detail::WorldState>(std::move(world_state));
    impl->handle.body = static_cast<physics::backend::BodyId>(body);
    impl->handle.generation = generation;
    return RigidBody(std::move(impl));
}

RigidBody::RigidBody(RigidBody&& other) noexcept = default;
RigidBody& RigidBody::operator=(RigidBody&& other) noexcept = default;

bool RigidBody::isValid() const {
    return impl_ && detail::IsHandleAlive(impl_->state, impl_->handle);
}

glm::vec3 RigidBody::getPosition() const {
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

glm::quat RigidBody::getRotation() const {
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

void RigidBody::setPosition(const glm::vec3& position) {
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

void RigidBody::setRotation(const glm::quat& rotation) {
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

void RigidBody::setTransform(const glm::vec3& position, const glm::quat& rotation) {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return;
    }

    const auto state = detail::LockState(impl_->state);
    auto* system = detail::ResolveSystem(state);
    if (!system) {
        return;
    }

    physics::backend::BodyTransform transform{};
    transform.position = position;
    transform.rotation = rotation;
    (void)system->setBodyTransform(impl_->handle.body, transform);
}

bool RigidBody::setGravityEnabled(bool enabled) {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return false;
    }

    const auto state = detail::LockState(impl_->state);
    auto* system = detail::ResolveSystem(state);
    if (!system) {
        return false;
    }

    return system->setBodyGravityEnabled(impl_->handle.body, enabled);
}

bool RigidBody::getGravityEnabled(bool& out_enabled) const {
    if (!impl_ || !detail::IsHandleAlive(impl_->state, impl_->handle)) {
        return false;
    }

    const auto state = detail::LockState(impl_->state);
    const auto* system = detail::ResolveSystem(state);
    if (!system) {
        return false;
    }

    return system->getBodyGravityEnabled(impl_->handle.body, out_enabled);
}

void RigidBody::destroy() {
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

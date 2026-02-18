#include "karma/physics/world.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/physics/physics_system.hpp"
#include "physics/facade_state.hpp"

#include <cmath>

#include <glm/geometric.hpp>

namespace karma::physics {
namespace {

bool IsFiniteVec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 SanitizedPlayerSize(const glm::vec3& size) {
    if (!IsFiniteVec3(size)) {
        return glm::vec3(0.5f, 0.9f, 0.5f);
    }
    if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return glm::vec3(0.5f, 0.9f, 0.5f);
    }
    return size;
}

} // namespace

class World::Impl {
 public:
    std::shared_ptr<detail::WorldState> state = std::make_shared<detail::WorldState>();
    std::unique_ptr<PlayerController> player_controller{};

    PhysicsSystem* system() {
        return detail::ResolveSystem(state);
    }

    const PhysicsSystem* system() const {
        return detail::ResolveSystem(state);
    }
};

World::World()
    : impl_(std::make_unique<Impl>()) {
    impl_->state->owned_system = std::make_unique<PhysicsSystem>();
    impl_->state->system = impl_->state->owned_system.get();
    impl_->state->owns_system = true;
}

World::World(PhysicsSystem& system)
    : impl_(std::make_unique<Impl>()) {
    impl_->state->system = &system;
    impl_->state->owns_system = false;
}

World::~World() {
    if (!impl_) {
        return;
    }

    if (impl_->player_controller) {
        impl_->player_controller->destroy();
        impl_->player_controller.reset();
    }

    if (impl_->state && impl_->state->owns_system) {
        auto* system = impl_->system();
        if (system && system->isInitialized()) {
            system->shutdown();
        }
        impl_->state->generation += 1;
    }
}

World::World(World&& other) noexcept = default;
World& World::operator=(World&& other) noexcept = default;

void World::setBackend(physics_backend::BackendKind backend) {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return;
    }
    system->setBackend(backend);
}

physics_backend::BackendKind World::requestedBackend() const {
    const auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return physics_backend::BackendKind::Auto;
    }
    return system->requestedBackend();
}

physics_backend::BackendKind World::selectedBackend() const {
    const auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return physics_backend::BackendKind::Auto;
    }
    return system->selectedBackend();
}

const char* World::selectedBackendName() const {
    const auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return physics_backend::BackendKindName(physics_backend::BackendKind::Auto);
    }
    return system->selectedBackendName();
}

bool World::isInitialized() const {
    const auto* system = impl_ ? impl_->system() : nullptr;
    return system && system->isInitialized();
}

void World::init() {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return;
    }
    system->init();
}

void World::shutdown() {
    if (!impl_) {
        return;
    }

    if (impl_->player_controller) {
        impl_->player_controller->destroy();
        impl_->player_controller.reset();
    }

    auto* system = impl_->system();
    if (system && impl_->state && impl_->state->owns_system) {
        system->shutdown();
    }

    if (impl_->state) {
        impl_->state->generation += 1;
    }
}

void World::beginFrame(float dt) {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return;
    }
    system->beginFrame(dt);
}

void World::simulateFixedStep(float fixed_dt) {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return;
    }
    system->simulateFixedStep(fixed_dt);
}

void World::endFrame() {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!system) {
        return;
    }
    system->endFrame();
}

void World::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }
    beginFrame(dt);
    simulateFixedStep(dt);
    endFrame();
}

void World::setGravity(float gravity) {
    if (!impl_ || !impl_->state) {
        return;
    }
    if (!std::isfinite(gravity)) {
        return;
    }
    impl_->state->gravity = gravity;
}

float World::gravity() const {
    if (!impl_ || !impl_->state) {
        return -9.8f;
    }
    return impl_->state->gravity;
}

RigidBody World::createBoxBody(const glm::vec3& half_extents,
                               float mass,
                               const glm::vec3& position,
                               const PhysicsMaterial& material) {
    (void)material;

    auto* system = impl_ ? impl_->system() : nullptr;
    if (!impl_ || !impl_->state || !system || !system->isInitialized()) {
        return RigidBody();
    }
    if (!IsFiniteVec3(half_extents) || half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
        return RigidBody();
    }
    if (!std::isfinite(mass) || mass < 0.0f) {
        return RigidBody();
    }
    if (!IsFiniteVec3(position)) {
        return RigidBody();
    }

    physics_backend::BodyDesc desc{};
    desc.is_static = mass <= 0.0f;
    desc.mass = desc.is_static ? 0.0f : mass;
    desc.gravity_enabled = (!desc.is_static) && std::fabs(impl_->state->gravity) > 1e-5f;
    desc.transform.position = position;
    desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    const auto body = system->createBody(desc);
    if (body == physics_backend::kInvalidBodyId) {
        return RigidBody();
    }

    return RigidBody::CreateFacadeHandle(impl_->state, impl_->state->generation, body);
}

StaticBody World::createStaticMesh(const std::string& mesh_path) {
    auto* system = impl_ ? impl_->system() : nullptr;
    if (!impl_ || !impl_->state || !system || !system->isInitialized() || mesh_path.empty()) {
        return StaticBody();
    }

    KARMA_TRACE("physics.system",
                "World facade: createStaticMesh('{}') currently creates placeholder static body",
                mesh_path);

    physics_backend::BodyDesc desc{};
    desc.is_static = true;
    desc.mass = 0.0f;
    desc.gravity_enabled = false;
    desc.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    const auto body = system->createBody(desc);
    if (body == physics_backend::kInvalidBodyId) {
        return StaticBody();
    }

    return StaticBody::CreateFacadeHandle(impl_->state, impl_->state->generation, body);
}

PlayerController& World::createPlayer() {
    return createPlayer(glm::vec3(0.5f, 0.9f, 0.5f));
}

PlayerController& World::createPlayer(const glm::vec3& size) {
    if (!impl_) {
        static PlayerController null_controller{};
        return null_controller;
    }

    const glm::vec3 resolved_size = SanitizedPlayerSize(size);
    auto* system = impl_->system();
    if (!impl_->state || !system || !system->isInitialized()) {
        impl_->player_controller = std::make_unique<PlayerController>();
        impl_->player_controller->setHalfExtents(resolved_size);
        return *impl_->player_controller;
    }

    if (impl_->player_controller && impl_->player_controller->isValid()) {
        impl_->player_controller->setHalfExtents(resolved_size);
        return *impl_->player_controller;
    }

    physics_backend::BodyDesc desc{};
    desc.is_static = false;
    desc.mass = 1.0f;
    desc.gravity_enabled = std::fabs(impl_->state->gravity) > 1e-5f;
    desc.rotation_locked = true;
    desc.translation_locked = false;
    desc.transform.position = glm::vec3(0.0f, resolved_size.y + 0.25f, 0.0f);
    desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    const auto body = system->createBody(desc);
    if (body == physics_backend::kInvalidBodyId) {
        impl_->player_controller = std::make_unique<PlayerController>();
        impl_->player_controller->setHalfExtents(resolved_size);
        return *impl_->player_controller;
    }

    impl_->player_controller = std::make_unique<PlayerController>(PlayerController::CreateFacadeHandle(
        impl_->state, impl_->state->generation, body, resolved_size));
    return *impl_->player_controller;
}

PlayerController* World::playerController() {
    return impl_ ? impl_->player_controller.get() : nullptr;
}

const PlayerController* World::playerController() const {
    return impl_ ? impl_->player_controller.get() : nullptr;
}

bool World::raycast(const glm::vec3& from,
                    const glm::vec3& to,
                    glm::vec3& hit_point,
                    glm::vec3& hit_normal) const {
    const auto* system = impl_ ? impl_->system() : nullptr;
    if (!system || !system->isInitialized()) {
        return false;
    }
    if (!IsFiniteVec3(from) || !IsFiniteVec3(to)) {
        return false;
    }

    const glm::vec3 direction = to - from;
    const float max_distance = glm::length(direction);
    if (!std::isfinite(max_distance) || max_distance <= 1e-6f) {
        return false;
    }

    physics_backend::RaycastHit hit{};
    if (!system->raycastClosest(from, direction, max_distance, hit)) {
        return false;
    }

    hit_point = hit.position;
    hit_normal = glm::vec3(0.0f, 0.0f, 0.0f);
    return true;
}

} // namespace karma::physics

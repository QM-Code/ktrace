#include "karma/physics/physics_system.hpp"

#include "karma/common/logging.hpp"
#include "karma/physics/backend.hpp"

#include <sstream>

namespace karma::physics {
namespace {

std::string CompiledBackendList() {
    const auto compiled = physics_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }

    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << physics_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

} // namespace

const char* PhysicsSystem::selectedBackendName() const {
    return physics_backend::BackendKindName(selected_backend_);
}

void PhysicsSystem::init() {
    if (initialized_) {
        return;
    }

    KARMA_TRACE("physics.system",
                "PhysicsSystem: creating backend requested='{}' compiled='{}'",
                physics_backend::BackendKindName(requested_backend_),
                CompiledBackendList());
    backend_ = physics_backend::CreateBackend(requested_backend_, &selected_backend_);
    if (!backend_) {
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    if (!backend_->init()) {
        KARMA_TRACE("physics.system",
                    "PhysicsSystem: backend '{}' init failed",
                    backend_->name());
        backend_.reset();
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    initialized_ = true;
    KARMA_TRACE("physics.system",
                "PhysicsSystem: backend ready selected='{}'",
                backend_->name());
}

void PhysicsSystem::shutdown() {
    if (!backend_) {
        initialized_ = false;
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    backend_->shutdown();
    backend_.reset();
    initialized_ = false;
    selected_backend_ = physics_backend::BackendKind::Auto;
}

void PhysicsSystem::beginFrame(float dt) {
    if (!backend_) {
        return;
    }
    backend_->beginFrame(dt);
}

void PhysicsSystem::simulateFixedStep(float fixed_dt) {
    if (!backend_) {
        return;
    }
    backend_->simulateFixedStep(fixed_dt);
}

void PhysicsSystem::endFrame() {
    if (!backend_) {
        return;
    }
    backend_->endFrame();
}

physics_backend::BodyId PhysicsSystem::createBody(const physics_backend::BodyDesc& desc) {
    if (!backend_) {
        return physics_backend::kInvalidBodyId;
    }
    return backend_->createBody(desc);
}

void PhysicsSystem::destroyBody(physics_backend::BodyId body) {
    if (!backend_) {
        return;
    }
    backend_->destroyBody(body);
}

bool PhysicsSystem::setBodyTransform(physics_backend::BodyId body,
                                     const physics_backend::BodyTransform& transform) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyTransform(body, transform);
}

bool PhysicsSystem::getBodyTransform(physics_backend::BodyId body,
                                     physics_backend::BodyTransform& out_transform) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyTransform(body, out_transform);
}

} // namespace karma::physics

#pragma once

namespace karma::physics {

enum class Backend {
    Jolt,
    PhysX
};

class PhysicsSystem {
 public:
    void setBackend(Backend backend) { backend_ = backend; }
    Backend backend() const { return backend_; }

    void init();
    void shutdown();
    void update(float dt);

 private:
    Backend backend_ = Backend::Jolt;
};

} // namespace karma::physics

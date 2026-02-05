#pragma once

namespace karma::ui {

enum class Backend {
    ImGui,
    RmlUi
};

class UiSystem {
 public:
    void setBackend(Backend backend) { backend_ = backend; }
    Backend backend() const { return backend_; }

    void init();
    void shutdown();
    void update(float dt);

 private:
    Backend backend_ = Backend::RmlUi;
};

} // namespace karma::ui

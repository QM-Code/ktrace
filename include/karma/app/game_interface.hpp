#pragma once

namespace karma::platform { class Window; }
namespace karma::renderer { class GraphicsDevice; }

namespace karma::app {

class GameInterface {
 public:
    virtual ~GameInterface() = default;
    virtual void onStart() = 0;
    virtual void onUpdate(float dt) = 0;
    virtual void onShutdown() = 0;

 protected:
    platform::Window* window = nullptr;
    renderer::GraphicsDevice* graphics = nullptr;

 private:
    friend class EngineApp;
    void bind(platform::Window& w, renderer::GraphicsDevice& g) {
        window = &w;
        graphics = &g;
    }
};

} // namespace karma::app

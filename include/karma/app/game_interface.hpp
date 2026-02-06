#pragma once

namespace karma::platform { class Window; }
namespace karma::renderer { class GraphicsDevice; class RenderSystem; }
namespace karma::input { class InputContext; }
namespace karma::ecs { class World; }
namespace karma::scene { class Scene; }

namespace karma::app {

class GameInterface {
 public:
    virtual ~GameInterface() = default;
    virtual void onStart() = 0;
    virtual void onUpdate(float dt) = 0;
    virtual void onUiStart() {}
    virtual void onUiUpdate(float dt) { (void)dt; }
    virtual void onUiShutdown() {}
    virtual void onShutdown() = 0;

 protected:
    platform::Window* window = nullptr;
    renderer::GraphicsDevice* graphics = nullptr;
    renderer::RenderSystem* render = nullptr;
    ecs::World* world = nullptr;
    scene::Scene* scene = nullptr;
    input::InputContext* input = nullptr;

 private:
    friend class EngineApp;
    void bind(platform::Window& w, renderer::GraphicsDevice& g, renderer::RenderSystem& r, ecs::World& world_ref, scene::Scene& s,
              input::InputContext& in) {
        window = &w;
        graphics = &g;
        render = &r;
        world = &world_ref;
        scene = &s;
        input = &in;
    }
};

} // namespace karma::app

#pragma once

namespace karma::app {
struct EngineContext;

class GameInterface {
public:
    virtual ~GameInterface() = default;

    virtual void onStart() {}
    virtual void onFixedUpdate(float dt) { (void)dt; }
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onShutdown() {}

    virtual bool shouldQuit() const { return false; }

protected:
    EngineContext *context() { return context_; }
    const EngineContext *context() const { return context_; }

private:
    friend class EngineApp;
    EngineContext *context_ = nullptr;
};
} // namespace karma::app

#include "karma/app/engine_app.hpp"
#include "karma/app/engine_config.hpp"
#include "karma/app/game_interface.hpp"

class NullGame final : public karma::app::GameInterface {
public:
    void onStart() override {}
    void onUpdate(float) override {}
    bool shouldQuit() const override { return true; }
};

int main() {
    karma::app::EngineApp app;
    NullGame game;
    app.start(game, app.config());
    app.tick();
    return 0;
}

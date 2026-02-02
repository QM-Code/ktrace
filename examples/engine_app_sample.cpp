#include "karma/karma.h"

namespace bz3::examples {

class EmptyGame final : public karma::app::GameInterface {
public:
    void onStart() override {}
    void onUpdate(float) override {}
    void onShutdown() override {}
};

} // namespace bz3::examples

int main() {
    karma::app::EngineApp app;
    bz3::examples::EmptyGame game;

    karma::app::EngineConfig config;
    config.window.title = "BZ3 EngineApp Sample";

    app.start(game, config);
    while (app.isRunning()) {
        app.tick();
    }

    return 0;
}

#include "game.hpp"

#include "client/net/client_connection.hpp"
#include "karma/common/logging.hpp"
#include "karma/input/input_system.hpp"
#include "karma/ui/ui_draw_context.hpp"

#include <spdlog/spdlog.h>
#include <cstdlib>
#include <string>
#include <utility>

namespace bz3 {

Game::Game(GameStartupOptions options) : startup_(std::move(options)) {}

Game::~Game() = default;

void Game::onStart() {
    if (input) {
        input->setMode(karma::input::InputMode::Roaming);
    }

    if (!startup_.connect_on_start) {
        return;
    }

    connection_ = std::make_unique<client::net::ClientConnection>(
        startup_.connect_addr,
        startup_.connect_port,
        startup_.player_name);
    if (!connection_->start()) {
        KARMA_TRACE("net.client",
                    "Game: startup connect failed addr='{}' port={} name='{}'",
                    startup_.connect_addr,
                    startup_.connect_port,
                    startup_.player_name);
        connection_.reset();
    }
}

void Game::onUpdate(float dt) {
    (void)dt;
    if (connection_) {
        connection_->poll();
        if (connection_->shouldExit()) {
            spdlog::error("Game: network join was rejected, closing client.");
            std::exit(1);
        }
    }
}

void Game::onUiUpdate(float dt, karma::ui::UiDrawContext& ui) {
    (void)dt;
    karma::ui::UiDrawContext::TextPanel panel{};
    panel.title = "BZ3 Client";
    panel.lines.push_back("Engine-owned UI lifecycle (game-submitted content)");
    panel.lines.push_back("");
    panel.lines.push_back("Player: " + startup_.player_name);
    if (startup_.connect_on_start) {
        panel.lines.push_back("Connect target: " + startup_.connect_addr + ":" +
                              std::to_string(startup_.connect_port));
        const bool connected = connection_ && connection_->isConnected();
        panel.lines.push_back(std::string("Connection: ") + (connected ? "connected" : "connecting"));
    } else {
        panel.lines.push_back("Connection: offline");
    }
    ui.addTextPanel(std::move(panel));
}

void Game::onShutdown() {
    if (connection_) {
        connection_->shutdown();
        connection_.reset();
    }
}

} // namespace bz3

#include "game.hpp"

#include "client/net/client_connection.hpp"
#include "karma/common/logging.hpp"
#include "karma/input/input_system.hpp"

#include <spdlog/spdlog.h>
#include <cstdlib>
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

void Game::onShutdown() {
    if (connection_) {
        connection_->shutdown();
        connection_.reset();
    }
}

} // namespace bz3

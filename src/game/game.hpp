#pragma once

#include "karma/app/game_interface.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace bz3::client::net {
class ClientConnection;
}

namespace bz3 {

struct GameStartupOptions {
    std::string player_name{};
    std::string connect_addr{};
    uint16_t connect_port = 0;
    bool connect_on_start = false;
};

class Game final : public karma::app::GameInterface {
 public:
    explicit Game(GameStartupOptions options = {});
    ~Game() override;
    void onStart() override;
    void onUpdate(float dt) override;
    void onShutdown() override;

 private:
    GameStartupOptions startup_{};
    std::unique_ptr<client::net::ClientConnection> connection_{};
};

} // namespace bz3

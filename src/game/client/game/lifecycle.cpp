#include "game.hpp"

#include "client/domain/tank_drive_controller.hpp"
#include "client/net/connection.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/ecs/world.hpp"
#include "karma/input/input_system.hpp"
#include "karma/ui/ui_draw_context.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace bz3 {

Game::Game(GameStartupOptions options) : startup_(std::move(options)) {}

Game::~Game() = default;

void Game::onStart() {
    tank_enabled_ = karma::common::config::ReadBoolConfig({"gameplay.tank.enabled"}, false);
    if (tank_enabled_) {
        static_cast<void>(ensureLocalTankEntity());
    } else {
        destroyLocalTankEntity();
    }
    syncInputMode();

    if (!startup_.connect_on_start) {
        return;
    }

    connection_ = std::make_unique<client::net::ClientConnection>(
        startup_.connect_addr,
        startup_.connect_port,
        startup_.player_name,
        startup_.auth_payload,
        [this](client::net::AudioEvent event) { onAudioEvent(event); });
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
    if (tank_enabled_) {
        static_cast<void>(ensureLocalTankEntity());
    }
    if (connection_) {
        connection_->poll();
        if (connection_->shouldExit()) {
            spdlog::error("Game: network join was rejected, closing client.");
            std::exit(1);
        }
    }

    if (!input) {
        chat_entry_focused_ = false;
        console_toggle_was_down_ = false;
        escape_was_down_ = false;
        chat_was_down_ = false;
        spawn_was_down_ = false;
        fire_was_down_ = false;
        return;
    }

    const bool console_down = input->global().actionDown("console");
    const bool escape_down = input->global().actionDown("quickMenu");
    const bool chat_down = input->game().actionDown("chat");
    const bool console_pressed =
        input->global().actionPressed("console") || (console_down && !console_toggle_was_down_);
    const bool escape_pressed =
        input->global().actionPressed("quickMenu") || (escape_down && !escape_was_down_);
    const bool chat_pressed =
        input->game().actionPressed("chat") || (chat_down && !chat_was_down_);

    if (console_pressed) {
        console_visible_ = !console_visible_;
        if (!console_visible_) {
            chat_entry_focused_ = false;
        }
    } else if (chat_pressed) {
        console_visible_ = true;
        chat_entry_focused_ = true;
    } else if (console_visible_ && escape_pressed) {
        console_visible_ = false;
        chat_entry_focused_ = false;
    }

    syncInputMode();

    const bool spawn_down = input->game().actionDown("spawn");
    const bool fire_down = input->game().actionDown("fire");

    const bool spawn_pressed =
        input->game().actionPressed("spawn") || (spawn_down && !spawn_was_down_);
    const bool fire_pressed =
        input->game().actionPressed("fire") || (fire_down && !fire_was_down_);

    const bool gameplay_input_enabled = !console_visible_ && !chat_entry_focused_;

    if (tank_enabled_) {
        updateLocalTank(dt, gameplay_input_enabled);
    }

    if (gameplay_input_enabled && spawn_pressed) {
        if (connection_ && connection_->isConnected()) {
            static_cast<void>(connection_->sendRequestPlayerSpawn());
        }
    }

    if (gameplay_input_enabled && fire_pressed) {
        // Play local fire immediately for responsive feedback.
        onAudioEvent(client::net::AudioEvent::ShotFire);
        if (connection_ && connection_->isConnected()) {
            static_cast<void>(connection_->sendCreateShot());
        }
    }

    console_toggle_was_down_ = console_down;
    escape_was_down_ = escape_down;
    chat_was_down_ = chat_down;
    spawn_was_down_ = spawn_down;
    fire_was_down_ = fire_down;
}

void Game::onUiUpdate(float dt, karma::ui::UiDrawContext& ui) {
    (void)dt;
    const bool connected = connection_ && connection_->isConnected();
    const bool hud_visible = connected || !console_visible_;

    if (hud_visible) {
        karma::ui::UiDrawContext::TextPanel hud{};
        hud.title = "BZ3 HUD";
        hud.lines.push_back("Player: " + startup_.player_name);
        if (startup_.connect_on_start) {
            hud.lines.push_back("Connect target: " + startup_.connect_addr + ":" +
                                std::to_string(startup_.connect_port));
            hud.lines.push_back(std::string("Connection: ") + (connected ? "connected" : "connecting"));
        } else {
            hud.lines.push_back("Connection: offline");
        }
        hud.lines.push_back(console_visible_ ? "Console: open" : "Console: closed");
        hud.lines.push_back(chat_entry_focused_ ? "Chat entry focus: active" : "Chat entry focus: idle");
        hud.lines.push_back("HUD rule: connected || !console");
        if (tank_enabled_ && tank_drive_) {
            const auto& tank = tank_drive_->state();
            hud.lines.push_back("Tank: arrow keys to drive");
            hud.lines.push_back("Tank pos: (" + std::to_string(tank.position.x) + ", " +
                                std::to_string(tank.position.y) + ", " +
                                std::to_string(tank.position.z) + ")");
        } else if (!tank_enabled_) {
            hud.lines.push_back("Mode: roaming camera (tank disabled)");
        } else {
            hud.lines.push_back("Tank: unavailable (player model failed to load)");
        }
        ui.addTextPanel(std::move(hud));
    }

    if (console_visible_) {
        karma::ui::UiDrawContext::TextPanel console{};
        console.title = "BZ3 Console";
        console.x = 72.0f;
        console.y = 84.0f;
        console.lines.push_back("Console parity slice (engine lifecycle + game state mapping).");
        console.lines.push_back("` toggle: open/close");
        console.lines.push_back("Chat action: focus chat entry + open console");
        console.lines.push_back("Esc: close console");
        console.lines.push_back(chat_entry_focused_ ? "Chat entry focus is active." : "Chat entry focus is idle.");
        console.lines.push_back("Gameplay input blocked while console/chat focus is active.");
        ui.addTextPanel(std::move(console));
    }
}

void Game::onShutdown() {
    if (connection_) {
        connection_->shutdown();
        connection_.reset();
    }
    destroyLocalTankEntity();
    console_visible_ = false;
    chat_entry_focused_ = false;
    syncInputMode();
}

void Game::syncInputMode() {
    if (!input) {
        return;
    }

    if (world && tank_entity_.isValid() && world->isAlive(tank_entity_)) {
        input->setMode(karma::input::InputMode::Game);
        return;
    }

    const bool ui_focus_active = console_visible_ || chat_entry_focused_;
    input->setMode(ui_focus_active ? karma::input::InputMode::Game
                                   : karma::input::InputMode::Roaming);
}

} // namespace bz3

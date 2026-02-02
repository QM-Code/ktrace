#include "client/server/server_connector.hpp"

#include <utility>

#include "game/engine/client_engine.hpp"
#include "client/game.hpp"
#include "karma/common/config_helpers.hpp"
#include "spdlog/spdlog.h"

ServerConnector::ServerConnector(ClientEngine &engine,
                                 std::string playerName,
                                 std::string worldDir,
                                 std::unique_ptr<Game> &game)
    : engine(engine),
      game(game),
      defaultPlayerName(std::move(playerName)),
      worldDir(std::move(worldDir)) {}

bool ServerConnector::connect(const std::string &targetHost,
                              uint16_t targetPort,
                              const std::string &playerName,
                              bool registeredUser,
                              bool communityAdmin,
                              bool localAdmin) {
    std::string status = "Connecting to " + targetHost + ":" + std::to_string(targetPort) + "...";
    auto &browser = engine.ui->console();
    browser.setStatus(status, false);
    spdlog::info("Attempting to connect to {}:{}", targetHost, targetPort);

    const std::string resolvedName = playerName.empty() ? defaultPlayerName : playerName;
    const uint16_t connectTimeoutMs = karma::config::ReadUInt16Config({"network.ConnectTimeoutMs"}, 2000);
    if (engine.network->connect(targetHost, targetPort, static_cast<int>(connectTimeoutMs))) {
        spdlog::info("Connected to server at {}:{}", targetHost, targetPort);
        spdlog::info("Requesting join for name '{}'", resolvedName);
        joinPending_ = true;
        pending_.host = targetHost;
        pending_.port = targetPort;
        pending_.name = resolvedName;
        pending_.registeredUser = registeredUser;
        pending_.communityAdmin = communityAdmin;
        pending_.localAdmin = localAdmin;

        ClientMsg_JoinRequest joinReq{};
        joinReq.clientId = 0;
        joinReq.name = resolvedName;
        joinReq.protocolVersion = NET_PROTOCOL_VERSION;
        engine.network->send<ClientMsg_JoinRequest>(joinReq);
        browser.setStatus("Validating player name...", false);
        return true;
    }

    spdlog::error("Failed to connect to server at {}:{}", targetHost, targetPort);
    std::string errorMsg = "Unable to reach " + targetHost + ":" + std::to_string(targetPort) + ".";
    browser.setStatus(errorMsg, true);
    browser.showErrorDialog(errorMsg);
    browser.setConnectionState({});
    return false;
}

void ServerConnector::handleJoinResponse(const ServerMsg_JoinResponse &response) {
    auto &browser = engine.ui->console();
    if (!joinPending_) {
        spdlog::warn("ServerConnector: Received join response with no pending join");
        return;
    }
    if (!response.accepted) {
        const std::string reason = response.reason.empty()
            ? "Join rejected by server."
            : response.reason;
        spdlog::warn("ServerConnector: Join rejected: {}", reason);
        browser.setStatus(reason, true);
        browser.showErrorDialog(reason);
        joinRejectionDialogShown_ = true;
        browser.setConnectionState({});
        joinPending_ = false;
        suppressDisconnectDialog_ = true;
        engine.network->disconnect(reason);
        return;
    }

    spdlog::info("Join accepted for '{}'", pending_.name);
    spdlog::info("Join mode: {} user", pending_.registeredUser ? "registered" : "anonymous");
    spdlog::info("Join flags: community_admin={}, local_admin={}",
                 pending_.communityAdmin, pending_.localAdmin);
    browser.setConnectionState({true, pending_.host, pending_.port});
    game = std::make_unique<Game>(engine,
                                  pending_.name,
                                  worldDir,
                                  pending_.registeredUser,
                                  pending_.communityAdmin,
                                  pending_.localAdmin);
    spdlog::trace("Game initialized successfully");

    ClientMsg_PlayerJoin joinMsg{};
    joinMsg.clientId = 0;
    joinMsg.name = pending_.name;
    joinMsg.protocolVersion = NET_PROTOCOL_VERSION;
    joinMsg.ip = "";
    joinMsg.registeredUser = pending_.registeredUser;
    joinMsg.communityAdmin = pending_.communityAdmin;
    joinMsg.localAdmin = pending_.localAdmin;
    engine.network->send<ClientMsg_PlayerJoin>(joinMsg);

    joinPending_ = false;
    browser.hide();
}

bool ServerConnector::consumeSuppressDisconnectDialog() {
    if (!suppressDisconnectDialog_) {
        return false;
    }
    suppressDisconnectDialog_ = false;
    return true;
}

bool ServerConnector::consumeJoinRejectionDialogShown() {
    if (!joinRejectionDialogShown_) {
        return false;
    }
    joinRejectionDialogShown_ = false;
    return true;
}

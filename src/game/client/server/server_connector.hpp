#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "game/net/messages.hpp"

class ClientEngine;
class Game;

class ServerConnector {
public:
    ServerConnector(ClientEngine &engine,
                    std::string playerName,
                    std::string worldDir,
                    std::unique_ptr<Game> &game);

    bool connect(const std::string &host,
                 uint16_t port,
                 const std::string &playerName,
                 bool registeredUser,
                 bool communityAdmin,
                 bool localAdmin);
    void handleJoinResponse(const ServerMsg_JoinResponse &response);
    bool isJoinPending() const { return joinPending_; }
    bool consumeSuppressDisconnectDialog();
    bool consumeJoinRejectionDialogShown();

private:
    struct PendingJoin {
        std::string host;
        uint16_t port = 0;
        std::string name;
        bool registeredUser = false;
        bool communityAdmin = false;
        bool localAdmin = false;
    };
    ClientEngine &engine;
    std::unique_ptr<Game> &game;
    std::string defaultPlayerName;
    std::string worldDir;
    bool joinPending_ = false;
    bool suppressDisconnectDialog_ = false;
    bool joinRejectionDialogShown_ = false;
    PendingJoin pending_{};
};

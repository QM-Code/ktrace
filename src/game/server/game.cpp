#include "server/game.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <utility>
#include "plugin.hpp"

void Game::addClient(std::unique_ptr<Client> client) {
    clients.push_back(std::move(client));
}

void Game::removeClient(client_id id) {
    clients.erase(
        std::remove_if(
            clients.begin(),
            clients.end(),
            [id](const std::unique_ptr<Client> &c) {
                if (c->isEqual(id)) {
                    return true;
                } else {
                    return false;
                }
            }
        ),
        clients.end()
    );
}

Client *Game::getClient(client_id id) {
    for (const auto &client : clients) {
        if (client->isEqual(id)) {
            return client.get();
        }
    }
    return nullptr;
}

Client *Game::getClientByName(const std::string &name) {
    for (const auto &client : clients) {
        if (client->isEqual(name)) {
            return client.get();
        }
    }
    return nullptr;
}

Game::Game(ServerEngine &engine,
           std::string serverName,
           std::string worldName,
           karma::json::Value worldConfig,
           std::string worldDir,
           bool enableWorldZipping)
    : engine(engine) {
    world = new ServerWorldSession(*this,
                      std::move(serverName),
                      std::move(worldName),
                      std::move(worldConfig),
                      std::move(worldDir),
                      enableWorldZipping);
    chat = new Chat(*this);
}

Game::~Game() {
    clients.clear();

    shots.clear();

    delete world;
    delete chat;
}

void Game::update(TimeUtils::duration deltaTime) {
    for (const auto &reqMsg : engine.network->consumeMessages<ClientMsg_JoinRequest>()) {
        spdlog::debug("Game::update: Join request from id {} (name='{}')",
                      reqMsg.clientId,
                      reqMsg.name);
        bool nameTaken = getClientByName(reqMsg.name) != nullptr;
        if (!nameTaken) {
            for (const auto &entry : pendingJoinNames_) {
                if (entry.second == reqMsg.name) {
                    nameTaken = true;
                    break;
                }
            }
        }

        ServerMsg_JoinResponse response{};
        if (reqMsg.protocolVersion != NET_PROTOCOL_VERSION) {
            response.accepted = false;
            response.reason = "Protocol version mismatch.";
            engine.network->send<ServerMsg_JoinResponse>(reqMsg.clientId, &response);
            engine.network->disconnectClient(reqMsg.clientId, response.reason);
            continue;
        }
        if (nameTaken) {
            response.accepted = false;
            response.reason = "Name already in use.";
            engine.network->send<ServerMsg_JoinResponse>(reqMsg.clientId, &response);
            engine.network->disconnectClient(reqMsg.clientId, response.reason);
            continue;
        }

        response.accepted = true;
        response.reason = "";
        engine.network->send<ServerMsg_JoinResponse>(reqMsg.clientId, &response);
        approvedJoinIds_.insert(reqMsg.clientId);
        pendingJoinNames_.insert({reqMsg.clientId, reqMsg.name});
    }

    for (const auto &connMsg : engine.network->consumeMessages<ClientMsg_PlayerJoin>()) {
        spdlog::debug("Game::update: New client connection with id {} from IP {}",
                      connMsg.clientId,
                      connMsg.ip);
        if (approvedJoinIds_.find(connMsg.clientId) == approvedJoinIds_.end()) {
            engine.network->disconnectClient(connMsg.clientId, "Join request required.");
            continue;
        }
        auto pendingIt = pendingJoinNames_.find(connMsg.clientId);
        if (pendingIt == pendingJoinNames_.end() || pendingIt->second != connMsg.name) {
            engine.network->disconnectClient(connMsg.clientId, "Join request mismatch.");
            approvedJoinIds_.erase(connMsg.clientId);
            if (pendingIt != pendingJoinNames_.end()) {
                pendingJoinNames_.erase(pendingIt);
            }
            continue;
        }
        pendingJoinNames_.erase(pendingIt);
        approvedJoinIds_.erase(connMsg.clientId);
        if (getClientByName(connMsg.name)) {
            engine.network->disconnectClient(connMsg.clientId, "Name already in use.");
            continue;
        }

        if (connMsg.protocolVersion != NET_PROTOCOL_VERSION) {
            spdlog::warn("Game::update: Client id {} protocol mismatch (client {}, server {})",
                         connMsg.clientId,
                         connMsg.protocolVersion,
                         NET_PROTOCOL_VERSION);
            engine.network->disconnectClient(connMsg.clientId, "Protocol version mismatch.");
            continue;
        }

        world->sendWorldInit(connMsg.clientId);
        auto newClient = std::make_unique<Client>(
            *this,
            connMsg.clientId,
            connMsg.ip,
            connMsg.name,
            connMsg.registeredUser,
            connMsg.communityAdmin,
            connMsg.localAdmin);

        // Send existing players to the newcomer
        for (const auto &client : clients) {
            ServerMsg_PlayerJoin existingMsg;
            existingMsg.clientId = client->getId();
            existingMsg.state = client->getState();
            engine.network->send<ServerMsg_PlayerJoin>(connMsg.clientId, &existingMsg);
        }

        addClient(std::move(newClient));
    }

    for (const auto &disconnMsg : engine.network->consumeMessages<ClientMsg_PlayerLeave>()) {
        spdlog::info("Game::update: Client with id {} disconnected", disconnMsg.clientId);
        pendingJoinNames_.erase(disconnMsg.clientId);
        approvedJoinIds_.erase(disconnMsg.clientId);
        removeClient(disconnMsg.clientId);

        Event_PlayerLeave event;
        event.playerId = disconnMsg.clientId;
        g_triggerPluginEvent<Event_PlayerLeave>(EventType_PlayerLeave, event);
    }

    for (const auto &chatMsg : engine.network->consumeMessages<ClientMsg_Chat>()) {
        chat->handleMessage(chatMsg);

        Event_Chat event;
        event.fromId = chatMsg.clientId;
        event.toId = chatMsg.toId;
        event.message = chatMsg.text;
        bool handled = g_triggerPluginEvent<Event_Chat>(EventType_Chat, event);
        if (handled) {
            return;
        }

        ServerMsg_Chat serverChatMsg;
        serverChatMsg.fromId = chatMsg.clientId;
        serverChatMsg.toId = chatMsg.toId;
        serverChatMsg.text = chatMsg.text;

        if (chatMsg.toId == BROADCAST_CLIENT_ID) {
            engine.network->sendExcept<ServerMsg_Chat>(chatMsg.clientId, &serverChatMsg);
        } else {
            engine.network->send<ServerMsg_Chat>(chatMsg.toId, &serverChatMsg);
        }
    }

    for (const auto &locMsg : engine.network->consumeMessages<ClientMsg_PlayerLocation>()) {
        Client *client = getClient(locMsg.clientId);
        if (!client) {
            continue;
        }

        client->applyLocation(locMsg.position, locMsg.rotation);
    }

    for (const auto &spawnMsg : engine.network->consumeMessages<ClientMsg_RequestPlayerSpawn>()) {
        Client *client = getClient(spawnMsg.clientId);
        if (!client) {
            continue;
        }

        Event_PlayerSpawn event;
        event.playerId = spawnMsg.clientId;
        bool handled = g_triggerPluginEvent<Event_PlayerSpawn>(EventType_PlayerSpawn, event);
        if (handled) {
            continue;
        }

        client->trySpawn(world->pickSpawnLocation());
    }

    for (const auto &shotMsg : engine.network->consumeMessages<ClientMsg_CreateShot>()) {
        shot_id globalShotId = 0;

        // Make the shot here before pushing it (using unique pointer)
        auto shot = std::make_unique<Shot>(
            *this,
            shotMsg.clientId,
            shotMsg.localShotId,
            shotMsg.position,
            shotMsg.velocity
        );
        globalShotId = shot->getGlobalId();
        shots.push_back(std::move(shot));

        Event_CreateShot event;
        event.shotId = globalShotId;
        g_triggerPluginEvent<Event_CreateShot>(EventType_CreateShot, event);
    }

    for (auto it = shots.begin(); it != shots.end(); ) {
        Shot *shot = it->get();
        shot->update(deltaTime);

        bool expired = shot->isExpired();
        bool hit = false;
        if (!expired) {
            for (const auto &client : clients) {
                if (client->getState().alive == false) {
                    continue;
                }

                if (shot->hits(client.get())) {
                    client_id victimId = client->getId();
                    client_id killerId = shot->getOwnerId();

                    // Apply authoritative score changes
                    if (Client* killer = getClient(killerId)) {
                        if (killerId != victimId) {
                            killer->setScore(killer->getScore() + 1);
                        }
                    }

                    Event_PlayerDie event;
                    event.victimPlayerId = victimId;
                    event.shotId = shot->getGlobalId();
                    bool handled = g_triggerPluginEvent<Event_PlayerDie>(EventType_PlayerDie, event);

                    if (handled) {
                        break;
                    }

                    client->setScore(client->getScore() - 1);
                    client->die();
                    hit = true;
                    break;
                }
            }
        }

        if (expired || hit) {
            it = shots.erase(it);
        } else {
            ++it;
        }
    }

    world->update();
}

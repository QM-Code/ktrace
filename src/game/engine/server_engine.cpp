#include "game/engine/server_engine.hpp"
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include <functional>

ServerEngine::ServerEngine(uint16_t serverPort) {
    network = new ServerNetwork(serverPort);
    physics = new PhysicsWorld();
}

ServerEngine::~ServerEngine() {
    delete network;
    delete physics;
}

void ServerEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    network->update();
}

void ServerEngine::lateUpdate(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
    network->flushPeekedMessages();
}

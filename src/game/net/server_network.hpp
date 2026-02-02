#pragma once
#include "game/net/messages.hpp"
#include "game/net/backend.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

class ServerNetwork {

private:
    std::unique_ptr<game::net::ServerBackend> backend_;

    void sendImpl(client_id clientId, const ServerMsg &input, bool flush);

public:
    ServerNetwork(
        uint16_t port,
        int maxClients = 50,
        int numChannels = 2
    );
    ~ServerNetwork();

    void flushPeekedMessages();
    void update();

    template<typename T> T* peekMessage(std::function<bool(const T&)> predicate = [](const T&) { return true; }) {
        static_assert(std::is_base_of_v<ClientMsg, T>, "T must be a subclass of ClientMsg");

        if (!backend_) {
            return nullptr;
        }
        auto &receivedMessages = backend_->receivedMessages();
        for (auto &msgData : receivedMessages) {
            // Check if the message is of type ClientMsg or if it is of type T
            if (msgData.msg && msgData.msg->type == T::Type) {
                auto* casted = static_cast<T*>(msgData.msg);

                if (predicate(*casted)) {
                    msgData.peeked = true;
                    return casted;
                }
            }
        }

        return nullptr;
    };

    template<typename T> std::vector<T> consumeMessages(std::function<bool(const T&)> predicate = [](const T&) { return true; }) {
        static_assert(std::is_base_of_v<ClientMsg, T>, "T must be a subclass of ClientMsg");

        std::vector<T> results;
        if (!backend_) {
            return results;
        }
        auto &receivedMessages = backend_->receivedMessages();
        auto it = receivedMessages.begin();
        while (it != receivedMessages.end()) {
            if (it->msg && it->msg->type == T::Type) {
                auto* casted = static_cast<T*>(it->msg);
                if (predicate(*casted)) {
                    results.push_back(*casted);
                    delete it->msg;
                    it = receivedMessages.erase(it);
                    continue;
                }
            }
            ++it;
        }
        return results;
    }

    template<typename T> void send(client_id clientId, const T *input) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        if (clientId == BROADCAST_CLIENT_ID) {
            sendAll<T>(input);
            return;
        }

        if (!backend_) {
            return;
        }
        const auto clients = backend_->getClients();
        if (std::find(clients.begin(), clients.end(), clientId) == clients.end()) {
            return;
        }

        sendImpl(clientId, *input, false);
    };

    template<typename T> void sendExcept(client_id client, const T *input) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        if (!backend_) {
            return;
        }
        const auto clients = backend_->getClients();
        for (const auto& id : clients) {
            if (id != client) {
                send<T>(id, input);
            }
        }
    };

    template<typename T> void sendAll(const T *input) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        if (!backend_) {
            return;
        }
        const auto clients = backend_->getClients();
        for (const auto& id : clients) {
            send<T>(id, input);
        }
    };

    void disconnectClient(client_id clientId, const std::string &reason = "");
    std::vector<client_id> getClients() const;
};

#pragma once

#include "karma/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using client_id = uint32_t;
using shot_id = uint32_t;

constexpr client_id SERVER_CLIENT_ID = 0;
constexpr client_id BROADCAST_CLIENT_ID = 1;
constexpr client_id FIRST_CLIENT_ID = 2;

constexpr uint32_t NET_PROTOCOL_VERSION = 5;

struct PlayerState {
    std::string name;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
    bool alive;
    int score = 0;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
    PlayerParameters params;
};

/*
 * Server messages
 */

enum ServerMsg_Type {
    ServerMsg_Type_PLAYER_JOIN,
    ServerMsg_Type_PLAYER_LEAVE,
    ServerMsg_Type_PLAYER_STATE,
    ServerMsg_Type_PLAYER_PARAMETERS,
    ServerMsg_Type_PLAYER_LOCATION,
    ServerMsg_Type_PLAYER_SPAWN,
    ServerMsg_Type_PLAYER_DEATH,
    ServerMsg_Type_SET_SCORE,
    ServerMsg_Type_CREATE_SHOT,
    ServerMsg_Type_REMOVE_SHOT,
    ServerMsg_Type_INIT,
    ServerMsg_Type_CHAT,
    ServerMsg_Type_JOIN_RESPONSE
};

struct ServerMsg {
    ServerMsg_Type type;
    virtual ~ServerMsg() = default;
};

struct ServerMsg_PlayerJoin : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_JOIN;
    ServerMsg_PlayerJoin() { type = Type; }
    client_id clientId;
    PlayerState state;
};

struct ServerMsg_PlayerLeave : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_LEAVE;
    ServerMsg_PlayerLeave() { type = Type; }
    client_id clientId;
};

struct ServerMsg_PlayerState : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_STATE;
    ServerMsg_PlayerState() { type = Type; }
    client_id clientId;
    PlayerState state;
};

struct ServerMsg_PlayerParameters : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_PARAMETERS;
    ServerMsg_PlayerParameters() { type = Type; }
    client_id clientId;
    PlayerParameters params;
};

struct ServerMsg_PlayerLocation : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_LOCATION;
    ServerMsg_PlayerLocation() { type = Type; }
    client_id clientId;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
};

struct ServerMsg_PlayerSpawn : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_SPAWN;
    ServerMsg_PlayerSpawn() { type = Type; }
    client_id clientId;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
};

struct ServerMsg_PlayerDeath : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_DEATH;
    ServerMsg_PlayerDeath() { type = Type; }
    client_id clientId;
};

struct ServerMsg_SetScore : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_SET_SCORE;
    ServerMsg_SetScore() { type = Type; }
    client_id clientId;
    int32_t score;
};

struct ServerMsg_CreateShot : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_CREATE_SHOT;
    ServerMsg_CreateShot() { type = Type; }
    shot_id globalShotId;
    glm::vec3 position;
    glm::vec3 velocity;
};

struct ServerMsg_RemoveShot : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_REMOVE_SHOT;
    ServerMsg_RemoveShot() { type = Type; }
    shot_id shotId;
    bool isGlobalId;
};

struct ServerMsg_Chat : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_CHAT;
    ServerMsg_Chat() { type = Type; }
    client_id fromId;
    client_id toId;
    std::string text;
};

struct ServerMsg_Init : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_INIT;
    ServerMsg_Init() { type = Type; }
    client_id clientId;
    std::string serverName;
    std::string worldName;
    uint32_t protocolVersion = NET_PROTOCOL_VERSION;
    std::vector<std::string> features;
    PlayerParameters defaultPlayerParams;
    std::vector<std::byte> worldData;
};

struct ServerMsg_JoinResponse : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_JOIN_RESPONSE;
    ServerMsg_JoinResponse() { type = Type; }
    bool accepted = false;
    std::string reason;
};

/*
 * Client messages
 */

enum ClientMsg_Type {
    ClientMsg_Type_PLAYER_JOIN,
    ClientMsg_Type_PLAYER_LEAVE,
    ClientMsg_Type_REQUEST_PLAYER_SPAWN,
    ClientMsg_Type_PLAYER_LOCATION,
    ClientMsg_Type_CREATE_SHOT,
    ClientMsg_Type_CHAT,
    ClientMsg_Type_JOIN_REQUEST
};

struct ClientMsg {
    ClientMsg_Type type;
    client_id clientId;
    virtual ~ClientMsg() = default;
};

struct ClientMsg_PlayerJoin : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_JOIN;
    ClientMsg_PlayerJoin() { type = Type; }
    std::string ip;
    std::string name;
    uint32_t protocolVersion = NET_PROTOCOL_VERSION;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};

struct ClientMsg_JoinRequest : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_JOIN_REQUEST;
    ClientMsg_JoinRequest() { type = Type; }
    std::string name;
    uint32_t protocolVersion = NET_PROTOCOL_VERSION;
};

struct ClientMsg_PlayerLeave : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_LEAVE;
    ClientMsg_PlayerLeave() { type = Type; }
};

struct ClientMsg_RequestPlayerSpawn : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_REQUEST_PLAYER_SPAWN;
    ClientMsg_RequestPlayerSpawn() { type = Type; }
};

struct ClientMsg_PlayerLocation : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_LOCATION;
    ClientMsg_PlayerLocation() { type = Type; }
    glm::vec3 position;
    glm::quat rotation;
};

struct ClientMsg_CreateShot : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_CREATE_SHOT;
    ClientMsg_CreateShot() { type = Type; }
    shot_id localShotId;
    glm::vec3 position;
    glm::vec3 velocity;
};

struct ClientMsg_Chat : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_CHAT;
    ClientMsg_Chat() { type = Type; }
    client_id toId;
    std::string text;
};

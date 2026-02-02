#include "game/net/proto_codec.hpp"

#include "messages.pb.h"

#include <string>

namespace net {
namespace {

std::vector<std::byte> toBytes(const std::string &buffer) {
    const auto *ptr = reinterpret_cast<const std::byte*>(buffer.data());
    return std::vector<std::byte>(ptr, ptr + buffer.size());
}

void decodeVec3(const karma::Vec3 &input, glm::vec3 &output) {
    output.x = input.x();
    output.y = input.y();
    output.z = input.z();
}

void encodeVec3(const glm::vec3 &input, karma::Vec3 *output) {
    output->set_x(input.x);
    output->set_y(input.y);
    output->set_z(input.z);
}

void decodeQuat(const karma::Quat &input, glm::quat &output) {
    output.w = input.w();
    output.x = input.x();
    output.y = input.y();
    output.z = input.z();
}

void encodeQuat(const glm::quat &input, karma::Quat *output) {
    output->set_w(input.w);
    output->set_x(input.x);
    output->set_y(input.y);
    output->set_z(input.z);
}

void decodePlayerState(const karma::PlayerState &input, PlayerState &output) {
    output.name = input.name();
    decodeVec3(input.position(), output.position);
    decodeQuat(input.rotation(), output.rotation);
    decodeVec3(input.velocity(), output.velocity);
    output.alive = input.alive();
    output.score = input.score();
    output.registeredUser = input.registered_user();
    output.communityAdmin = input.community_admin();
    output.localAdmin = input.local_admin();
    output.params.clear();
    for (const auto &[key, val] : input.params().params()) {
        output.params[key] = val;
    }
}

void encodePlayerState(const PlayerState &input, karma::PlayerState *output) {
    output->set_name(input.name);
    encodeVec3(input.position, output->mutable_position());
    encodeQuat(input.rotation, output->mutable_rotation());
    encodeVec3(input.velocity, output->mutable_velocity());
    output->set_alive(input.alive);
    output->set_score(input.score);
    output->set_registered_user(input.registeredUser);
    output->set_community_admin(input.communityAdmin);
    output->set_local_admin(input.localAdmin);
    auto *params = output->mutable_params();
    for (const auto &[key, val] : input.params) {
        (*params->mutable_params())[key] = val;
    }
}

} // namespace

std::unique_ptr<ServerMsg> decodeServerMsg(const std::byte *data, std::size_t size) {
    if (!data || size == 0) {
        return nullptr;
    }

    karma::ServerMsg msg;
    if (!msg.ParseFromArray(data, static_cast<int>(size))) {
        return nullptr;
    }

    switch (msg.payload_case()) {

    case karma::ServerMsg::kPlayerJoin: {
        auto out = std::make_unique<ServerMsg_PlayerJoin>();
        out->clientId = msg.player_join().client_id();
        decodePlayerState(msg.player_join().state(), out->state);
        return out;
    }

    case karma::ServerMsg::kPlayerLeave: {
        auto out = std::make_unique<ServerMsg_PlayerLeave>();
        out->clientId = msg.player_leave().client_id();
        return out;
    }

    case karma::ServerMsg::kPlayerState: {
        auto out = std::make_unique<ServerMsg_PlayerState>();
        out->clientId = msg.player_state().client_id();
        decodePlayerState(msg.player_state().state(), out->state);
        return out;
    }

    case karma::ServerMsg::kPlayerParameters: {
        auto out = std::make_unique<ServerMsg_PlayerParameters>();
        out->clientId = msg.player_parameters().client_id();
        for (const auto& [key, val] : msg.player_parameters().params().params()) {
            out->params[key] = val;
        }
        return out;
    }

    case karma::ServerMsg::kPlayerLocation: {
        auto out = std::make_unique<ServerMsg_PlayerLocation>();
        out->clientId = msg.player_location().client_id();
        decodeVec3(msg.player_location().position(), out->position);
        decodeQuat(msg.player_location().rotation(), out->rotation);
        decodeVec3(msg.player_location().velocity(), out->velocity);
        return out;
    }

    case karma::ServerMsg::kPlayerSpawn: {
        auto out = std::make_unique<ServerMsg_PlayerSpawn>();
        out->clientId = msg.player_spawn().client_id();
        decodeVec3(msg.player_spawn().position(), out->position);
        decodeQuat(msg.player_spawn().rotation(), out->rotation);
        decodeVec3(msg.player_spawn().velocity(), out->velocity);
        return out;
    }

    case karma::ServerMsg::kPlayerDeath: {
        auto out = std::make_unique<ServerMsg_PlayerDeath>();
        out->clientId = msg.player_death().client_id();
        return out;
    }

    case karma::ServerMsg::kSetScore: {
        auto out = std::make_unique<ServerMsg_SetScore>();
        out->clientId = msg.set_score().client_id();
        out->score = msg.set_score().score();
        return out;
    }

    case karma::ServerMsg::kCreateShot: {
        auto out = std::make_unique<ServerMsg_CreateShot>();
        out->globalShotId = msg.create_shot().global_shot_id();
        decodeVec3(msg.create_shot().position(), out->position);
        decodeVec3(msg.create_shot().velocity(), out->velocity);
        return out;
    }

    case karma::ServerMsg::kRemoveShot: {
        auto out = std::make_unique<ServerMsg_RemoveShot>();
        out->shotId = msg.remove_shot().shot_id();
        out->isGlobalId = msg.remove_shot().is_global_id();
        return out;
    }

    case karma::ServerMsg::kInit: {
        auto out = std::make_unique<ServerMsg_Init>();
        out->clientId = msg.init().client_id();
        out->serverName = msg.init().server_name();
        out->worldName = msg.init().world_name();
        out->protocolVersion = msg.init().protocol_version();
        out->features.assign(msg.init().features().begin(), msg.init().features().end());
        for (const auto& [key, val] : msg.init().default_player_params().params()) {
            out->defaultPlayerParams[key] = val;
        }

        const std::string &worldDataStr = msg.init().world_data();
        const auto *dataPtr = reinterpret_cast<const std::byte*>(worldDataStr.data());
        out->worldData = std::vector<std::byte>(dataPtr, dataPtr + worldDataStr.size());
        return out;
    }

    case karma::ServerMsg::kChat: {
        auto out = std::make_unique<ServerMsg_Chat>();
        out->fromId = msg.chat().from_id();
        out->toId = msg.chat().to_id();
        out->text = msg.chat().text();
        return out;
    }
    case karma::ServerMsg::kJoinResponse: {
        auto out = std::make_unique<ServerMsg_JoinResponse>();
        out->accepted = msg.join_response().accepted();
        out->reason = msg.join_response().reason();
        return out;
    }

    default:
        return nullptr;
    }
}

std::unique_ptr<ClientMsg> decodeClientMsg(const std::byte *data, std::size_t size) {
    if (!data || size == 0) {
        return nullptr;
    }

    karma::ClientMsg msg;
    if (!msg.ParseFromArray(data, static_cast<int>(size))) {
        return nullptr;
    }

    switch (msg.payload_case()) {

    case karma::ClientMsg::kChat: {
        auto out = std::make_unique<ClientMsg_Chat>();
        out->clientId = msg.client_id();
        out->toId = msg.chat().to_id();
        out->text = msg.chat().text();
        return out;
    }

    case karma::ClientMsg::kPlayerLocation: {
        auto out = std::make_unique<ClientMsg_PlayerLocation>();
        out->clientId = msg.client_id();
        decodeVec3(msg.player_location().position(), out->position);
        decodeQuat(msg.player_location().rotation(), out->rotation);
        return out;
    }

    case karma::ClientMsg::kRequestPlayerSpawn: {
        auto out = std::make_unique<ClientMsg_RequestPlayerSpawn>();
        out->clientId = msg.client_id();
        return out;
    }

    case karma::ClientMsg::kCreateShot: {
        auto out = std::make_unique<ClientMsg_CreateShot>();
        out->clientId = msg.client_id();
        out->localShotId = msg.create_shot().local_shot_id();
        decodeVec3(msg.create_shot().position(), out->position);
        decodeVec3(msg.create_shot().velocity(), out->velocity);
        return out;
    }

    case karma::ClientMsg::kPlayerJoin: {
        auto out = std::make_unique<ClientMsg_PlayerJoin>();
        out->clientId = msg.client_id();
        out->ip = msg.player_join().ip();
        out->name = msg.player_join().name();
        out->protocolVersion = msg.player_join().protocol_version();
        out->registeredUser = msg.player_join().registered_user();
        out->communityAdmin = msg.player_join().community_admin();
        out->localAdmin = msg.player_join().local_admin();
        return out;
    }
    case karma::ClientMsg::kJoinRequest: {
        auto out = std::make_unique<ClientMsg_JoinRequest>();
        out->clientId = msg.client_id();
        out->name = msg.join_request().name();
        out->protocolVersion = msg.join_request().protocol_version();
        return out;
    }

    case karma::ClientMsg::kPlayerLeave: {
        auto out = std::make_unique<ClientMsg_PlayerLeave>();
        out->clientId = msg.client_id();
        return out;
    }

    default:
        return nullptr;
    }
}

std::optional<std::vector<std::byte>> encodeClientMsg(const ClientMsg &input) {
    karma::ClientMsg msg;
    msg.set_client_id(input.clientId);

    switch (input.type) {
    case ClientMsg_Type_PLAYER_JOIN: {
        msg.set_type(karma::ClientMsg::PLAYER_JOIN);
        const auto &typed = static_cast<const ClientMsg_PlayerJoin&>(input);
        auto *join = msg.mutable_player_join();
        join->set_ip(typed.ip);
        join->set_name(typed.name);
        join->set_protocol_version(typed.protocolVersion);
        join->set_registered_user(typed.registeredUser);
        join->set_community_admin(typed.communityAdmin);
        join->set_local_admin(typed.localAdmin);
        break;
    }
    case ClientMsg_Type_CHAT: {
        msg.set_type(karma::ClientMsg::CHAT);
        const auto &typed = static_cast<const ClientMsg_Chat&>(input);
        auto* chat = msg.mutable_chat();
        chat->set_to_id(typed.toId);
        chat->set_text(typed.text);
        break;
    }
    case ClientMsg_Type_PLAYER_LOCATION: {
        msg.set_type(karma::ClientMsg::PLAYER_LOCATION);
        const auto &typed = static_cast<const ClientMsg_PlayerLocation&>(input);
        auto* loc = msg.mutable_player_location();
        encodeVec3(typed.position, loc->mutable_position());
        encodeQuat(typed.rotation, loc->mutable_rotation());
        break;
    }
    case ClientMsg_Type_REQUEST_PLAYER_SPAWN: {
        msg.set_type(karma::ClientMsg::REQUEST_PLAYER_SPAWN);
        msg.mutable_request_player_spawn();
        break;
    }
    case ClientMsg_Type_CREATE_SHOT: {
        msg.set_type(karma::ClientMsg::CREATE_SHOT);
        const auto &typed = static_cast<const ClientMsg_CreateShot&>(input);
        auto* shot = msg.mutable_create_shot();
        shot->set_local_shot_id(typed.localShotId);
        encodeVec3(typed.position, shot->mutable_position());
        encodeVec3(typed.velocity, shot->mutable_velocity());
        break;
    }
    case ClientMsg_Type_PLAYER_LEAVE: {
        msg.set_type(karma::ClientMsg::PLAYER_LEAVE);
        msg.mutable_player_leave();
        break;
    }
    case ClientMsg_Type_JOIN_REQUEST: {
        msg.set_type(karma::ClientMsg::JOIN_REQUEST);
        const auto &typed = static_cast<const ClientMsg_JoinRequest&>(input);
        auto *join = msg.mutable_join_request();
        join->set_name(typed.name);
        join->set_protocol_version(typed.protocolVersion);
        break;
    }
    default:
        return std::nullopt;
    }

    std::string buffer;
    msg.SerializeToString(&buffer);
    return toBytes(buffer);
}

std::optional<std::vector<std::byte>> encodeServerMsg(const ServerMsg &input) {
    karma::ServerMsg msg;

    switch (input.type) {
    case ServerMsg_Type_PLAYER_JOIN: {
        msg.set_type(karma::ServerMsg::PLAYER_JOIN);
        const auto &typed = static_cast<const ServerMsg_PlayerJoin&>(input);
        auto* join = msg.mutable_player_join();
        join->set_client_id(typed.clientId);
        encodePlayerState(typed.state, join->mutable_state());
        break;
    }
    case ServerMsg_Type_PLAYER_LEAVE: {
        msg.set_type(karma::ServerMsg::PLAYER_LEAVE);
        const auto &typed = static_cast<const ServerMsg_PlayerLeave&>(input);
        msg.mutable_player_leave()->set_client_id(typed.clientId);
        break;
    }
    case ServerMsg_Type_PLAYER_STATE: {
        msg.set_type(karma::ServerMsg::PLAYER_STATE);
        const auto &typed = static_cast<const ServerMsg_PlayerState&>(input);
        auto* ps = msg.mutable_player_state();
        ps->set_client_id(typed.clientId);
        encodePlayerState(typed.state, ps->mutable_state());
        break;
    }
    case ServerMsg_Type_PLAYER_PARAMETERS: {
        msg.set_type(karma::ServerMsg::PLAYER_PARAMETERS);
        const auto &typed = static_cast<const ServerMsg_PlayerParameters&>(input);
        auto* pp = msg.mutable_player_parameters();
        pp->set_client_id(typed.clientId);
        auto* params = pp->mutable_params();
        for (const auto& [key, val] : typed.params) {
            (*params->mutable_params())[key] = val;
        }
        break;
    }
    case ServerMsg_Type_PLAYER_LOCATION: {
        msg.set_type(karma::ServerMsg::PLAYER_LOCATION);
        const auto &typed = static_cast<const ServerMsg_PlayerLocation&>(input);
        auto* loc = msg.mutable_player_location();
        loc->set_client_id(typed.clientId);
        encodeVec3(typed.position, loc->mutable_position());
        encodeQuat(typed.rotation, loc->mutable_rotation());
        encodeVec3(typed.velocity, loc->mutable_velocity());
        break;
    }
    case ServerMsg_Type_PLAYER_SPAWN: {
        msg.set_type(karma::ServerMsg::PLAYER_SPAWN);
        const auto &typed = static_cast<const ServerMsg_PlayerSpawn&>(input);
        auto* spawn = msg.mutable_player_spawn();
        spawn->set_client_id(typed.clientId);
        encodeVec3(typed.position, spawn->mutable_position());
        encodeQuat(typed.rotation, spawn->mutable_rotation());
        encodeVec3(typed.velocity, spawn->mutable_velocity());
        break;
    }
    case ServerMsg_Type_PLAYER_DEATH: {
        msg.set_type(karma::ServerMsg::PLAYER_DEATH);
        const auto &typed = static_cast<const ServerMsg_PlayerDeath&>(input);
        msg.mutable_player_death()->set_client_id(typed.clientId);
        break;
    }
    case ServerMsg_Type_SET_SCORE: {
        msg.set_type(karma::ServerMsg::SET_SCORE);
        const auto &typed = static_cast<const ServerMsg_SetScore&>(input);
        auto* setScore = msg.mutable_set_score();
        setScore->set_client_id(typed.clientId);
        setScore->set_score(typed.score);
        break;
    }
    case ServerMsg_Type_CREATE_SHOT: {
        msg.set_type(karma::ServerMsg::CREATE_SHOT);
        const auto &typed = static_cast<const ServerMsg_CreateShot&>(input);
        auto* shot = msg.mutable_create_shot();
        shot->set_global_shot_id(typed.globalShotId);
        encodeVec3(typed.position, shot->mutable_position());
        encodeVec3(typed.velocity, shot->mutable_velocity());
        break;
    }
    case ServerMsg_Type_REMOVE_SHOT: {
        msg.set_type(karma::ServerMsg::REMOVE_SHOT);
        const auto &typed = static_cast<const ServerMsg_RemoveShot&>(input);
        auto* remove = msg.mutable_remove_shot();
        remove->set_shot_id(typed.shotId);
        remove->set_is_global_id(typed.isGlobalId);
        break;
    }
    case ServerMsg_Type_CHAT: {
        msg.set_type(karma::ServerMsg::CHAT);
        const auto &typed = static_cast<const ServerMsg_Chat&>(input);
        auto* chat = msg.mutable_chat();
        chat->set_from_id(typed.fromId);
        chat->set_to_id(typed.toId);
        chat->set_text(typed.text);
        break;
    }
    case ServerMsg_Type_INIT: {
        msg.set_type(karma::ServerMsg::INIT);
        const auto &typed = static_cast<const ServerMsg_Init&>(input);
        auto* init = msg.mutable_init();
        init->set_client_id(typed.clientId);
        init->set_server_name(typed.serverName);
        init->set_world_name(typed.worldName);
        init->set_protocol_version(typed.protocolVersion);
        for (const auto &feature : typed.features) {
            init->add_features(feature);
        }
        auto* params = init->mutable_default_player_params();
        for (const auto& [key, val] : typed.defaultPlayerParams) {
            (*params->mutable_params())[key] = val;
        }
        init->set_world_data(typed.worldData.data(), typed.worldData.size());
        break;
    }
    case ServerMsg_Type_JOIN_RESPONSE: {
        msg.set_type(karma::ServerMsg::JOIN_RESPONSE);
        const auto &typed = static_cast<const ServerMsg_JoinResponse&>(input);
        auto *resp = msg.mutable_join_response();
        resp->set_accepted(typed.accepted);
        resp->set_reason(typed.reason);
        break;
    }
    default:
        return std::nullopt;
    }

    std::string buffer;
    msg.SerializeToString(&buffer);
    return toBytes(buffer);
}

} // namespace net

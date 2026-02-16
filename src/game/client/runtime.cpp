#include "client/runtime.hpp"

#include "karma/app/backend_resolution.hpp"
#include "karma/app/engine_app.hpp"
#include "karma/cli/client_runtime_options.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/json.hpp"

#include "game.hpp"

#include <openssl/evp.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bz3::client {

namespace {

struct CommunityCredential {
    std::string username{};
    std::string password_hash{};
    std::string salt{};
};

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string StripIpv6Brackets(std::string host) {
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
        return host.substr(1, host.size() - 2);
    }
    return host;
}

std::string ExtractHostFromCredentialKey(std::string key) {
    if (key.empty()) {
        return {};
    }
    const std::size_t scheme_split = key.find("://");
    if (scheme_split != std::string::npos) {
        key = key.substr(scheme_split + 3);
    }
    const std::size_t path_split = key.find_first_of("/?#");
    if (path_split != std::string::npos) {
        key = key.substr(0, path_split);
    }
    if (key.empty()) {
        return {};
    }

    if (key.front() == '[') {
        const std::size_t closing = key.find(']');
        if (closing == std::string::npos) {
            return {};
        }
        return ToLowerAscii(key.substr(1, closing - 1));
    }

    const std::size_t port_split = key.find(':');
    if (port_split != std::string::npos) {
        key = key.substr(0, port_split);
    }
    return ToLowerAscii(key);
}

bool IsLanLikeHost(const std::string& host_in) {
    const std::string host = ToLowerAscii(StripIpv6Brackets(host_in));
    if (host == "localhost" || host == "::1" || host == "127.0.0.1") {
        return true;
    }
    if (host.rfind("10.", 0) == 0 || host.rfind("192.168.", 0) == 0) {
        return true;
    }
    if (host.rfind("172.", 0) == 0) {
        const std::size_t next_dot = host.find('.', 4);
        if (next_dot != std::string::npos) {
            try {
                const int second_octet = std::stoi(host.substr(4, next_dot - 4));
                return second_octet >= 16 && second_octet <= 31;
            } catch (...) {
                return false;
            }
        }
    }
    return false;
}

std::optional<CommunityCredential> ReadCommunityCredentialEntry(const karma::json::Value& credentials,
                                                                const std::string& key) {
    const auto it = credentials.find(key);
    if (it == credentials.end() || !it->is_object()) {
        return std::nullopt;
    }
    CommunityCredential out{};
    if (const auto username_it = it->find("username");
        username_it != it->end() && username_it->is_string()) {
        out.username = username_it->get<std::string>();
    }
    if (const auto hash_it = it->find("passwordHash");
        hash_it != it->end() && hash_it->is_string()) {
        out.password_hash = hash_it->get<std::string>();
    }
    if (const auto salt_it = it->find("salt");
        salt_it != it->end() && salt_it->is_string()) {
        out.salt = salt_it->get<std::string>();
    }
    return out;
}

std::optional<CommunityCredential> ResolveCommunityCredential(
    const std::optional<karma::cli::ClientServerEndpoint>& endpoint,
    const std::string& server_option_raw) {
    const auto* credentials = karma::config::ConfigStore::Get("communityCredentials");
    if (!credentials || !credentials->is_object()) {
        return std::nullopt;
    }

    if (!server_option_raw.empty()) {
        if (auto exact = ReadCommunityCredentialEntry(*credentials, server_option_raw)) {
            return exact;
        }
    }

    std::string endpoint_host{};
    if (endpoint.has_value()) {
        endpoint_host = ToLowerAscii(StripIpv6Brackets(endpoint->host));
        const std::string endpoint_host_with_port = endpoint->host + ":" + std::to_string(endpoint->port);
        if (auto exact = ReadCommunityCredentialEntry(*credentials, endpoint_host_with_port)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, endpoint->host)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, "http://" + endpoint_host_with_port)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, "https://" + endpoint_host_with_port)) {
            return exact;
        }
    }

    if (!endpoint_host.empty()) {
        for (auto it = credentials->begin(); it != credentials->end(); ++it) {
            if (!it->is_object()) {
                continue;
            }
            const std::string key = it.key();
            if (ToLowerAscii(key) == "lan") {
                continue;
            }
            const std::string key_host = ExtractHostFromCredentialKey(key);
            if (!key_host.empty() && key_host == endpoint_host) {
                CommunityCredential out{};
                if (const auto username_it = it->find("username");
                    username_it != it->end() && username_it->is_string()) {
                    out.username = username_it->get<std::string>();
                }
                if (const auto hash_it = it->find("passwordHash");
                    hash_it != it->end() && hash_it->is_string()) {
                    out.password_hash = hash_it->get<std::string>();
                }
                if (const auto salt_it = it->find("salt");
                    salt_it != it->end() && salt_it->is_string()) {
                    out.salt = salt_it->get<std::string>();
                }
                return out;
            }
        }
    }

    if (endpoint_host.empty() || IsLanLikeHost(endpoint_host)) {
        if (auto lan = ReadCommunityCredentialEntry(*credentials, "LAN")) {
            return lan;
        }
        if (auto lan = ReadCommunityCredentialEntry(*credentials, "lan")) {
            return lan;
        }
    }

    return std::nullopt;
}

std::string BuildStructuredAuthPayload(std::string username,
                                       std::optional<std::string> password,
                                       std::optional<std::string> passhash) {
    if (username.empty()) {
        return {};
    }
    karma::json::Value auth = karma::json::Object();
    auth["username"] = std::move(username);
    if (passhash.has_value() && !passhash->empty()) {
        auth["passhash"] = std::move(*passhash);
    } else if (password.has_value() && !password->empty()) {
        auth["password"] = std::move(*password);
    }
    if (!auth.contains("password") && !auth.contains("passhash")) {
        return {};
    }
    return karma::json::Dump(auth);
}

bool HashPasswordPBKDF2Sha256(const std::string& password,
                              const std::string& salt,
                              std::string* out_hex_digest) {
    if (!out_hex_digest || salt.empty()) {
        return false;
    }

    constexpr int kIterations = 100000;
    constexpr std::size_t kDigestLen = 32;
    std::array<unsigned char, kDigestLen> digest{};
    const int ok = PKCS5_PBKDF2_HMAC(password.data(),
                                     static_cast<int>(password.size()),
                                     reinterpret_cast<const unsigned char*>(salt.data()),
                                     static_cast<int>(salt.size()),
                                     kIterations,
                                     EVP_sha256(),
                                     static_cast<int>(digest.size()),
                                     digest.data());
    if (ok != 1) {
        return false;
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string hex{};
    hex.resize(digest.size() * 2);
    for (size_t i = 0; i < digest.size(); ++i) {
        const unsigned char value = digest[i];
        hex[i * 2] = kHex[(value >> 4) & 0x0F];
        hex[(i * 2) + 1] = kHex[value & 0x0F];
    }
    *out_hex_digest = std::move(hex);
    return true;
}

glm::vec3 ReadRequiredVec3(const char* path) {
    const auto values = karma::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must have 3 elements");
    }
    return {values[0], values[1], values[2]};
}

glm::vec4 ReadRequiredColor(const char* path) {
    const auto values = karma::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() == 3) {
        return {values[0], values[1], values[2], 1.0f};
    }
    if (values.size() == 4) {
        return {values[0], values[1], values[2], values[3]};
    }
    throw std::runtime_error(std::string("Config '") + path + "' must have 3 or 4 elements");
}

bz3::GameStartupOptions ResolveGameStartupOptions(const karma::cli::ClientAppOptions& options) {
    bz3::GameStartupOptions startup{};

    std::string connect_error{};
    const auto endpoint =
        karma::cli::ResolveClientServerEndpoint(options.server, options.server_explicit, &connect_error);

    const auto credential = ResolveCommunityCredential(endpoint, options.server);
    const std::string default_player_name = (credential.has_value() && !credential->username.empty())
        ? credential->username
        : karma::config::ReadStringConfig({"userDefaults.username"}, "Player");
    startup.player_name = karma::cli::ResolveClientPlayerName(
        options.username,
        options.username_explicit,
        default_player_name);

    if (options.password_explicit) {
        std::optional<std::string> password = options.password;
        std::optional<std::string> passhash = std::nullopt;
        const bool can_hash_with_salt = credential.has_value()
            && !credential->salt.empty()
            && (!options.username_explicit
                || credential->username.empty()
                || credential->username == startup.player_name);
        if (can_hash_with_salt) {
            std::string computed_hash{};
            if (HashPasswordPBKDF2Sha256(options.password, credential->salt, &computed_hash)) {
                password = std::nullopt;
                passhash = std::move(computed_hash);
            }
        }
        startup.auth_payload = BuildStructuredAuthPayload(
            startup.player_name,
            std::move(password),
            std::move(passhash));
    } else if (credential.has_value()
               && !credential->password_hash.empty()
               && (!options.username_explicit
                   || credential->username.empty()
                   || credential->username == startup.player_name)) {
        startup.auth_payload = BuildStructuredAuthPayload(
            startup.player_name,
            std::nullopt,
            credential->password_hash);
    }

    if (!endpoint.has_value()) {
        if (options.server_explicit) {
            throw std::runtime_error(connect_error.empty()
                    ? std::string("Client --server is invalid.")
                    : connect_error);
        }
        return startup;
    }

    startup.connect_addr = endpoint->host;
    startup.connect_port = endpoint->port;
    startup.connect_on_start = true;
    return startup;
}

} // namespace

int RunRuntime(const karma::cli::ClientAppOptions& options) {
    karma::app::EngineConfig config;
    config.window.title = karma::config::ReadRequiredStringConfig("platform.WindowTitle");
    config.window.width = karma::config::ReadRequiredUInt16Config("platform.WindowWidth");
    config.window.height = karma::config::ReadRequiredUInt16Config("platform.WindowHeight");
    config.window.preferredVideoDriver = karma::app::ReadPreferredVideoDriverFromConfig();
    config.window.fullscreen = karma::config::ReadRequiredBoolConfig("platform.Fullscreen");
    config.window.wayland_libdecor = karma::config::ReadRequiredBoolConfig("platform.WaylandLibdecor");
    config.vsync = karma::config::ReadRequiredBoolConfig("platform.VSync");
    config.default_camera.position = ReadRequiredVec3("roamingMode.camera.default.position");
    config.default_camera.target = ReadRequiredVec3("roamingMode.camera.default.target");
    config.default_camera.fov_y_degrees =
        karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.fovYDegrees");
    config.default_camera.near_clip = karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.nearClip");
    config.default_camera.far_clip = karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.farClip");
    config.default_light.direction = ReadRequiredVec3("roamingMode.graphics.lighting.sunDirection");
    config.default_light.color = ReadRequiredColor("roamingMode.graphics.lighting.sunColor");
    config.default_light.ambient = ReadRequiredColor("roamingMode.graphics.lighting.ambientColor");
    config.default_light.unlit = karma::config::ReadFloatConfig({"roamingMode.graphics.lighting.unlit"}, 0.0f);
    config.default_light.shadow.enabled = karma::config::ReadBoolConfig(
        {"roamingMode.graphics.lighting.shadows.enabled"},
        config.default_light.shadow.enabled);
    config.default_light.shadow.strength = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.strength"},
        config.default_light.shadow.strength);
    config.default_light.shadow.bias = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.bias"},
        config.default_light.shadow.bias);
    config.default_light.shadow.receiver_bias_scale = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.receiverBiasScale"},
        config.default_light.shadow.receiver_bias_scale);
    config.default_light.shadow.normal_bias_scale = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.normalBiasScale"},
        config.default_light.shadow.normal_bias_scale);
    config.default_light.shadow.raster_depth_bias = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.rasterDepthBias"},
        config.default_light.shadow.raster_depth_bias);
    config.default_light.shadow.raster_slope_bias = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.rasterSlopeBias"},
        config.default_light.shadow.raster_slope_bias);
    config.default_light.shadow.extent = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.extent"},
        config.default_light.shadow.extent);
    config.default_light.shadow.map_size = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.mapSize"},
        static_cast<uint16_t>(config.default_light.shadow.map_size)));
    config.default_light.shadow.pcf_radius = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pcfRadius"},
        static_cast<uint16_t>(config.default_light.shadow.pcf_radius)));
    config.default_light.shadow.triangle_budget = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.triangleBudget"},
        static_cast<uint16_t>(config.default_light.shadow.triangle_budget)));
    config.default_light.shadow.update_every_frames = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.updateEveryFrames"},
        static_cast<uint16_t>(config.default_light.shadow.update_every_frames)));
    config.default_light.shadow.point_map_size = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointMapSize"},
        static_cast<uint16_t>(config.default_light.shadow.point_map_size)));
    config.default_light.shadow.point_max_shadow_lights = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointMaxShadowLights"},
        static_cast<uint16_t>(config.default_light.shadow.point_max_shadow_lights)));
    config.default_light.shadow.point_faces_per_frame_budget = static_cast<int>(karma::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointFacesPerFrameBudget"},
        static_cast<uint16_t>(config.default_light.shadow.point_faces_per_frame_budget)));
    config.default_light.shadow.point_constant_bias = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointConstantBias"},
        config.default_light.shadow.point_constant_bias);
    config.default_light.shadow.point_slope_bias_scale = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointSlopeBiasScale"},
        config.default_light.shadow.point_slope_bias_scale);
    config.default_light.shadow.point_normal_bias_scale = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointNormalBiasScale"},
        config.default_light.shadow.point_normal_bias_scale);
    config.default_light.shadow.point_receiver_bias_scale = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointReceiverBiasScale"},
        config.default_light.shadow.point_receiver_bias_scale);
    config.default_light.shadow.local_light_distance_damping = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightDistanceDamping"},
        config.default_light.shadow.local_light_distance_damping);
    config.default_light.shadow.local_light_range_falloff_exponent = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightRangeFalloffExponent"},
        config.default_light.shadow.local_light_range_falloff_exponent);
    config.default_light.shadow.ao_affects_local_lights = karma::config::ReadBoolConfig(
        {"roamingMode.graphics.lighting.shadows.aoAffectsLocalLights"},
        config.default_light.shadow.ao_affects_local_lights);
    config.default_light.shadow.local_light_directional_shadow_lift_strength = karma::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightDirectionalShadowLiftStrength"},
        config.default_light.shadow.local_light_directional_shadow_lift_strength);
    const std::string shadow_execution_mode_raw = karma::config::ReadStringConfig(
        {"roamingMode.graphics.lighting.shadows.executionMode"},
        karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
            config.default_light.shadow.execution_mode));
    karma::renderer::DirectionalLightData::ShadowExecutionMode shadow_execution_mode =
        config.default_light.shadow.execution_mode;
    if (!karma::renderer::DirectionalLightData::TryParseShadowExecutionMode(
            shadow_execution_mode_raw,
            &shadow_execution_mode)) {
        spdlog::warn(
            "{}: invalid roamingMode.graphics.lighting.shadows.executionMode='{}'; using '{}'",
            options.app_name,
            shadow_execution_mode_raw,
            karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
                config.default_light.shadow.execution_mode));
    } else {
        config.default_light.shadow.execution_mode = shadow_execution_mode;
    }
    config.render_backend =
        karma::app::ResolveRenderBackendFromOption(options.backend_render, options.backend_render_explicit);
    config.physics_backend =
        karma::app::ResolvePhysicsBackendFromOption(options.backend_physics, options.backend_physics_explicit);
    config.audio_backend =
        karma::app::ResolveAudioBackendFromOption(options.backend_audio, options.backend_audio_explicit);
    config.enable_audio = karma::config::ReadBoolConfig({"audio.enabled"}, true);
    config.simulation_fixed_hz = karma::config::ReadFloatConfig({"simulation.fixedHz"}, 60.0f);
    config.simulation_max_frame_dt = karma::config::ReadFloatConfig({"simulation.maxFrameDeltaTime"}, 0.25f);
    config.simulation_max_steps =
        static_cast<int>(karma::config::ReadUInt16Config({"simulation.maxSubsteps"}, 4));
    config.ui_backend_override =
        karma::app::ResolveUiBackendOverrideFromOption(options.backend_ui, options.backend_ui_explicit);

    const bz3::GameStartupOptions startup = ResolveGameStartupOptions(options);

    karma::app::EngineApp app;
    bz3::Game game{startup};
    app.start(game, config);
    while (app.isRunning()) {
        app.tick();
    }
    return 0;
}

} // namespace bz3::client

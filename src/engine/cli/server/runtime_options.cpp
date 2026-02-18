#include "karma/cli/server/runtime_options.hpp"

#include "karma/common/config/helpers.hpp"
#include "karma/common/config/store.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/common/serialization/json.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace karma::cli::server {
namespace {

std::filesystem::path TryCanonical(const std::filesystem::path& path) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return result;
    }
    result = std::filesystem::absolute(path, ec);
    if (!ec) {
        return result;
    }
    return path;
}

} // namespace

shared::ConsumeResult ConsumeRuntimeOption(const std::string& arg,
                                           int& index,
                                           int argc,
                                           char** argv,
                                           std::string& server_config_path_out,
                                           bool& server_config_explicit_out,
                                           uint16_t& listen_port_out,
                                           bool& listen_port_explicit_out,
                                           std::string& community_out,
                                           bool& community_explicit_out) {
    shared::ConsumeResult out{};

    if (arg == "--server-config") {
        std::string error{};
        auto value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        server_config_path_out = *value;
        server_config_explicit_out = true;
        return out;
    }
    if (shared::StartsWith(arg, "--server-config=")) {
        server_config_path_out = shared::ValueAfterEquals(arg, "--server-config=");
        server_config_explicit_out = true;
        out.consumed = true;
        return out;
    }

    if (arg == "--listen-port") {
        std::string error{};
        auto value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        const auto parsed = shared::ParseUInt16Port(*value, &error);
        if (!parsed) {
            out.error = error;
            return out;
        }
        listen_port_out = *parsed;
        listen_port_explicit_out = true;
        return out;
    }
    if (shared::StartsWith(arg, "--listen-port=")) {
        const std::string raw = shared::ValueAfterEquals(arg, "--listen-port=");
        std::string error{};
        const auto parsed = shared::ParseUInt16Port(raw, &error);
        out.consumed = true;
        if (!parsed) {
            out.error = error;
            return out;
        }
        listen_port_out = *parsed;
        listen_port_explicit_out = true;
        return out;
    }

    if (arg == "--community") {
        std::string error{};
        auto value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!value) {
            out.error = error;
            return out;
        }
        community_out = *value;
        community_explicit_out = true;
        return out;
    }
    if (shared::StartsWith(arg, "--community=")) {
        community_out = shared::ValueAfterEquals(arg, "--community=");
        community_explicit_out = true;
        out.consumed = true;
        return out;
    }

    return out;
}

void AppendRuntimeHelp(std::ostream& out) {
    out << "      --server-config <path>      Server config overlay file\n"
        << "      --listen-port <port>        Server listen port\n"
        << "      --community <url>           Community endpoint (http://host:port or host:port)\n";
}

std::optional<std::filesystem::path> ResolveConfigOverlayPath(const std::string& server_config_path,
                                                              bool server_config_explicit) {
    if (!server_config_explicit) {
        return std::nullopt;
    }
    std::filesystem::path path(server_config_path);
    if (!path.is_absolute()) {
        path = std::filesystem::absolute(path);
    }
    return TryCanonical(path);
}

std::optional<std::filesystem::path> ApplyConfigOverlay(const std::string& server_config_path,
                                                        bool server_config_explicit) {
    const auto overlay_path = ResolveConfigOverlayPath(server_config_path, server_config_explicit);
    if (!overlay_path.has_value()) {
        return std::nullopt;
    }

    const auto overlay_json_opt =
        common::data::LoadJsonFile(*overlay_path, "server config overlay", spdlog::level::err);
    if (!overlay_json_opt || !overlay_json_opt->is_object()) {
        throw std::runtime_error("Failed to load server config overlay object from "
                                 + overlay_path->string());
    }

    const std::filesystem::path overlay_base_dir = overlay_path->parent_path();
    if (!common::config::ConfigStore::AddRuntimeLayer("server config overlay", *overlay_json_opt, overlay_base_dir)) {
        throw std::runtime_error("Failed to add server config overlay runtime layer.");
    }

    const auto* overlay_layer = common::config::ConfigStore::LayerByLabel("server config overlay");
    if (!overlay_layer || !overlay_layer->is_object()) {
        throw std::runtime_error("Runtime layer lookup failed for server config overlay.");
    }

    KARMA_TRACE("engine.server",
                "Applied server config overlay '{}' with base '{}'",
                overlay_path->string(),
                overlay_base_dir.string());
    return overlay_path;
}

uint16_t ResolveListenPort(uint16_t listen_port,
                           bool listen_port_explicit,
                           uint16_t fallback_port) {
    if (listen_port_explicit) {
        return listen_port;
    }
    return common::config::ReadUInt16Config({"network.ServerPort"}, fallback_port);
}

} // namespace karma::cli::server

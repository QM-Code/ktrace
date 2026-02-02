#include "spdlog/spdlog.h"
#include "karma/app/engine_app.hpp"
#include "game/engine/server_engine.hpp"
#include "server/game.hpp"
#include "plugin.hpp"
#include "server/server_discovery.hpp"
#include "server/terminal_commands.hpp"
#include "server/server_cli_options.hpp"
#include "server/community_heartbeat.hpp"
#include "game/common/data_path_spec.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/json.hpp"
#include <pybind11/embed.h>
#include <csignal>
#include <atomic>
#include <iostream>
#include <limits>
#include <poll.h>
#include <unistd.h>
#include <filesystem>
#include <vector>

#define MIN_FRAME_HZ (1.0f / 120.0f)

spdlog::level::level_enum ParseLogLevel(const std::string &level) {
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "info") {
        return spdlog::level::info;
    }
    if (level == "warn") {
        return spdlog::level::warn;
    }
    if (level == "err") {
        return spdlog::level::err;
    }
    if (level == "critical") {
        return spdlog::level::critical;
    }
    if (level == "off") {
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

void ConfigureLogging(spdlog::level::level_enum level, bool includeTimestamp) {
    if (includeTimestamp) {
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    } else {
        spdlog::set_pattern("[%^%l%$] %v");
    }
    spdlog::set_level(level);
}

std::atomic<bool> g_running{true};
namespace py = pybind11;
Game* g_game = nullptr;
ServerEngine* g_engine = nullptr;

/**
 * Signal handler for graceful shutdown.
 *
 * @param signum The signal number.
 * 
 */
void signalHandler(int signum) {
    spdlog::info("Interrupt signal ({}) received. Shutting down...", signum);
    g_running = false;
}

class ServerLoopAdapter final : public karma::app::GameInterface {
public:
    ServerLoopAdapter(ServerEngine &engine,
                      Game &game,
                      CommunityHeartbeat &heartbeat)
        : engine_(engine),
          game_(game),
          heartbeat_(heartbeat) {}

    void onUpdate(float dt) override {
        if (dt < MIN_FRAME_HZ) {
            TimeUtils::sleep(MIN_FRAME_HZ - dt);
            return;
        }

        if (poll(&pfd_, 1, 0) > 0 && (pfd_.revents & POLLIN)) {
            std::string line;
            if (std::getline(std::cin, line)) {
                if (!line.empty()) {
                    std::string response = processTerminalInput(line);
                    if (!response.empty()) {
                        std::cout << response << std::endl;
                    }
                }
                std::cout << "> " << std::flush;
            }
        }

        engine_.earlyUpdate(dt);
        game_.update(dt);
        engine_.lateUpdate(dt);
        heartbeat_.update(game_);
    }

    bool shouldQuit() const override { return !g_running.load(); }

private:
    ServerEngine &engine_;
    Game &game_;
    CommunityHeartbeat &heartbeat_;
    struct pollfd pfd_ = { STDIN_FILENO, POLLIN, 0 };
};

int main(int argc, char *argv[]) {
    ConfigureLogging(spdlog::level::info, false);

    game_common::ConfigureDataPathSpec();

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const auto dataDirResult = karma::data::ApplyDataDirOverrideFromArgs(argc, argv, std::filesystem::path("server/config.json"));

    const std::vector<karma::config::ConfigFileSpec> baseConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true, true}
    };
    karma::config::ConfigStore::Initialize(baseConfigSpecs, dataDirResult.userConfigPath);

    ServerCLIOptions cliOptions;
    try {
        cliOptions = ParseServerCLIOptions(argc, argv);
    } catch (const std::exception &ex) {
        spdlog::error("Failed to parse server command line options: {}", ex.what());
        return 1;
    }
    const auto configIssues = karma::config::ValidateRequiredKeys(karma::config::ServerRequiredKeys());
    if (!configIssues.empty()) {
        if (cliOptions.strictConfig) {
            spdlog::error("Config validation failed:");
        } else {
            spdlog::warn("Config validation reported issues:");
        }
        for (const auto &issue : configIssues) {
            if (cliOptions.strictConfig) {
                spdlog::error("  {}: {}", issue.path, issue.message);
            } else {
                spdlog::warn("  {}: {}", issue.path, issue.message);
            }
        }
        if (cliOptions.strictConfig) {
            return 1;
        }
    }

    const spdlog::level::level_enum logLevel = cliOptions.logLevelExplicit
        ? ParseLogLevel(cliOptions.logLevel)
        : (cliOptions.verbose >= 2 ? spdlog::level::trace
           : cliOptions.verbose == 1 ? spdlog::level::debug
           : spdlog::level::info);
    ConfigureLogging(logLevel, cliOptions.timestampLogging);

    if (!cliOptions.worldSpecified) {
        spdlog::error("No world directory specified. Use -w <directory> or -D to load the bundled default world.");
        return 1;
    }

    std::filesystem::path worldDirPath = karma::data::Resolve(cliOptions.worldDir);

    if (!std::filesystem::is_directory(worldDirPath)) {
        spdlog::error("World directory not found: {}", worldDirPath.string());
        return 1;
    }
    const std::filesystem::path configPath = worldDirPath / "config.json";

    if (auto worldConfigOpt = karma::data::LoadJsonFile(configPath, "world config", spdlog::level::err)) {
        if (worldConfigOpt->is_object()) {
            karma::config::ConfigStore::AddRuntimeLayer("world config", *worldConfigOpt, configPath.parent_path());
        }
    }

    const auto *worldConfigPtr = karma::config::ConfigStore::LayerByLabel("world config");
    if (!worldConfigPtr || !worldConfigPtr->is_object()) {
        spdlog::error("main: Failed to load world config object from {}", configPath.string());
        return 1;
    }

    const auto &mergedConfig = karma::config::ConfigStore::Merged();
    if (!mergedConfig.is_object()) {
        spdlog::error("main: Merged configuration is not a JSON object");
        return 1;
    }

    uint16_t port = cliOptions.hostPort;
    if (!cliOptions.hostPortExplicit) {
        port = karma::config::ReadUInt16Config({"network.ServerPort"}, port);
    } else {
        port = cliOptions.hostPort;
    }

    std::string serverName = karma::config::ReadStringConfig("serverName", "BZ Server");
    std::string worldName = karma::config::ReadStringConfig("worldName", worldDirPath.filename().string());
    ServerEngine engine(port);
    spdlog::trace("ServerEngine initialized successfully");

    const bool shouldZipWorld = cliOptions.customWorldProvided;

    Game game(engine, serverName, worldName, *worldConfigPtr, worldDirPath.string(), shouldZipWorld);
    spdlog::trace("Game initialized successfully");
    g_engine = &engine;
    g_game = &game;

    ServerDiscoveryBeacon discoveryBeacon(port, serverName, worldName);

    CommunityHeartbeat communityHeartbeat;
    const std::string communityOverride = cliOptions.communityExplicit ? cliOptions.community : std::string();
    communityHeartbeat.configureFromConfig(mergedConfig, port, communityOverride);

    spdlog::trace("Loading plugins...");
    py::scoped_interpreter guard{};

    // Redirect Python bytecode to a writable temp (or configured) directory
    {
        namespace fs = std::filesystem;
        py::module_ sys = py::module_::import("sys");

        fs::path pycachePrefix;
        if (const char *envPrefix = std::getenv("KARMA_PY_CACHE_DIR")) {
            pycachePrefix = fs::path(envPrefix);
        } else {
            pycachePrefix = fs::temp_directory_path() / "bz3-pycache";
        }

        std::error_code ec;
        fs::create_directories(pycachePrefix, ec);
        if (!ec) {
            sys.attr("pycache_prefix") = pycachePrefix.string();
            spdlog::info("Python bytecode cache set to {}", pycachePrefix.string());
        } else {
            sys.attr("dont_write_bytecode") = true;
            spdlog::warn("Failed to create pycache dir {}; disabling bytecode write ({}).", pycachePrefix.string(), ec.message());
        }
    }
    PluginAPI::loadPythonPlugins(mergedConfig);
    spdlog::trace("Plugins loaded successfully");

    spdlog::trace("Starting main loop");
    std::cout << "> " << std::flush;

    ServerLoopAdapter adapter(engine, game, communityHeartbeat);
    karma::app::EngineApp app;
    app.context().physics = engine.physics;
    int result = 0;
    spdlog::info("EngineApp loop enabled (start/tick)");
    if (!app.start(adapter, app.config())) {
        result = 1;
    } else {
        while (app.isRunning()) {
            app.tick();
        }
    }
    spdlog::info("Server shutdown complete");
    return result;
}

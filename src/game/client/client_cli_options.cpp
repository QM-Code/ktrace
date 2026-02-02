#include "client/client_cli_options.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_store.hpp"
#include "cxxopts.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

std::string ConfiguredPortDefault() {
    if (const auto *portNode = karma::config::ConfigStore::Get("network.ServerPort")) {
        if (portNode->is_string()) {
            return portNode->get<std::string>();
        }
        if (portNode->is_number_unsigned()) {
            return std::to_string(portNode->get<unsigned int>());
        }
    }
    return std::string("0");
}

bool IsValidLogLevel(std::string level) {
    std::transform(level.begin(), level.end(), level.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return level == "trace" ||
           level == "debug" ||
           level == "info" ||
           level == "warn" ||
           level == "error" ||
           level == "err" ||
           level == "critical" ||
           level == "off";
}

std::string NormalizeLogLevel(std::string level) {
    std::transform(level.begin(), level.end(), level.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (level == "error") {
        return "err";
    }
    return level;
}

std::string NormalizeLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace

ClientCLIOptions ParseClientCLIOptions(int argc, char *argv[]) {
    cxxopts::Options options("bz3", "BZ3 client");
    options.add_options()
        ("n,name", "Player name", cxxopts::value<std::string>()->default_value("Player"));
    options.add_options()
        ("a,addr", "Connection address", cxxopts::value<std::string>()->default_value("localhost"));
    options.add_options()
        ("p,port", "Connection port", cxxopts::value<uint16_t>()->default_value(ConfiguredPortDefault()));
    options.add_options()
        ("w,world", "World directory", cxxopts::value<std::string>());
    options.add_options()
        ("d,data-dir", "Data directory (overrides KARMA_DATA_DIR)", cxxopts::value<std::string>());
    options.add_options()
        ("c,config", "User config file path", cxxopts::value<std::string>());
    options.add_options()
        ("language", "Language override (e.g., en, es, fr)", cxxopts::value<std::string>());
    options.add_options()
        ("theme", "Render theme (overrides graphics.theme)", cxxopts::value<std::string>());
    options.add_options()
        ("dev-quick-start", "Dev helper: show console, start local server, auto-connect");
    options.add_options()
        ("ui-smoke", "UI smoke test: auto-toggle HUD elements on a timer");
    options.add_options()
        ("ecs-smoke", "ECS smoke test: spawn a simple ECS world mesh + camera");
    options.add_options()
        ("test-3d", "Minimal 3D scene: no UI/network, just ECS render + camera");
    options.add_options()
        ("test-3d-world", "test-3d: render world.glb via MeshComponent instead of procedural cube");
    options.add_options()
        ("strict-config", "Fail startup if required config keys are missing", cxxopts::value<bool>()->default_value("true"));
    options.add_options()
        ("v,verbose", "Enable verbose logging (-v=debug, -vv=trace)")
        ("L,log-level", "Logging level (trace, debug, info, warn, err, critical, off)", cxxopts::value<std::string>());
    options.add_options()
        ("T,timestamp-logging", "Enable timestamped logging output");
    options.add_options()
        ("h,help", "Show help");

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        std::cerr << options.help() << std::endl;
        std::exit(1);
    }

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    ClientCLIOptions parsed;
    parsed.playerName = result["name"].as<std::string>();
    parsed.connectAddr = result["addr"].as<std::string>();
    parsed.connectPort = result["port"].as<uint16_t>();
    parsed.worldDir = result.count("world") ? result["world"].as<std::string>() : std::string();
    parsed.dataDir = result.count("data-dir") ? result["data-dir"].as<std::string>() : std::string();
    parsed.userConfigPath = result.count("config") ? result["config"].as<std::string>() : std::string();
    parsed.language = result.count("language") ? result["language"].as<std::string>() : std::string();
    parsed.theme = result.count("theme") ? result["theme"].as<std::string>() : std::string();
    parsed.addrExplicit = result.count("addr") > 0;
    parsed.worldExplicit = result.count("world") > 0;
    parsed.dataDirExplicit = result.count("data-dir") > 0;
    parsed.userConfigExplicit = result.count("config") > 0;
    parsed.languageExplicit = result.count("language") > 0;
    parsed.themeExplicit = result.count("theme") > 0;
    parsed.devQuickStart = result.count("dev-quick-start") > 0;
    parsed.uiSmokeTest = result.count("ui-smoke") > 0;
    parsed.ecsSmokeTest = result.count("ecs-smoke") > 0;
    parsed.test3d = result.count("test-3d") > 0;
    parsed.test3dWorld = result.count("test-3d-world") > 0;
    parsed.strictConfig = result["strict-config"].as<bool>();
    parsed.verbose = static_cast<int>(result.count("verbose"));
    parsed.logLevel = result.count("log-level") ? result["log-level"].as<std::string>() : std::string();
    parsed.logLevelExplicit = result.count("log-level") > 0;
    parsed.timestampLogging = result.count("timestamp-logging") > 0;
    if (parsed.logLevelExplicit && !IsValidLogLevel(parsed.logLevel)) {
        std::cerr << "Error: invalid --log-level value '" << parsed.logLevel << "'.\n";
        std::cerr << options.help() << std::endl;
        std::exit(1);
    }
    if (parsed.logLevelExplicit) {
        parsed.logLevel = NormalizeLogLevel(parsed.logLevel);
    }
    return parsed;
}

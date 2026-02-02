#include "server/server_cli_options.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_store.hpp"
#include "cxxopts.hpp"
#include "karma/common/json.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <algorithm>

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

} // namespace

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

ServerCLIOptions ParseServerCLIOptions(int argc, char *argv[]) {
    cxxopts::Options options("bz3-server", "BZ3 server");
    options.add_options()
        ("w,world", "World directory", cxxopts::value<std::string>())
        ("D,default-world", "Use bundled default world")
        ("p,port", "Server listen port", cxxopts::value<uint16_t>()->default_value(ConfiguredPortDefault()))
        ("d,data-dir", "Data directory (overrides KARMA_DATA_DIR)", cxxopts::value<std::string>())
        ("c,config", "User config file path", cxxopts::value<std::string>())
        ("C,community", "Community server (http://host:port or host:port)", cxxopts::value<std::string>())
        ("strict-config", "Fail startup if required config keys are missing", cxxopts::value<bool>()->default_value("true"))
        ("v,verbose", "Enable verbose logging (-v=debug, -vv=trace)")
        ("L,log-level", "Logging level (trace, debug, info, warn, err, critical, off)", cxxopts::value<std::string>())
        ("T,timestamp-logging", "Enable timestamped logging output")
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

    ServerCLIOptions parsed;
    if (result.count("world") && result.count("default-world")) {
        throw std::runtime_error("Cannot specify both -w/--world and -D/--default-world");
    }

    if (result.count("default-world")) {
        parsed.worldSpecified = true;

        const auto serverConfigPath = karma::data::Resolve("server/config.json");
        auto serverConfigOpt = karma::data::LoadJsonFile(serverConfigPath, "data/server/config.json", spdlog::level::err);
        if (!serverConfigOpt || !serverConfigOpt->is_object()) {
            throw std::runtime_error("default world flag requires data/server/config.json to be a JSON object");
        }

        auto it = serverConfigOpt->find("defaultWorld");
        if (it == serverConfigOpt->end() || !it->is_string()) {
            throw std::runtime_error("defaultWorld missing or not a string in data/server/config.json");
        }

        parsed.worldDir = it->get<std::string>();
        parsed.customWorldProvided = false;
    }

    if (result.count("world")) {
        parsed.worldDir = result["world"].as<std::string>();
        parsed.worldSpecified = true;
        parsed.customWorldProvided = true;
    }

    parsed.dataDir = result.count("data-dir") ? result["data-dir"].as<std::string>() : std::string();
    parsed.userConfigPath = result.count("config") ? result["config"].as<std::string>() : std::string();
    parsed.hostPort = result["port"].as<uint16_t>();
    parsed.hostPortExplicit = result.count("port") > 0;
    parsed.dataDirExplicit = result.count("data-dir") > 0;
    parsed.userConfigExplicit = result.count("config") > 0;
    parsed.verbose = static_cast<int>(result.count("verbose"));
    parsed.logLevel = result.count("log-level") ? result["log-level"].as<std::string>() : std::string();
    parsed.logLevelExplicit = result.count("log-level") > 0;
    parsed.timestampLogging = result.count("timestamp-logging") > 0;
    parsed.community = result.count("community") ? result["community"].as<std::string>() : std::string();
    parsed.communityExplicit = result.count("community") > 0;
    parsed.strictConfig = result["strict-config"].as<bool>();
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

#pragma once

#include <cstdint>
#include <string>

struct ServerCLIOptions {
    std::string worldDir;
    bool worldSpecified = false;
    bool customWorldProvided = false;
    uint16_t hostPort;
    bool hostPortExplicit = false;
    std::string dataDir;
    std::string userConfigPath;
    bool dataDirExplicit = false;
    bool userConfigExplicit = false;
    int verbose = 0;
    std::string logLevel;
    bool logLevelExplicit = false;
    bool timestampLogging = false;
    std::string community;
    bool communityExplicit = false;
    bool strictConfig = true;
};

ServerCLIOptions ParseServerCLIOptions(int argc, char *argv[]);

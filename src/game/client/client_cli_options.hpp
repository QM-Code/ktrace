#pragma once

#include <cstdint>
#include <string>

struct ClientCLIOptions {
    std::string playerName;
    std::string connectAddr;
    uint16_t connectPort;
    std::string worldDir;
    std::string dataDir;
    std::string userConfigPath;
    std::string language;
    std::string theme;
    bool addrExplicit = false;
    bool worldExplicit = false;
    bool dataDirExplicit = false;
    bool userConfigExplicit = false;
    bool languageExplicit = false;
    bool themeExplicit = false;
    bool devQuickStart = false;
    bool strictConfig = true;
    bool uiSmokeTest = false;
    bool ecsSmokeTest = false;
    bool test3d = false;
    bool test3dWorld = false;
    int verbose = 0;
    std::string logLevel;
    bool logLevelExplicit = false;
    bool timestampLogging = false;
};

ClientCLIOptions ParseClientCLIOptions(int argc, char *argv[]);

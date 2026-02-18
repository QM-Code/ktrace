#include "common/data/directory_override.hpp"

#include "common/data/path_resolver.hpp"
#include "common/serialization/json.hpp"
#include "karma/common/logging/logging.hpp"
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <stdexcept>

namespace {

std::optional<std::filesystem::path> ParsePathArg(int argc, char *argv[], const std::string &shortOpt, const std::string &longOpt) {
    const std::string longPrefix = longOpt + "=";

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if ((!shortOpt.empty() && arg == shortOpt) || arg == longOpt) {
            if (i + 1 < argc) {
                return std::filesystem::path(argv[i + 1]);
            }
            break;
        }

        if (arg.rfind(longPrefix, 0) == 0) {
            return std::filesystem::path(arg.substr(longPrefix.size()));
        }
    }

    return std::nullopt;
}

std::filesystem::path CanonicalizePath(const std::filesystem::path &path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    canonical = std::filesystem::absolute(path, ec);
    return ec ? path : canonical;
}

std::filesystem::path EnsureConfigFileAtPath(const std::filesystem::path &path, const std::filesystem::path &defaultRelative) {
    if (path.empty()) {
        // Build a path under the user config directory respecting relative subfolders (e.g., server/config.json).
        const auto baseDir = karma::common::data::UserConfigDirectory();
        const auto target = baseDir / defaultRelative;

        const auto parent = target.parent_path();
        if (!parent.empty()) {
            std::error_code dirEc;
            std::filesystem::create_directories(parent, dirEc);
            if (dirEc) {
                throw std::runtime_error("Failed to create config directory " + parent.string() + ": " + dirEc.message());
            }
        }

        if (!std::filesystem::exists(target)) {
            std::ofstream stream(target);
            if (!stream) {
                throw std::runtime_error("Failed to create user config file " + target.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize user config file " + target.string());
            }
        } else if (std::filesystem::is_regular_file(target)) {
            std::error_code sizeEc;
            const auto fileSize = std::filesystem::file_size(target, sizeEc);
            if (!sizeEc && fileSize == 0) {
                std::ofstream stream(target, std::ios::trunc);
                if (!stream) {
                    throw std::runtime_error("Failed to truncate empty user config file " + target.string());
                }

                stream << "{}\n";
                if (!stream) {
                    throw std::runtime_error("Failed to initialize truncated user config file " + target.string());
                }
            }
        }

        return CanonicalizePath(target);
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code dirEc;
        std::filesystem::create_directories(parent, dirEc);
        if (dirEc) {
            throw std::runtime_error("Failed to create config directory " + parent.string() + ": " + dirEc.message());
        }
    }

    if (!std::filesystem::exists(path)) {
        std::ofstream stream(path);
        if (!stream) {
            throw std::runtime_error("Failed to create user config file " + path.string());
        }

        stream << "{}\n";
        if (!stream) {
            throw std::runtime_error("Failed to initialize user config file " + path.string());
        }
    } else if (std::filesystem::is_regular_file(path)) {
        std::error_code sizeEc;
        const auto fileSize = std::filesystem::file_size(path, sizeEc);
        if (!sizeEc && fileSize == 0) {
            std::ofstream stream(path, std::ios::trunc);
            if (!stream) {
                throw std::runtime_error("Failed to truncate empty user config file " + path.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize truncated user config file " + path.string());
            }
        }
    }

    return CanonicalizePath(path);
}

std::optional<std::filesystem::path> ExtractDataDirFromConfig(const std::filesystem::path &configPath) {
    std::ifstream stream(configPath);
    if (!stream) {
        // If the file cannot be opened, fall back to other mechanisms.
        return std::nullopt;
    }

    try {
        karma::common::serialization::Value configJson;
        stream >> configJson;

        if (!configJson.is_object()) {
            return std::nullopt;
        }

        if (auto dataDirIt = configJson.find("DataDir"); dataDirIt != configJson.end() && dataDirIt->is_string()) {
            const auto value = dataDirIt->get<std::string>();
            if (!value.empty()) {
                std::filesystem::path dataDirPath(value);
                if (dataDirPath.is_relative()) {
                    dataDirPath = configPath.parent_path() / dataDirPath;
                }
                return CanonicalizePath(dataDirPath);
            }
        }
    } catch (const std::exception &ex) {
        throw std::runtime_error("Failed to parse user config at " + configPath.string() + ": " + ex.what());
    }

    return std::nullopt;
}

bool IsValidDir(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

void ValidateDataDirOrExit(const std::filesystem::path &path, const std::string &sourceLabel, const std::optional<std::filesystem::path> &configPath = std::nullopt) {
    if (!IsValidDir(path)) {
        std::cerr << "Invalid data directory specified: \"" << sourceLabel << "\"\n";
        std::cerr << "" << path.string() << " does not exist or is not a directory.\n";
        if (configPath) {
            std::cerr << "User config path: " << configPath->string() << "\n";
        }
        std::exit(1);
    }

    std::error_code ec;
    const auto spec = karma::common::data::GetDataPathSpec();
    if (!spec.requiredDataMarker.empty()) {
        const auto markerPath = path / spec.requiredDataMarker;
        if (!std::filesystem::exists(markerPath, ec) || !std::filesystem::is_regular_file(markerPath, ec)) {
            std::cerr << "Invalid data directory specified: \"" << sourceLabel << "\"\n";
            std::cerr << markerPath.string() << " does not exist." << std::endl;
            if (configPath) {
                std::cerr << "User config path: " << configPath->string() << std::endl;
            }
            std::exit(1);
        }
    }
}

} // namespace

namespace karma::common::data {

DataDirectoryOverrideResult ApplyDataDirectoryOverrideFromArgs(
    int argc,
    char* argv[],
    const std::filesystem::path& defaultConfigRelative,
    bool enableUserConfig,
    bool allowDataDirFromUserConfigWhenUserConfigDisabled) {
    try {
        const auto cliConfigPath = ParsePathArg(argc, argv, "", "--user-config");
        const auto cliDataDir = ParsePathArg(argc, argv, "", "--data-dir");
        std::filesystem::path configPath{};
        std::optional<std::filesystem::path> configDataDir{};

        if (enableUserConfig) {
            configPath = EnsureConfigFileAtPath(cliConfigPath ? *cliConfigPath : std::filesystem::path{},
                                                defaultConfigRelative);
            const auto configRoot = configPath.parent_path();
            if (!configRoot.empty()) {
                karma::common::data::SetUserConfigRootOverride(configRoot);
            }
            configDataDir = cliDataDir ? std::optional<std::filesystem::path>{}
                                       : ExtractDataDirFromConfig(configPath);
        } else if (cliConfigPath) {
            std::cerr << "The --user-config option is not supported for this executable.\n";
            std::exit(1);
        } else if (allowDataDirFromUserConfigWhenUserConfigDisabled) {
            const auto readOnlyConfigPath = CanonicalizePath(karma::common::data::UserConfigDirectory() / defaultConfigRelative);
            configPath = readOnlyConfigPath;
            configDataDir = cliDataDir ? std::optional<std::filesystem::path>{}
                                       : ExtractDataDirFromConfig(readOnlyConfigPath);
        }

        if (cliDataDir) {
            ValidateDataDirOrExit(*cliDataDir, std::string("--data-dir ") + cliDataDir->string());
            karma::common::data::SetDataRootOverride(*cliDataDir);
            KARMA_TRACE("config", "Using data directory from CLI override: {}", cliDataDir->string());
            return {configPath, *cliDataDir};
        }

        if (configDataDir) {
            ValidateDataDirOrExit(*configDataDir, std::string("user config"), configPath);
            karma::common::data::SetDataRootOverride(*configDataDir);
            KARMA_TRACE("config", "Using data directory from user config: {}", configDataDir->string());
            return {configPath, *configDataDir};
        }

        const auto spec = karma::common::data::GetDataPathSpec();

        // Fall back to env var if present; otherwise fail with a friendly message.
        if (const char *envDataDir = std::getenv(spec.dataDirEnvVar.c_str()); envDataDir && *envDataDir) {
            const std::filesystem::path envPath(envDataDir);
            ValidateDataDirOrExit(envPath, std::string(spec.dataDirEnvVar) + ": " + envDataDir);
            karma::common::data::SetDataRootOverride(envPath);
            KARMA_TRACE("config", "Using data directory from {}: {}", spec.dataDirEnvVar, envPath.string());
            return {configPath, envPath};
        }

        std::cerr << "\n";
        std::cerr << "The data directory could not be found.\n";
        std::cerr << "\n";
        std::cerr << "This should not happen and may indicate a problem with installation.\n\n";
        std::cerr << "This directory can be specified";
        if (enableUserConfig || allowDataDirFromUserConfigWhenUserConfigDisabled) {
            std::cerr << " in three ways:\n";
        } else {
            std::cerr << " in two ways:\n";
        }
        std::cerr << "  1. Set the " << spec.dataDirEnvVar << " environment variable.\n";
        std::cerr << "  2. Use the command-line option \"--data-dir <datadir>\".\n";
        if (enableUserConfig || allowDataDirFromUserConfigWhenUserConfigDisabled) {
            std::cerr << "  3. Add the following to your user config file:\n";
            std::cerr << "     " << configPath.string() << "\n";
            std::cerr << "     {\n";
            std::cerr << "         \"DataDir\" : \"<datadir>\"\n";
            std::cerr << "     }\n";
        }
        std::cerr << "\n";
        std::exit(1);
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        std::exit(1);
    }
}

} // namespace karma::common::data

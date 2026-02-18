#include "common/config/helpers.hpp"

#include "common/config/store.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace karma::common::config {

bool ReadBoolConfig(std::initializer_list<const char*> paths, bool defaultValue) {
    for (const char* path : paths) {
        if (const auto* value = ConfigStore::Get(path)) {
            if (value->is_boolean()) {
                return value->get<bool>();
            }
            if (value->is_number_integer()) {
                return value->get<long long>() != 0;
            }
            if (value->is_number_float()) {
                return value->get<double>() != 0.0;
            }
            if (value->is_string()) {
                std::string text = value->get<std::string>();
                std::transform(text.begin(), text.end(), text.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (text == "true" || text == "1" || text == "yes" || text == "on") {
                    return true;
                }
                if (text == "false" || text == "0" || text == "no" || text == "off") {
                    return false;
                }
            }
            spdlog::warn("Config '{}' cannot be interpreted as boolean", path);
        }
    }
    return defaultValue;
}

uint16_t ReadUInt16Config(std::initializer_list<const char*> paths, uint16_t defaultValue) {
    auto clampToUint16 = [](long long number) -> std::optional<uint16_t> {
        if (number < 0 || number > std::numeric_limits<uint16_t>::max()) {
            return std::nullopt;
        }
        return static_cast<uint16_t>(number);
    };
    for (const char* path : paths) {
        if (const auto* value = ConfigStore::Get(path)) {
            if (value->is_number_unsigned()) {
                if (auto clamped = clampToUint16(static_cast<long long>(value->get<unsigned long long>()))) {
                    if (*clamped > 0) {
                        return *clamped;
                    }
                    spdlog::warn("Config '{}' must be positive; falling back", path);
                    return defaultValue;
                }
            }
            if (value->is_number_integer()) {
                if (auto clamped = clampToUint16(static_cast<long long>(value->get<long long>()))) {
                    if (*clamped > 0) {
                        return *clamped;
                    }
                    spdlog::warn("Config '{}' must be positive; falling back", path);
                    return defaultValue;
                }
            }
        }

        if (const auto* raw = ConfigStore::Get(path); raw && raw->is_string()) {
            try {
                const auto parsed = std::stoul(raw->get<std::string>());
                if (parsed > 0 && parsed <= std::numeric_limits<uint16_t>::max()) {
                    return static_cast<uint16_t>(parsed);
                }
            } catch (...) {
                spdlog::warn("Config '{}' string value is not a valid uint16", path);
            }
            return defaultValue;
        }
    }
    return defaultValue;
}

float ReadFloatConfig(std::initializer_list<const char*> paths, float defaultValue) {
    for (const char* path : paths) {
        if (const auto* value = ConfigStore::Get(path)) {
            if (value->is_number_float()) {
                return static_cast<float>(value->get<double>());
            }
            if (value->is_number_integer()) {
                return static_cast<float>(value->get<long long>());
            }
            if (value->is_string()) {
                try {
                    return std::stof(value->get<std::string>());
                } catch (...) {
                    spdlog::warn("Config '{}' string value is not a valid float", path);
                }
            } else {
                spdlog::warn("Config '{}' cannot be interpreted as float", path);
            }
        }
    }
    return defaultValue;
}

std::string ReadStringConfig(const char *path, const std::string &defaultValue) {
    if (const auto* value = ConfigStore::Get(path)) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    return defaultValue;
}

bool ReadRequiredBoolConfig(const char *path) {
    if (const auto* value = ConfigStore::Get(path)) {
        if (value->is_boolean()) {
            return value->get<bool>();
        }
        if (value->is_number_integer()) {
            return value->get<long long>() != 0;
        }
        if (value->is_number_float()) {
            return value->get<double>() != 0.0;
        }
        if (value->is_string()) {
            std::string text = value->get<std::string>();
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (text == "true" || text == "1" || text == "yes" || text == "on") {
                return true;
            }
            if (text == "false" || text == "0" || text == "no" || text == "off") {
                return false;
            }
        }
    }
    throw std::runtime_error(std::string("Missing required boolean config: ") + path);
}

uint16_t ReadRequiredUInt16Config(const char *path) {
    if (const auto* value = ConfigStore::Get(path)) {
        if (value->is_number_integer()) {
            const auto raw = value->get<long long>();
            if (raw >= 0 && raw <= std::numeric_limits<uint16_t>::max()) {
                return static_cast<uint16_t>(raw);
            }
        }
        if (value->is_number_float()) {
            const auto raw = value->get<double>();
            if (raw >= 0.0 && raw <= static_cast<double>(std::numeric_limits<uint16_t>::max())) {
                return static_cast<uint16_t>(raw);
            }
        }
    }
    throw std::runtime_error(std::string("Missing required uint16 config: ") + path);
}

float ReadRequiredFloatConfig(const char *path) {
    if (const auto* value = ConfigStore::Get(path)) {
        if (value->is_number_float()) {
            return static_cast<float>(value->get<double>());
        }
        if (value->is_number_integer()) {
            return static_cast<float>(value->get<long long>());
        }
    }
    throw std::runtime_error(std::string("Missing required float config: ") + path);
}

std::string ReadRequiredStringConfig(const char *path) {
    if (const auto* value = ConfigStore::Get(path)) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    throw std::runtime_error(std::string("Missing required string config: ") + path);
}

std::vector<float> ReadRequiredFloatArrayConfig(std::string_view path) {
    const auto* value = ConfigStore::Get(path);
    if (!value) {
        throw std::runtime_error(std::string("Missing required float array config: ") + std::string(path));
    }
    if (!value->is_array()) {
        throw std::runtime_error(std::string("Config '") + std::string(path) + "' must be an array");
    }
    std::vector<float> out;
    out.reserve(value->size());
    for (const auto& entry : *value) {
        if (entry.is_number_float()) {
            out.push_back(static_cast<float>(entry.get<double>()));
        } else if (entry.is_number_integer()) {
            out.push_back(static_cast<float>(entry.get<long long>()));
        } else {
            throw std::runtime_error(std::string("Config '") + std::string(path) + "' contains a non-numeric value");
        }
    }
    return out;
}

std::vector<std::string> ReadRequiredStringArrayConfig(std::string_view path) {
    const auto* value = ConfigStore::Get(path);
    if (!value) {
        throw std::runtime_error(std::string("Missing required string array config: ") + std::string(path));
    }
    if (!value->is_array()) {
        throw std::runtime_error(std::string("Config '") + std::string(path) + "' must be an array");
    }
    std::vector<std::string> out;
    out.reserve(value->size());
    for (const auto& entry : *value) {
        if (!entry.is_string()) {
            throw std::runtime_error(std::string("Config '") + std::string(path) + "' contains a non-string value");
        }
        out.push_back(entry.get<std::string>());
    }
    return out;
}

const karma::common::serialization::Value& ReadRequiredObjectConfig(std::string_view path) {
    const auto* value = ConfigStore::Get(path);
    if (!value) {
        throw std::runtime_error(std::string("Missing required object config: ") + std::string(path));
    }
    if (!value->is_object()) {
        throw std::runtime_error(std::string("Config '") + std::string(path) + "' must be an object");
    }
    return *value;
}

} // namespace karma::common::config

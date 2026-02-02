#include "common/config_validation.hpp"

#include "common/config_store.hpp"
#include "common/json.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace {

using karma::config::RequiredKey;
using karma::config::RequiredType;

bool IsBoolLike(const karma::json::Value& value) {
    if (value.is_boolean() || value.is_number_integer() || value.is_number_float()) {
        return true;
    }
    if (!value.is_string()) {
        return false;
    }
    std::string text = value.get<std::string>();
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text == "true" || text == "false" || text == "1" || text == "0"
        || text == "yes" || text == "no" || text == "on" || text == "off";
}

bool IsUInt16Like(const karma::json::Value& value) {
    if (value.is_number_integer()) {
        const auto raw = value.get<long long>();
        return raw >= 0 && raw <= std::numeric_limits<uint16_t>::max();
    }
    if (value.is_number_float()) {
        const auto raw = value.get<double>();
        return raw >= 0.0 && raw <= static_cast<double>(std::numeric_limits<uint16_t>::max());
    }
    return false;
}

bool IsFloatLike(const karma::json::Value& value) {
    return value.is_number_float() || value.is_number_integer();
}

bool IsStringLike(const karma::json::Value& value) {
    return value.is_string();
}

bool IsValidType(const karma::json::Value& value, RequiredType type) {
    switch (type) {
    case RequiredType::Bool:
        return IsBoolLike(value);
    case RequiredType::UInt16:
        return IsUInt16Like(value);
    case RequiredType::Float:
        return IsFloatLike(value);
    case RequiredType::String:
        return IsStringLike(value);
    }
    return false;
}

const char* TypeLabel(RequiredType type) {
    switch (type) {
    case RequiredType::Bool:
        return "bool";
    case RequiredType::UInt16:
        return "uint16";
    case RequiredType::Float:
        return "float";
    case RequiredType::String:
        return "string";
    }
    return "unknown";
}

void AppendKeys(std::vector<RequiredKey>& out, std::initializer_list<RequiredKey> keys) {
    out.insert(out.end(), keys.begin(), keys.end());
}

std::vector<RequiredKey> CommonKeys() {
    std::vector<RequiredKey> keys;
    keys.reserve(10);
    AppendKeys(keys, {
        {"language", RequiredType::String},
        {"platform.WindowWidth", RequiredType::UInt16},
        {"platform.WindowHeight", RequiredType::UInt16},
        {"platform.WindowTitle", RequiredType::String},
        {"graphics.theme", RequiredType::String},
        {"graphics.skybox.Mode", RequiredType::String},
        {"graphics.skybox.Cubemap.Name", RequiredType::String},
        {"graphics.Camera.FovDegrees", RequiredType::Float},
        {"graphics.Camera.NearPlane", RequiredType::Float},
        {"graphics.Camera.FarPlane", RequiredType::Float}
    });
    return keys;
}

std::vector<RequiredKey> ClientOnlyKeys() {
    std::vector<RequiredKey> keys;
    keys.reserve(10);
    AppendKeys(keys, {
        {"game.roamingCamera.MoveSpeed", RequiredType::Float},
        {"game.roamingCamera.FastMultiplier", RequiredType::Float},
        {"game.roamingCamera.LookSensitivity", RequiredType::Float},
        {"game.roamingCamera.InvertY", RequiredType::Bool},
        {"game.roamingCamera.StartYawOffsetDeg", RequiredType::Float},
        {"assets.hud.fonts.console.Regular.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Title.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Heading.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Button.Size", RequiredType::Float}
    });
    return keys;
}

} // namespace

namespace karma::config {

std::vector<ValidationIssue> ValidateRequiredKeys(const std::vector<RequiredKey>& keys) {
    std::vector<ValidationIssue> issues;
    for (const auto& entry : keys) {
        const auto* value = ConfigStore::Get(entry.path);
        if (!value) {
            issues.push_back({entry.path, "missing required config"});
            continue;
        }
        if (!IsValidType(*value, entry.type)) {
            issues.push_back({entry.path, std::string("invalid type (expected ") + TypeLabel(entry.type) + ")"});
        }
    }
    return issues;
}

std::vector<RequiredKey> ClientRequiredKeys() {
    auto keys = CommonKeys();
    auto clientKeys = ClientOnlyKeys();
    keys.insert(keys.end(), clientKeys.begin(), clientKeys.end());
    return keys;
}

std::vector<RequiredKey> ServerRequiredKeys() {
    return CommonKeys();
}

} // namespace karma::config

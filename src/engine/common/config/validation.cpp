#include "common/config/validation.hpp"

#include "common/config/store.hpp"
#include "common/serialization/json.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace {

using karma::common::config::RequiredKey;
using karma::common::config::RequiredType;

bool IsBoolLike(const karma::common::serialization::Value& value) {
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

bool IsUInt16Like(const karma::common::serialization::Value& value) {
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

bool IsFloatLike(const karma::common::serialization::Value& value) {
    return value.is_number_float() || value.is_number_integer();
}

bool IsStringLike(const karma::common::serialization::Value& value) {
    return value.is_string();
}

bool IsValidType(const karma::common::serialization::Value& value, RequiredType type) {
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

std::vector<RequiredKey> ClientKeys() {
    std::vector<RequiredKey> keys;
    keys.reserve(21);
    AppendKeys(keys, {
        {"language", RequiredType::String},
        {"window.WindowWidth", RequiredType::UInt16},
        {"window.WindowHeight", RequiredType::UInt16},
        {"window.WindowTitle", RequiredType::String},
        {"roamingMode.graphics.theme", RequiredType::String},
        {"roamingMode.graphics.skybox.Mode", RequiredType::String},
        {"roamingMode.graphics.skybox.Cubemap.Name", RequiredType::String},
        {"roamingMode.camera.default.fovYDegrees", RequiredType::Float},
        {"roamingMode.camera.default.nearClip", RequiredType::Float},
        {"roamingMode.camera.default.farClip", RequiredType::Float},
        {"roamingMode.camera.roaming.MoveSpeed", RequiredType::Float},
        {"roamingMode.camera.roaming.FastMultiplier", RequiredType::Float},
        {"roamingMode.camera.roaming.LookSensitivity", RequiredType::Float},
        {"roamingMode.camera.roaming.LookSmoothing", RequiredType::Float},
        {"roamingMode.camera.roaming.InvertY", RequiredType::Bool},
        {"roamingMode.camera.roaming.StartYawOffsetDeg", RequiredType::Float},
        {"roamingMode.camera.roaming.StartPitchOffsetDeg", RequiredType::Float},
        {"assets.hud.fonts.console.Regular.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Title.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Heading.Size", RequiredType::Float},
        {"assets.hud.fonts.console.Button.Size", RequiredType::Float}
    });
    return keys;
}

} // namespace

namespace karma::common::config {

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
    return ClientKeys();
}

std::vector<RequiredKey> ServerRequiredKeys() {
    return {};
}

} // namespace karma::common::config

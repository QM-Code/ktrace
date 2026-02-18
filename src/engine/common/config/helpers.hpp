#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <cstdint>

#include "common/serialization/json.hpp"

namespace karma::common::config {

bool ReadBoolConfig(std::initializer_list<const char*> paths, bool defaultValue);
uint16_t ReadUInt16Config(std::initializer_list<const char*> paths, uint16_t defaultValue);
float ReadFloatConfig(std::initializer_list<const char*> paths, float defaultValue);
std::string ReadStringConfig(const char *path, const std::string &defaultValue);
bool ReadRequiredBoolConfig(const char *path);
uint16_t ReadRequiredUInt16Config(const char *path);
float ReadRequiredFloatConfig(const char *path);
std::string ReadRequiredStringConfig(const char *path);
std::vector<float> ReadRequiredFloatArrayConfig(std::string_view path);
std::vector<std::string> ReadRequiredStringArrayConfig(std::string_view path);
const karma::common::serialization::Value& ReadRequiredObjectConfig(std::string_view path);

} // namespace karma::common::config

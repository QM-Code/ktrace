#pragma once

#include <array>
#include <string>

namespace ui {
namespace config {

float GetRequiredFloat(const char* path);
std::string GetRequiredString(const char* path);
bool GetRequiredBool(const char* path);
std::array<float, 4> GetRequiredColor(const char* path);

} // namespace config
} // namespace ui

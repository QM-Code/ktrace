#pragma once

#include <string>
#include <vector>

namespace karma::config {

enum class RequiredType {
    Bool,
    UInt16,
    Float,
    String
};

struct RequiredKey {
    const char* path;
    RequiredType type;
};

struct ValidationIssue {
    std::string path;
    std::string message;
};

std::vector<ValidationIssue> ValidateRequiredKeys(const std::vector<RequiredKey>& keys);

std::vector<RequiredKey> ClientRequiredKeys();
std::vector<RequiredKey> ServerRequiredKeys();

} // namespace karma::config

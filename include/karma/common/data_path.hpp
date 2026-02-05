#pragma once

#include <filesystem>
#include <string>

namespace karma::data {

std::filesystem::path DataRoot();
std::filesystem::path Resolve(const std::string& relative);

} // namespace karma::data

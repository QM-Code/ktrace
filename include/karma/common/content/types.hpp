#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace karma::content {

using ArchiveBytes = std::vector<std::byte>;

inline constexpr uint64_t kFNV1aOffsetBasis64 = 14695981039346656037ULL;
inline constexpr uint64_t kFNV1aPrime64 = 1099511628211ULL;

} // namespace karma::content

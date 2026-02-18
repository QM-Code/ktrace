#pragma once

#include "client/net/connection/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace bz3::client::net {

inline constexpr const char* kRuntimeLayerLabel = "world package config";
inline constexpr const char* kPackageMountId = "world.package";
inline constexpr const char* kWorldIdentityFile = "active_world_identity.txt";
inline constexpr const char* kWorldManifestFile = "active_world_manifest.txt";
inline constexpr const char* kWorldPackagesDir = "world-packages";
inline constexpr const char* kWorldPackagesByWorldDir = "by-world";
inline constexpr size_t kMaxCachePathComponentLen = 96;
inline constexpr uint16_t kDefaultMaxRevisionsPerWorld = 4;
inline constexpr uint16_t kDefaultMaxPackagesPerRevision = 2;

using CachedWorldIdentity = detail::CachedWorldIdentity;

std::filesystem::path ActiveWorldIdentityPath(const std::filesystem::path& server_cache_dir);
std::filesystem::path ActiveWorldManifestPath(const std::filesystem::path& server_cache_dir);
std::filesystem::path WorldPackagesByWorldRoot(const std::filesystem::path& server_cache_dir);

void ClearCachedWorldIdentity(const std::filesystem::path& server_cache_dir);

} // namespace bz3::client::net

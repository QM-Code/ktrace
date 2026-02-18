#pragma once

#include "karma/scene/physics_components.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace karma::physics::detail {

enum class StaticMeshIngestTraceCause : uint8_t {
    None = 0,
    InvalidIntent,
    IneligibleDynamicMeshIntent,
    MeshAssetLoadOrCookFailed,
    BackendRejectCreate
};

inline const char* StaticMeshIngestTraceCauseTag(StaticMeshIngestTraceCause cause) {
    switch (cause) {
        case StaticMeshIngestTraceCause::None:
            return "none";
        case StaticMeshIngestTraceCause::InvalidIntent:
            return "invalid_intent";
        case StaticMeshIngestTraceCause::IneligibleDynamicMeshIntent:
            return "ineligible_dynamic_mesh_intent";
        case StaticMeshIngestTraceCause::MeshAssetLoadOrCookFailed:
            return "mesh_asset_load_or_cook_failed";
        case StaticMeshIngestTraceCause::BackendRejectCreate:
            return "backend_reject_create";
        default:
            return "unknown";
    }
}

inline StaticMeshIngestTraceCause ClassifyStaticMeshCreateRejectCause(std::string_view mesh_path) {
    if (!scene::IsNonEmptyPath(std::string(mesh_path))) {
        return StaticMeshIngestTraceCause::InvalidIntent;
    }

    std::error_code ec{};
    const std::filesystem::path path(mesh_path);
    const bool path_exists = std::filesystem::exists(path, ec);
    if (ec || !path_exists) {
        return StaticMeshIngestTraceCause::MeshAssetLoadOrCookFailed;
    }
    const bool path_is_regular_file = std::filesystem::is_regular_file(path, ec);
    if (ec || !path_is_regular_file) {
        return StaticMeshIngestTraceCause::MeshAssetLoadOrCookFailed;
    }
    return StaticMeshIngestTraceCause::BackendRejectCreate;
}

} // namespace karma::physics::detail

#pragma once

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace karma::renderer_backend::detail {

struct ResolvedMaterialSemantics {
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float normal_variation = 0.0f;
    float occlusion = 1.0f;
    float occlusion_edge = 0.0f;
    renderer::MaterialAlphaMode alpha_mode = renderer::MaterialAlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
    bool alpha_blend = false;
    bool draw = true;
    bool used_metallic_roughness_texture = false;
    bool used_emissive_texture = false;
    bool used_normal_texture = false;
    bool used_occlusion_texture = false;
};

inline float Clamp01(float value, float fallback = 0.0f) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

inline float ClampPositive(float value, float fallback = 0.0f, float max_value = 8.0f) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0f, max_value);
}

inline std::size_t ResolveTextureChannels(const renderer::MeshData::TextureData& texture) {
    if (texture.channels > 0) {
        return static_cast<std::size_t>(texture.channels);
    }
    if (texture.width <= 0 || texture.height <= 0) {
        return 0;
    }
    const std::size_t pixel_count =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    if (pixel_count == 0) {
        return 0;
    }
    const std::size_t inferred_channels = texture.pixels.size() / pixel_count;
    return inferred_channels;
}

inline bool IsUsableTexture(const renderer::MeshData::TextureData& texture) {
    if (texture.width <= 0 || texture.height <= 0 || texture.pixels.empty()) {
        return false;
    }
    const std::size_t channels = ResolveTextureChannels(texture);
    if (channels == 0) {
        return false;
    }
    const std::size_t required =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * channels;
    return texture.pixels.size() >= required;
}

inline glm::vec4 DecodeTextureSample(const uint8_t* texel, std::size_t channels) {
    const auto normalize = [](uint8_t value) -> float {
        return static_cast<float>(value) / 255.0f;
    };

    if (channels == 1) {
        const float v = normalize(texel[0]);
        return glm::vec4(v, v, v, 1.0f);
    }
    if (channels == 2) {
        const float v = normalize(texel[0]);
        const float a = normalize(texel[1]);
        return glm::vec4(v, v, v, a);
    }
    if (channels == 3) {
        return glm::vec4(
            normalize(texel[0]),
            normalize(texel[1]),
            normalize(texel[2]),
            1.0f);
    }
    return glm::vec4(
        normalize(texel[0]),
        normalize(texel[1]),
        normalize(texel[2]),
        normalize(texel[3]));
}

inline glm::vec4 SampleTextureTexel(const renderer::MeshData::TextureData& texture,
                                    std::size_t x,
                                    std::size_t y) {
    if (!IsUsableTexture(texture)) {
        return glm::vec4(1.0f);
    }
    const std::size_t channels = ResolveTextureChannels(texture);
    const std::size_t clamped_channels = std::clamp<std::size_t>(channels, 1u, 4u);
    const std::size_t width = static_cast<std::size_t>(texture.width);
    const std::size_t height = static_cast<std::size_t>(texture.height);
    const std::size_t sx = std::min(x, width - 1u);
    const std::size_t sy = std::min(y, height - 1u);
    const std::size_t base_index = ((sy * width) + sx) * channels;
    if (base_index + clamped_channels > texture.pixels.size()) {
        return glm::vec4(1.0f);
    }
    return DecodeTextureSample(texture.pixels.data() + base_index, clamped_channels);
}

inline glm::vec4 SampleOriginTexel(const renderer::MeshData::TextureData& texture) {
    return SampleTextureTexel(texture, 0u, 0u);
}

inline glm::vec4 SampleRepresentativeTexel(const renderer::MeshData::TextureData& texture) {
    if (!IsUsableTexture(texture)) {
        return glm::vec4(1.0f);
    }
    const std::size_t width = static_cast<std::size_t>(texture.width);
    const std::size_t height = static_cast<std::size_t>(texture.height);
    const std::array<std::pair<std::size_t, std::size_t>, 5> sample_points{{
        {0u, 0u},
        {width - 1u, 0u},
        {0u, height - 1u},
        {width - 1u, height - 1u},
        {width / 2u, height / 2u},
    }};
    glm::vec4 accum{0.0f};
    for (const auto& [sx, sy] : sample_points) {
        accum += SampleTextureTexel(texture, sx, sy);
    }
    return accum / static_cast<float>(sample_points.size());
}

inline float RadicalInverseBase(uint32_t value, uint32_t base) {
    float reversed = 0.0f;
    float inv_base = 1.0f / static_cast<float>(base);
    float inv = inv_base;
    while (value > 0u) {
        const uint32_t digit = value % base;
        reversed += static_cast<float>(digit) * inv;
        value /= base;
        inv *= inv_base;
    }
    return reversed;
}

inline std::pair<std::size_t, std::size_t> ResolveQuasirandomSampleCoord(
    const renderer::MeshData::TextureData& texture,
    uint32_t sample_index) {
    const std::size_t width = static_cast<std::size_t>(texture.width);
    const std::size_t height = static_cast<std::size_t>(texture.height);
    const float u = RadicalInverseBase(sample_index + 1u, 2u);
    const float v = RadicalInverseBase(sample_index + 1u, 3u);
    const std::size_t x = std::min(width - 1u, static_cast<std::size_t>(u * static_cast<float>(width)));
    const std::size_t y = std::min(height - 1u, static_cast<std::size_t>(v * static_cast<float>(height)));
    return {x, y};
}

inline constexpr uint32_t kHighFrequencySampleCount = 32u;

struct ScalarSampleStats {
    float mean = 0.0f;
    float min = 1.0f;
    float max = 0.0f;
    float variance = 0.0f;
};

inline ScalarSampleStats SampleNormalVariationStats(const renderer::MeshData::TextureData& texture) {
    if (!IsUsableTexture(texture)) {
        return {};
    }
    ScalarSampleStats stats{};
    float accum = 0.0f;
    float accum_sq = 0.0f;
    for (uint32_t i = 0u; i < kHighFrequencySampleCount; ++i) {
        const auto [x, y] = ResolveQuasirandomSampleCoord(texture, i);
        const glm::vec4 sample = SampleTextureTexel(texture, x, y);
        const glm::vec2 encoded_xy = glm::vec2(sample.r, sample.g) * 2.0f - glm::vec2(1.0f);
        const float value = std::clamp(glm::length(encoded_xy), 0.0f, 1.0f);
        accum += value;
        accum_sq += (value * value);
        stats.min = std::min(stats.min, value);
        stats.max = std::max(stats.max, value);
    }
    stats.mean = accum / static_cast<float>(kHighFrequencySampleCount);
    const float second_moment = accum_sq / static_cast<float>(kHighFrequencySampleCount);
    stats.variance = std::max(0.0f, second_moment - (stats.mean * stats.mean));
    return stats;
}

inline ScalarSampleStats SampleOcclusionStats(const renderer::MeshData::TextureData& texture) {
    if (!IsUsableTexture(texture)) {
        ScalarSampleStats identity{};
        identity.mean = 1.0f;
        identity.min = 1.0f;
        identity.max = 1.0f;
        identity.variance = 0.0f;
        return identity;
    }
    ScalarSampleStats stats{};
    float accum = 0.0f;
    float accum_sq = 0.0f;
    for (uint32_t i = 0u; i < kHighFrequencySampleCount; ++i) {
        const auto [x, y] = ResolveQuasirandomSampleCoord(texture, i);
        const glm::vec4 sample = SampleTextureTexel(texture, x, y);
        const float value = Clamp01(sample.r, 1.0f);
        accum += value;
        accum_sq += (value * value);
        stats.min = std::min(stats.min, value);
        stats.max = std::max(stats.max, value);
    }
    stats.mean = accum / static_cast<float>(kHighFrequencySampleCount);
    const float second_moment = accum_sq / static_cast<float>(kHighFrequencySampleCount);
    stats.variance = std::max(0.0f, second_moment - (stats.mean * stats.mean));
    return stats;
}

inline float ResolveNormalVariationPolicy(const ScalarSampleStats& stats, float normal_scale) {
    const float deadzone = 0.06f;
    const float normalized = std::clamp((stats.mean - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
    const float variance_norm = std::clamp(std::sqrt(std::max(stats.variance, 0.0f)) * 1.35f, 0.0f, 1.0f);
    const float contrast = std::clamp(stats.max - stats.min, 0.0f, 1.0f);
    const float stability = std::clamp(1.0f - (0.30f * variance_norm) - (0.10f * contrast * variance_norm), 0.0f, 1.0f);
    return std::clamp(normalized * stability * normal_scale, 0.0f, 1.0f);
}

inline float ResolveOcclusionPolicy(const ScalarSampleStats& stats, float occlusion_strength) {
    float ao_mean = Clamp01(stats.mean, 1.0f);
    if (stats.max <= (1.0f / 255.0f)) {
        ao_mean = 0.0f;
    } else if (stats.min >= (254.0f / 255.0f)) {
        ao_mean = 1.0f;
    } else {
        const float contrast = std::clamp(stats.max - stats.min, 0.0f, 1.0f);
        const float center_weight = 1.0f - std::clamp(std::abs(ao_mean - 0.5f) * 2.0f, 0.0f, 1.0f);
        const float edge_bias = 0.08f * contrast * center_weight;
        ao_mean = std::clamp(ao_mean + edge_bias, 0.0f, 1.0f);
    }

    float resolved = 1.0f - (occlusion_strength * (1.0f - ao_mean));
    resolved = std::clamp(resolved, 0.0f, 1.0f);
    if (resolved < 1e-4f) {
        resolved = 0.0f;
    } else if (resolved > (1.0f - 1e-4f)) {
        resolved = 1.0f;
    }
    return resolved;
}

inline float ResolveOcclusionEdgePolicy(const ScalarSampleStats& stats) {
    const float contrast = std::clamp(stats.max - stats.min, 0.0f, 1.0f);
    const float variance_norm = std::clamp(std::sqrt(std::max(stats.variance, 0.0f)) * 2.0f, 0.0f, 1.0f);
    const float center_weight = 1.0f - std::clamp(std::abs(stats.mean - 0.5f) * 2.0f, 0.0f, 1.0f);
    return std::clamp(0.60f * contrast * ((0.50f * variance_norm) + (0.50f * center_weight)), 0.0f, 1.0f);
}

inline constexpr int kTextureLifecycleMaxDimension = 1024;
inline constexpr std::size_t kTextureLifecycleMaxTexels = 1024u * 1024u;

enum class MaterialTextureSemantic : uint8_t {
    Normal = 0,
    Occlusion = 1,
};

struct TextureLifecycleIngestion {
    std::optional<renderer::MeshData::TextureData> texture{};
    bool bounded = false;
};

struct MaterialTextureSetLifecycleIngestion {
    TextureLifecycleIngestion normal{};
    TextureLifecycleIngestion occlusion{};
};

struct MaterialShaderPathTextureIngestion {
    std::optional<renderer::MeshData::TextureData> texture{};
    bool used_albedo_texture = false;
    bool used_normal_lifecycle_texture = false;
    bool used_occlusion_lifecycle_texture = false;
    bool bounded = false;
};

enum class MaterialShaderTextureInputPath : uint8_t {
    Disabled = 0,
    DirectMultiSampler = 1,
    CompositeFallback = 2,
};

struct MaterialShaderInputContract {
    MaterialShaderTextureInputPath path = MaterialShaderTextureInputPath::Disabled;
    std::optional<renderer::MeshData::TextureData> direct_albedo_texture{};
    std::optional<renderer::MeshData::TextureData> direct_normal_texture{};
    std::optional<renderer::MeshData::TextureData> direct_occlusion_texture{};
    std::optional<renderer::MeshData::TextureData> fallback_composite_texture{};
    bool used_albedo_texture = false;
    bool used_normal_lifecycle_texture = false;
    bool used_occlusion_lifecycle_texture = false;
    bool bounded = false;
};

inline std::pair<int, int> ResolveBoundedLifecycleDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        return {0, 0};
    }
    int bounded_width = width;
    int bounded_height = height;

    if (bounded_width > kTextureLifecycleMaxDimension || bounded_height > kTextureLifecycleMaxDimension) {
        const float scale_x =
            static_cast<float>(kTextureLifecycleMaxDimension) / static_cast<float>(bounded_width);
        const float scale_y =
            static_cast<float>(kTextureLifecycleMaxDimension) / static_cast<float>(bounded_height);
        const float scale = std::min(scale_x, scale_y);
        bounded_width = std::max(1, static_cast<int>(std::floor(static_cast<float>(bounded_width) * scale)));
        bounded_height = std::max(1, static_cast<int>(std::floor(static_cast<float>(bounded_height) * scale)));
    }

    std::size_t bounded_texels =
        static_cast<std::size_t>(bounded_width) * static_cast<std::size_t>(bounded_height);
    if (bounded_texels > kTextureLifecycleMaxTexels) {
        const float scale = std::sqrt(
            static_cast<float>(kTextureLifecycleMaxTexels) / static_cast<float>(bounded_texels));
        bounded_width = std::max(1, static_cast<int>(std::floor(static_cast<float>(bounded_width) * scale)));
        bounded_height = std::max(1, static_cast<int>(std::floor(static_cast<float>(bounded_height) * scale)));
        while (static_cast<std::size_t>(bounded_width) * static_cast<std::size_t>(bounded_height) >
               kTextureLifecycleMaxTexels) {
            if (bounded_width >= bounded_height && bounded_width > 1) {
                --bounded_width;
            } else if (bounded_height > 1) {
                --bounded_height;
            } else {
                break;
            }
        }
    }

    return {bounded_width, bounded_height};
}

inline TextureLifecycleIngestion IngestMaterialTextureLifecycle(
    const std::optional<renderer::MeshData::TextureData>& texture,
    MaterialTextureSemantic semantic) {
    TextureLifecycleIngestion ingestion{};
    if (!texture || !IsUsableTexture(*texture)) {
        return ingestion;
    }

    const int src_width = texture->width;
    const int src_height = texture->height;
    const auto [dst_width, dst_height] = ResolveBoundedLifecycleDimensions(src_width, src_height);
    if (dst_width <= 0 || dst_height <= 0) {
        return ingestion;
    }

    renderer::MeshData::TextureData out{};
    out.width = dst_width;
    out.height = dst_height;
    out.channels = (semantic == MaterialTextureSemantic::Occlusion) ? 1 : 3;
    out.pixels.assign(
        static_cast<std::size_t>(dst_width) * static_cast<std::size_t>(dst_height) *
            static_cast<std::size_t>(out.channels),
        0u);

    const std::size_t src_width_u = static_cast<std::size_t>(src_width);
    const std::size_t src_height_u = static_cast<std::size_t>(src_height);
    for (std::size_t y = 0; y < static_cast<std::size_t>(dst_height); ++y) {
        const std::size_t src_y = std::min(
            src_height_u - 1u,
            (y * src_height_u) / static_cast<std::size_t>(dst_height));
        for (std::size_t x = 0; x < static_cast<std::size_t>(dst_width); ++x) {
            const std::size_t src_x = std::min(
                src_width_u - 1u,
                (x * src_width_u) / static_cast<std::size_t>(dst_width));
            const glm::vec4 sample = SampleTextureTexel(*texture, src_x, src_y);

            const std::size_t dst = ((y * static_cast<std::size_t>(dst_width)) + x) *
                                    static_cast<std::size_t>(out.channels);
            if (semantic == MaterialTextureSemantic::Occlusion) {
                out.pixels[dst] = static_cast<uint8_t>(std::clamp(sample.r, 0.0f, 1.0f) * 255.0f + 0.5f);
            } else {
                out.pixels[dst + 0u] = static_cast<uint8_t>(std::clamp(sample.r, 0.0f, 1.0f) * 255.0f + 0.5f);
                out.pixels[dst + 1u] = static_cast<uint8_t>(std::clamp(sample.g, 0.0f, 1.0f) * 255.0f + 0.5f);
                out.pixels[dst + 2u] = static_cast<uint8_t>(std::clamp(sample.b, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }

    ingestion.bounded = (dst_width != src_width) || (dst_height != src_height);
    ingestion.texture = std::move(out);
    return ingestion;
}

inline TextureLifecycleIngestion IngestMaterialTextureLifecycle(
    const renderer::MeshData::TextureData& texture,
    MaterialTextureSemantic semantic) {
    return IngestMaterialTextureLifecycle(
        std::optional<renderer::MeshData::TextureData>{texture}, semantic);
}

inline MaterialTextureSetLifecycleIngestion IngestMaterialTextureSetForLifecycle(
    const renderer::MaterialDesc& material) {
    MaterialTextureSetLifecycleIngestion ingestion{};
    ingestion.normal = IngestMaterialTextureLifecycle(material.normal, MaterialTextureSemantic::Normal);
    ingestion.occlusion = IngestMaterialTextureLifecycle(material.occlusion, MaterialTextureSemantic::Occlusion);
    return ingestion;
}

inline glm::vec4 SampleTextureTexelMapped(const renderer::MeshData::TextureData& texture,
                                          std::size_t x,
                                          std::size_t y,
                                          std::size_t target_width,
                                          std::size_t target_height) {
    if (!IsUsableTexture(texture) || target_width == 0u || target_height == 0u) {
        return glm::vec4(1.0f);
    }
    const std::size_t src_width = static_cast<std::size_t>(texture.width);
    const std::size_t src_height = static_cast<std::size_t>(texture.height);
    const std::size_t src_x =
        std::min(src_width - 1u, (x * src_width) / target_width);
    const std::size_t src_y =
        std::min(src_height - 1u, (y * src_height) / target_height);
    return SampleTextureTexel(texture, src_x, src_y);
}

inline float ResolveShaderNormalModulation(const glm::vec4& normal_sample) {
    const glm::vec2 encoded_xy = glm::vec2(normal_sample.r, normal_sample.g) * 2.0f - glm::vec2(1.0f);
    const float normal_response = std::clamp(glm::length(encoded_xy), 0.0f, 1.0f);
    return std::clamp(1.0f + (0.16f * std::max(0.0f, normal_response - 0.18f)), 1.0f, 1.18f);
}

inline float ResolveShaderOcclusionModulation(const glm::vec4& occlusion_sample) {
    return std::clamp(0.25f + (0.75f * Clamp01(occlusion_sample.r, 1.0f)), 0.25f, 1.0f);
}

inline float ResolveShaderCombinedModulation(const std::optional<glm::vec4>& normal_sample,
                                             const std::optional<glm::vec4>& occlusion_sample) {
    float modulation = 1.0f;
    if (normal_sample.has_value()) {
        modulation *= ResolveShaderNormalModulation(*normal_sample);
    }
    if (occlusion_sample.has_value()) {
        modulation *= ResolveShaderOcclusionModulation(*occlusion_sample);
    }
    return std::clamp(modulation, 0.20f, 1.20f);
}

inline glm::vec4 ResolveShaderPathBaseSample(
    const glm::vec4& albedo_sample,
    const std::optional<glm::vec4>& normal_sample,
    const std::optional<glm::vec4>& occlusion_sample) {
    const float modulation = ResolveShaderCombinedModulation(normal_sample, occlusion_sample);
    const glm::vec3 rgb = glm::clamp(
        glm::vec3(albedo_sample) * modulation,
        glm::vec3(0.0f),
        glm::vec3(1.0f));
    return glm::vec4(rgb, std::clamp(albedo_sample.a, 0.0f, 1.0f));
}

inline glm::vec4 ResolveShaderPathDirectSample(
    const renderer::MeshData::TextureData* albedo_texture,
    const renderer::MeshData::TextureData* normal_texture,
    const renderer::MeshData::TextureData* occlusion_texture,
    std::size_t x,
    std::size_t y,
    std::size_t target_width,
    std::size_t target_height) {
    const glm::vec4 base = (albedo_texture && IsUsableTexture(*albedo_texture))
        ? SampleTextureTexelMapped(*albedo_texture, x, y, target_width, target_height)
        : glm::vec4(1.0f);
    const std::optional<glm::vec4> normal = (normal_texture && IsUsableTexture(*normal_texture))
        ? std::optional<glm::vec4>(SampleTextureTexelMapped(*normal_texture, x, y, target_width, target_height))
        : std::nullopt;
    const std::optional<glm::vec4> occlusion = (occlusion_texture && IsUsableTexture(*occlusion_texture))
        ? std::optional<glm::vec4>(SampleTextureTexelMapped(*occlusion_texture, x, y, target_width, target_height))
        : std::nullopt;
    return ResolveShaderPathBaseSample(base, normal, occlusion);
}

inline MaterialShaderPathTextureIngestion BuildShaderPathTextureIngestion(
    const renderer::MaterialDesc& material,
    const MaterialTextureSetLifecycleIngestion& lifecycle_ingestion) {
    MaterialShaderPathTextureIngestion ingestion{};
    const bool has_albedo = material.albedo && IsUsableTexture(*material.albedo);
    const bool has_normal =
        lifecycle_ingestion.normal.texture && IsUsableTexture(*lifecycle_ingestion.normal.texture);
    const bool has_occlusion =
        lifecycle_ingestion.occlusion.texture &&
        IsUsableTexture(*lifecycle_ingestion.occlusion.texture);
    if (!has_albedo && !has_normal && !has_occlusion) {
        return ingestion;
    }

    std::size_t width = has_albedo
        ? static_cast<std::size_t>(material.albedo->width)
        : 1u;
    std::size_t height = has_albedo
        ? static_cast<std::size_t>(material.albedo->height)
        : 1u;
    if (!has_albedo) {
        if (has_normal) {
            width = std::max(width, static_cast<std::size_t>(lifecycle_ingestion.normal.texture->width));
            height = std::max(height, static_cast<std::size_t>(lifecycle_ingestion.normal.texture->height));
        }
        if (has_occlusion) {
            width = std::max(width, static_cast<std::size_t>(lifecycle_ingestion.occlusion.texture->width));
            height = std::max(height, static_cast<std::size_t>(lifecycle_ingestion.occlusion.texture->height));
        }
    }
    if (width == 0u || height == 0u) {
        return ingestion;
    }

    renderer::MeshData::TextureData out{};
    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.channels = 4;
    out.pixels.assign(width * height * 4u, 0u);

    const renderer::MeshData::TextureData* albedo_texture = has_albedo ? &(*material.albedo) : nullptr;
    const renderer::MeshData::TextureData* normal_texture = has_normal ? &(*lifecycle_ingestion.normal.texture) : nullptr;
    const renderer::MeshData::TextureData* occlusion_texture =
        has_occlusion ? &(*lifecycle_ingestion.occlusion.texture) : nullptr;

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const glm::vec4 composed = ResolveShaderPathDirectSample(
                albedo_texture,
                normal_texture,
                occlusion_texture,
                x,
                y,
                width,
                height);

            const std::size_t dst = ((y * width) + x) * 4u;
            out.pixels[dst + 0u] = static_cast<uint8_t>(composed.r * 255.0f + 0.5f);
            out.pixels[dst + 1u] = static_cast<uint8_t>(composed.g * 255.0f + 0.5f);
            out.pixels[dst + 2u] = static_cast<uint8_t>(composed.b * 255.0f + 0.5f);
            out.pixels[dst + 3u] = static_cast<uint8_t>(composed.a * 255.0f + 0.5f);
        }
    }

    ingestion.texture = std::move(out);
    ingestion.used_albedo_texture = has_albedo;
    ingestion.used_normal_lifecycle_texture = has_normal;
    ingestion.used_occlusion_lifecycle_texture = has_occlusion;
    ingestion.bounded = lifecycle_ingestion.normal.bounded || lifecycle_ingestion.occlusion.bounded;
    return ingestion;
}

inline MaterialShaderPathTextureIngestion BuildShaderPathTextureIngestion(
    const renderer::MaterialDesc& material) {
    return BuildShaderPathTextureIngestion(
        material, IngestMaterialTextureSetForLifecycle(material));
}

inline MaterialShaderInputContract ResolveMaterialShaderInputContract(
    const renderer::MaterialDesc& material,
    const MaterialTextureSetLifecycleIngestion& lifecycle_ingestion,
    bool direct_multi_sampler_available) {
    MaterialShaderInputContract contract{};

    const bool has_albedo = material.albedo && IsUsableTexture(*material.albedo);
    const bool has_normal =
        lifecycle_ingestion.normal.texture && IsUsableTexture(*lifecycle_ingestion.normal.texture);
    const bool has_occlusion =
        lifecycle_ingestion.occlusion.texture &&
        IsUsableTexture(*lifecycle_ingestion.occlusion.texture);

    if (!has_albedo && !has_normal && !has_occlusion) {
        return contract;
    }

    contract.used_albedo_texture = has_albedo;
    contract.used_normal_lifecycle_texture = has_normal;
    contract.used_occlusion_lifecycle_texture = has_occlusion;
    contract.bounded = lifecycle_ingestion.normal.bounded || lifecycle_ingestion.occlusion.bounded;

    if (direct_multi_sampler_available && (has_normal || has_occlusion)) {
        contract.path = MaterialShaderTextureInputPath::DirectMultiSampler;
        if (has_albedo) {
            contract.direct_albedo_texture = *material.albedo;
        }
        if (has_normal) {
            contract.direct_normal_texture = *lifecycle_ingestion.normal.texture;
        }
        if (has_occlusion) {
            contract.direct_occlusion_texture = *lifecycle_ingestion.occlusion.texture;
        }
        return contract;
    }

    const MaterialShaderPathTextureIngestion composite_ingestion =
        BuildShaderPathTextureIngestion(material, lifecycle_ingestion);
    if (composite_ingestion.texture) {
        contract.path = MaterialShaderTextureInputPath::CompositeFallback;
        contract.fallback_composite_texture = composite_ingestion.texture;
        contract.used_albedo_texture = composite_ingestion.used_albedo_texture;
        contract.used_normal_lifecycle_texture = composite_ingestion.used_normal_lifecycle_texture;
        contract.used_occlusion_lifecycle_texture = composite_ingestion.used_occlusion_lifecycle_texture;
        contract.bounded = composite_ingestion.bounded;
    }
    return contract;
}

inline MaterialShaderInputContract ResolveMaterialShaderInputContract(
    const renderer::MaterialDesc& material,
    bool direct_multi_sampler_available) {
    return ResolveMaterialShaderInputContract(
        material,
        IngestMaterialTextureSetForLifecycle(material),
        direct_multi_sampler_available);
}

inline std::vector<uint8_t> ExpandTextureToRgba8(const renderer::MeshData::TextureData& texture) {
    if (!IsUsableTexture(texture)) {
        return {};
    }
    const std::size_t width = static_cast<std::size_t>(texture.width);
    const std::size_t height = static_cast<std::size_t>(texture.height);
    const std::size_t pixel_count = width * height;
    std::vector<uint8_t> out_rgba(pixel_count * 4u, 0u);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const glm::vec4 sample = SampleTextureTexel(texture, x, y);
            const std::size_t dst = ((y * width) + x) * 4u;
            out_rgba[dst + 0u] = static_cast<uint8_t>(std::clamp(sample.r, 0.0f, 1.0f) * 255.0f + 0.5f);
            out_rgba[dst + 1u] = static_cast<uint8_t>(std::clamp(sample.g, 0.0f, 1.0f) * 255.0f + 0.5f);
            out_rgba[dst + 2u] = static_cast<uint8_t>(std::clamp(sample.b, 0.0f, 1.0f) * 255.0f + 0.5f);
            out_rgba[dst + 3u] = static_cast<uint8_t>(std::clamp(sample.a, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
    return out_rgba;
}

inline ResolvedMaterialSemantics ResolveMaterialSemantics(const renderer::MaterialDesc& material) {
    ResolvedMaterialSemantics semantics{};
    semantics.alpha_mode = material.alpha_mode;
    semantics.double_sided = material.double_sided;
    const MaterialTextureSetLifecycleIngestion texture_ingestion =
        IngestMaterialTextureSetForLifecycle(material);

    semantics.base_color.r = ClampPositive(material.base_color.r, 1.0f, 4.0f);
    semantics.base_color.g = ClampPositive(material.base_color.g, 1.0f, 4.0f);
    semantics.base_color.b = ClampPositive(material.base_color.b, 1.0f, 4.0f);
    semantics.base_color.a = Clamp01(material.base_color.a, 1.0f);

    semantics.emissive.r = ClampPositive(material.emissive_color.r, 0.0f, 8.0f);
    semantics.emissive.g = ClampPositive(material.emissive_color.g, 0.0f, 8.0f);
    semantics.emissive.b = ClampPositive(material.emissive_color.b, 0.0f, 8.0f);

    semantics.metallic = Clamp01(material.metallic_factor, 0.0f);
    semantics.roughness = std::clamp(Clamp01(material.roughness_factor, 1.0f), 0.04f, 1.0f);
    semantics.alpha_cutoff = Clamp01(material.alpha_cutoff, 0.5f);
    const float normal_scale = ClampPositive(material.normal_scale, 1.0f, 2.0f);
    const float occlusion_strength = Clamp01(material.occlusion_strength, 1.0f);

    if (material.metallic_roughness && IsUsableTexture(*material.metallic_roughness)) {
        const glm::vec4 mr = SampleRepresentativeTexel(*material.metallic_roughness);
        semantics.metallic = Clamp01(semantics.metallic * mr.b, 0.0f);
        semantics.roughness = std::clamp(Clamp01(semantics.roughness * mr.g, semantics.roughness), 0.04f, 1.0f);
        semantics.used_metallic_roughness_texture = true;
    }

    if (material.emissive && IsUsableTexture(*material.emissive)) {
        const glm::vec4 emissive_sample = SampleRepresentativeTexel(*material.emissive);
        semantics.emissive.r = ClampPositive(semantics.emissive.r * emissive_sample.r, semantics.emissive.r, 8.0f);
        semantics.emissive.g = ClampPositive(semantics.emissive.g * emissive_sample.g, semantics.emissive.g, 8.0f);
        semantics.emissive.b = ClampPositive(semantics.emissive.b * emissive_sample.b, semantics.emissive.b, 8.0f);
        semantics.used_emissive_texture = true;
    }

    if (texture_ingestion.normal.texture && IsUsableTexture(*texture_ingestion.normal.texture)) {
        const ScalarSampleStats normal_stats =
            SampleNormalVariationStats(*texture_ingestion.normal.texture);
        semantics.normal_variation = ResolveNormalVariationPolicy(normal_stats, normal_scale);
        semantics.used_normal_texture = true;
    }

    if (texture_ingestion.occlusion.texture &&
        IsUsableTexture(*texture_ingestion.occlusion.texture)) {
        const ScalarSampleStats occlusion_stats =
            SampleOcclusionStats(*texture_ingestion.occlusion.texture);
        semantics.occlusion = ResolveOcclusionPolicy(occlusion_stats, occlusion_strength);
        semantics.occlusion_edge = ResolveOcclusionEdgePolicy(occlusion_stats);
        semantics.used_occlusion_texture = true;
    }

    float effective_alpha = semantics.base_color.a;
    if (material.albedo && IsUsableTexture(*material.albedo)) {
        const glm::vec4 albedo_sample = SampleRepresentativeTexel(*material.albedo);
        effective_alpha = Clamp01(effective_alpha * albedo_sample.a, effective_alpha);
    }

    switch (semantics.alpha_mode) {
        case renderer::MaterialAlphaMode::Mask:
            semantics.draw = (effective_alpha >= semantics.alpha_cutoff);
            semantics.alpha_blend = false;
            semantics.base_color.a = 1.0f;
            break;
        case renderer::MaterialAlphaMode::Blend:
            semantics.draw = (effective_alpha > 1e-4f);
            semantics.alpha_blend = true;
            semantics.base_color.a = effective_alpha;
            break;
        case renderer::MaterialAlphaMode::Opaque:
        default:
            semantics.draw = true;
            semantics.alpha_blend = false;
            semantics.base_color.a = 1.0f;
            break;
    }

    return semantics;
}

inline bool ValidateResolvedMaterialSemantics(const ResolvedMaterialSemantics& semantics) {
    const auto finite_vec3 = [](const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    const auto finite_vec4 = [](const glm::vec4& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w);
    };
    if (!finite_vec4(semantics.base_color) || !finite_vec3(semantics.emissive)) {
        return false;
    }
    if (!std::isfinite(semantics.metallic) || !std::isfinite(semantics.roughness) || !std::isfinite(semantics.alpha_cutoff)) {
        return false;
    }
    if (!std::isfinite(semantics.normal_variation) || !std::isfinite(semantics.occlusion) ||
        !std::isfinite(semantics.occlusion_edge)) {
        return false;
    }
    if (semantics.metallic < 0.0f || semantics.metallic > 1.0f) {
        return false;
    }
    if (semantics.roughness < 0.04f || semantics.roughness > 1.0f) {
        return false;
    }
    if (semantics.base_color.a < 0.0f || semantics.base_color.a > 1.0f) {
        return false;
    }
    if (semantics.alpha_cutoff < 0.0f || semantics.alpha_cutoff > 1.0f) {
        return false;
    }
    if (semantics.normal_variation < 0.0f || semantics.normal_variation > 1.0f) {
        return false;
    }
    if (semantics.occlusion < 0.0f || semantics.occlusion > 1.0f) {
        return false;
    }
    if (semantics.occlusion_edge < 0.0f || semantics.occlusion_edge > 1.0f) {
        return false;
    }
    return true;
}

} // namespace karma::renderer_backend::detail

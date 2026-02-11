#include "renderer/backends/debug_line_internal.hpp"
#include "renderer/backends/direct_sampler_observability_internal.hpp"
#include "renderer/backends/directional_shadow_internal.hpp"
#include "renderer/backends/environment_lighting_internal.hpp"
#include "renderer/backends/material_lighting_internal.hpp"
#include "renderer/backends/material_semantics_internal.hpp"

#include "karma/renderer/types.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

using karma::renderer_backend::detail::ComputeDirectionalShadowFactor;
using karma::renderer_backend::detail::ComputeEnvironmentAmbientColor;
using karma::renderer_backend::detail::ComputeEnvironmentClearColor;
using karma::renderer_backend::detail::ComputeEnvironmentSpecularBoost;
using karma::renderer_backend::detail::BuildShaderPathTextureIngestion;
using karma::renderer_backend::detail::BuildRgba8MipChain;
using karma::renderer_backend::detail::ComputeRgba8MipLevelCount;
using karma::renderer_backend::detail::DirectSamplerDrawInvariantInput;
using karma::renderer_backend::detail::EvaluateBgfxDirectSamplerContract;
using karma::renderer_backend::detail::EvaluateDirectSamplerDrawInvariants;
using karma::renderer_backend::detail::EvaluateDiligentDirectSamplerContract;
using karma::renderer_backend::detail::ExpandTextureToRgba8;
using karma::renderer_backend::detail::IngestMaterialTextureLifecycle;
using karma::renderer_backend::detail::IngestMaterialTextureSetForLifecycle;
using karma::renderer_backend::detail::IsUsableTexture;
using karma::renderer_backend::detail::kDiligentMaterialVariantCount;
using karma::renderer_backend::detail::kTextureLifecycleMaxDimension;
using karma::renderer_backend::detail::kTextureLifecycleMaxTexels;
using karma::renderer_backend::detail::MaterialShaderTextureInputPath;
using karma::renderer_backend::detail::MaterialTextureSemantic;
using karma::renderer_backend::detail::ResolveDebugLineSemantics;
using karma::renderer_backend::detail::DirectionalShadowCaster;
using karma::renderer_backend::detail::DirectionalShadowMap;
using karma::renderer_backend::detail::ResolveEnvironmentLightingSemantics;
using karma::renderer_backend::detail::ResolveMaterialLighting;
using karma::renderer_backend::detail::ResolveMaterialShaderInputContract;
using karma::renderer_backend::detail::ResolveMaterialSemantics;
using karma::renderer_backend::detail::ResolveDirectionalShadowSemantics;
using karma::renderer_backend::detail::ResolveShaderPathDirectSample;
using karma::renderer_backend::detail::ResolvedDebugLineSemantics;
using karma::renderer_backend::detail::ResolvedEnvironmentLightingSemantics;
using karma::renderer_backend::detail::ResolvedMaterialLighting;
using karma::renderer_backend::detail::ResolvedMaterialSemantics;
using karma::renderer_backend::detail::ResolvedDirectionalShadowSemantics;
using karma::renderer_backend::detail::SamplerVariableAvailability;
using karma::renderer_backend::detail::SampleDirectionalShadowVisibility;
using karma::renderer_backend::detail::ComputeDirectionalShadowVisibilityForReceiver;
using karma::renderer_backend::detail::ValidateResolvedDebugLineSemantics;
using karma::renderer_backend::detail::ValidateResolvedEnvironmentLightingSemantics;
using karma::renderer_backend::detail::ValidateResolvedMaterialLighting;
using karma::renderer_backend::detail::ValidateResolvedMaterialSemantics;
using karma::renderer_backend::detail::ValidateResolvedDirectionalShadowSemantics;
using karma::renderer_backend::detail::BuildDirectionalShadowMap;

bool NearlyEqual(float lhs, float rhs, float epsilon = 1e-4f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

uint8_t ToU8(float value) {
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

bool ValidateCompositeMatchesDirectPath(
    const karma::renderer::MeshData::TextureData& composite,
    const karma::renderer::MeshData::TextureData* albedo,
    const karma::renderer::MeshData::TextureData* normal,
    const karma::renderer::MeshData::TextureData* occlusion,
    int max_lsb_delta,
    const char* context) {
    if (!IsUsableTexture(composite) || composite.channels != 4) {
        std::cerr << "invalid composite texture in direct/fallback comparison";
        if (context) {
            std::cerr << " (" << context << ")";
        }
        std::cerr << "\n";
        return false;
    }
    const std::size_t width = static_cast<std::size_t>(composite.width);
    const std::size_t height = static_cast<std::size_t>(composite.height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const glm::vec4 direct_sample = ResolveShaderPathDirectSample(
                albedo, normal, occlusion, x, y, width, height);
            const uint8_t expected[4] = {
                ToU8(direct_sample.r),
                ToU8(direct_sample.g),
                ToU8(direct_sample.b),
                ToU8(direct_sample.a),
            };
            const std::size_t base = ((y * width) + x) * 4u;
            for (std::size_t c = 0; c < 4u; ++c) {
                const int delta = std::abs(
                    static_cast<int>(composite.pixels[base + c]) -
                    static_cast<int>(expected[c]));
                if (delta > max_lsb_delta) {
                    std::cerr << "composite/direct mismatch at (" << x << "," << y << ") channel "
                              << c << " delta=" << delta;
                    if (context) {
                        std::cerr << " (" << context << ")";
                    }
                    std::cerr << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool RunShadowSemanticsClampChecks() {
    karma::renderer::DirectionalLightData light{};
    light.shadow.enabled = true;
    light.shadow.strength = std::numeric_limits<float>::quiet_NaN();
    light.shadow.bias = -1.0f;
    light.shadow.extent = 10000.0f;
    light.shadow.map_size = 2;
    light.shadow.pcf_radius = 99;

    const ResolvedDirectionalShadowSemantics semantics = ResolveDirectionalShadowSemantics(light);
    if (!ValidateResolvedDirectionalShadowSemantics(semantics)) {
        std::cerr << "resolved directional shadow semantics failed validation\n";
        return false;
    }
    if (!NearlyEqual(semantics.strength, 0.65f)) {
        std::cerr << "expected non-finite strength to fall back to 0.65\n";
        return false;
    }
    if (!NearlyEqual(semantics.bias, 0.0f)) {
        std::cerr << "expected finite out-of-range bias to clamp to 0.0\n";
        return false;
    }
    if (!NearlyEqual(semantics.extent, 512.0f)) {
        std::cerr << "expected finite out-of-range extent to clamp to 512.0\n";
        return false;
    }
    if (semantics.map_size != 256) {
        std::cerr << "expected invalid map size to fall back to 256\n";
        return false;
    }
    if (semantics.pcf_radius != 1) {
        std::cerr << "expected invalid pcf radius to fall back to 1\n";
        return false;
    }

    return true;
}

bool RunShadowMapBuildAndSampleChecks() {
    karma::renderer::DirectionalLightData light{};
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.shadow.enabled = true;
    light.shadow.strength = 0.75f;
    light.shadow.bias = 0.0005f;
    light.shadow.extent = 8.0f;
    light.shadow.map_size = 128;
    light.shadow.pcf_radius = 1;

    const ResolvedDirectionalShadowSemantics semantics = ResolveDirectionalShadowSemantics(light);
    if (!ValidateResolvedDirectionalShadowSemantics(semantics)) {
        std::cerr << "shadow semantics unexpectedly invalid for map build\n";
        return false;
    }

    std::vector<glm::vec3> quad_positions{
        {-0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, 0.5f},
        {-0.5f, 0.0f, 0.5f},
    };
    std::vector<uint32_t> quad_indices{0, 1, 2, 0, 2, 3};

    DirectionalShadowCaster caster{};
    caster.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
    caster.positions = &quad_positions;
    caster.indices = &quad_indices;
    caster.sample_center = glm::vec3(0.0f, 0.0f, 0.0f);
    caster.casts_shadow = true;

    const std::vector<DirectionalShadowCaster> casters{caster};
    const DirectionalShadowMap shadow_map = BuildDirectionalShadowMap(semantics, light.direction, casters);
    if (!shadow_map.ready || shadow_map.depth.empty()) {
        std::cerr << "shadow map build failed for a valid occluder\n";
        return false;
    }

    bool has_shadow_depth = false;
    for (const float depth : shadow_map.depth) {
        if (std::isfinite(depth)) {
            has_shadow_depth = true;
            break;
        }
    }
    if (!has_shadow_depth) {
        std::cerr << "expected shadow map rasterization to produce finite depth values\n";
        return false;
    }

    const float sampled_visibility =
        SampleDirectionalShadowVisibility(shadow_map, glm::vec3(0.0f, 0.0f, 0.0f));
    if (!std::isfinite(sampled_visibility) || sampled_visibility < 0.0f || sampled_visibility > 1.0f) {
        std::cerr << "sampled visibility is out of range: " << sampled_visibility << "\n";
        return false;
    }

    const float shadow_factor = ComputeDirectionalShadowFactor(shadow_map, sampled_visibility);
    if (!std::isfinite(shadow_factor) || shadow_factor < 0.0f || shadow_factor > 1.0f) {
        std::cerr << "shadow factor out of expected range, got " << shadow_factor << "\n";
        return false;
    }

    return true;
}

bool RunShadowReceiverVisibilityChecks() {
    karma::renderer::DirectionalLightData light{};
    light.direction = glm::vec3(0.35f, -1.0f, -0.25f);
    light.shadow.enabled = true;
    light.shadow.strength = 0.85f;
    light.shadow.bias = 0.0005f;
    light.shadow.extent = 10.0f;
    light.shadow.map_size = 256;
    light.shadow.pcf_radius = 1;

    const ResolvedDirectionalShadowSemantics semantics = ResolveDirectionalShadowSemantics(light);
    if (!ValidateResolvedDirectionalShadowSemantics(semantics)) {
        std::cerr << "shadow semantics unexpectedly invalid for receiver visibility checks\n";
        return false;
    }

    std::vector<glm::vec3> quad_positions{
        {-1.0f, 0.0f, -1.0f},
        {1.0f, 0.0f, -1.0f},
        {1.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 1.0f},
    };
    std::vector<uint32_t> quad_indices{0, 1, 2, 0, 2, 3};

    DirectionalShadowCaster occluder{};
    occluder.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
    occluder.positions = &quad_positions;
    occluder.indices = &quad_indices;
    occluder.sample_center = glm::vec3(0.0f, 0.0f, 0.0f);
    occluder.casts_shadow = true;
    const std::vector<DirectionalShadowCaster> occluders{occluder};

    const DirectionalShadowMap shadow_map = BuildDirectionalShadowMap(semantics, light.direction, occluders);
    if (!shadow_map.ready || shadow_map.depth.empty()) {
        std::cerr << "shadow map build failed for receiver visibility checks\n";
        return false;
    }

    const glm::mat4 receiver_transform = glm::mat4(1.0f);
    const float receiver_visibility = ComputeDirectionalShadowVisibilityForReceiver(
        shadow_map,
        receiver_transform,
        &quad_positions,
        &quad_indices,
        glm::vec3(0.0f, 0.0f, 0.0f));
    const float receiver_repeat = ComputeDirectionalShadowVisibilityForReceiver(
        shadow_map,
        receiver_transform,
        &quad_positions,
        &quad_indices,
        glm::vec3(0.0f, 0.0f, 0.0f));
    if (!std::isfinite(receiver_visibility) || receiver_visibility < 0.0f || receiver_visibility > 1.0f) {
        std::cerr << "receiver visibility out of range: " << receiver_visibility << "\n";
        return false;
    }
    if (!NearlyEqual(receiver_visibility, receiver_repeat, 1e-6f)) {
        std::cerr << "receiver visibility should be deterministic for identical input\n";
        return false;
    }

    const float outside_visibility =
        SampleDirectionalShadowVisibility(shadow_map, glm::vec3(6.0f, 0.0f, 6.0f));
    if (!std::isfinite(outside_visibility) || outside_visibility < 0.0f || outside_visibility > 1.0f) {
        std::cerr << "outside visibility out of range: " << outside_visibility << "\n";
        return false;
    }
    if (!(receiver_visibility + 0.05f < outside_visibility)) {
        std::cerr << "expected receiver sampling visibility to show stronger shadowing than outside point (receiver="
                  << receiver_visibility << ", outside=" << outside_visibility << ")\n";
        return false;
    }

    ResolvedDirectionalShadowSemantics high_bias_semantics = semantics;
    high_bias_semantics.bias = 0.02f;
    const DirectionalShadowMap high_bias_map =
        BuildDirectionalShadowMap(high_bias_semantics, light.direction, occluders);
    if (!high_bias_map.ready) {
        std::cerr << "high-bias shadow map unexpectedly not ready\n";
        return false;
    }
    const float high_bias_receiver_visibility = ComputeDirectionalShadowVisibilityForReceiver(
        high_bias_map,
        receiver_transform,
        &quad_positions,
        &quad_indices,
        glm::vec3(0.0f, 0.0f, 0.0f));
    if (!std::isfinite(high_bias_receiver_visibility) ||
        high_bias_receiver_visibility < receiver_visibility) {
        std::cerr << "expected higher bias to avoid additional shadowing (base=" << receiver_visibility
                  << ", high-bias=" << high_bias_receiver_visibility << ")\n";
        return false;
    }

    return true;
}

bool RunEnvironmentSemanticsChecks() {
    karma::renderer::EnvironmentLightingData environment{};
    environment.enabled = true;
    environment.sky_color = glm::vec4(std::numeric_limits<float>::quiet_NaN(), 20.0f, 0.4f, 1.0f);
    environment.ground_color = glm::vec4(0.1f, -2.0f, 20.0f, 1.0f);
    environment.diffuse_strength = std::numeric_limits<float>::quiet_NaN();
    environment.specular_strength = 5.0f;
    environment.skybox_exposure = -1.0f;

    const ResolvedEnvironmentLightingSemantics semantics = ResolveEnvironmentLightingSemantics(environment);
    if (!ValidateResolvedEnvironmentLightingSemantics(semantics)) {
        std::cerr << "resolved environment semantics failed validation\n";
        return false;
    }
    if (!NearlyEqual(semantics.sky_color.x, 0.56f)) {
        std::cerr << "expected non-finite sky color component to fall back\n";
        return false;
    }
    if (!NearlyEqual(semantics.sky_color.y, 8.0f)) {
        std::cerr << "expected large sky color component to clamp\n";
        return false;
    }
    if (!NearlyEqual(semantics.ground_color.y, 0.0f)) {
        std::cerr << "expected negative ground color component to clamp\n";
        return false;
    }
    if (!NearlyEqual(semantics.diffuse_strength, 0.75f)) {
        std::cerr << "expected non-finite diffuse strength to fall back\n";
        return false;
    }
    if (!NearlyEqual(semantics.specular_strength, 2.0f)) {
        std::cerr << "expected specular strength to clamp to max\n";
        return false;
    }
    if (!NearlyEqual(semantics.skybox_exposure, 0.0f)) {
        std::cerr << "expected negative skybox exposure to clamp to zero\n";
        return false;
    }

    const glm::vec3 ambient = ComputeEnvironmentAmbientColor(semantics, glm::vec3(0.0f, -1.0f, 0.0f));
    if (!std::isfinite(ambient.x) || !std::isfinite(ambient.y) || !std::isfinite(ambient.z)) {
        std::cerr << "environment ambient contains non-finite values\n";
        return false;
    }

    const float specular_boost = ComputeEnvironmentSpecularBoost(semantics, 0.2f);
    if (!std::isfinite(specular_boost) || specular_boost < 1.0f || specular_boost > 2.0f) {
        std::cerr << "environment specular boost out of range: " << specular_boost << "\n";
        return false;
    }

    const glm::vec4 clear_color = ComputeEnvironmentClearColor(semantics);
    if (!std::isfinite(clear_color.r) || !std::isfinite(clear_color.g) || !std::isfinite(clear_color.b)) {
        std::cerr << "environment clear color contains non-finite values\n";
        return false;
    }
    if (!NearlyEqual(clear_color.r, 0.0f) || !NearlyEqual(clear_color.g, 0.0f) || !NearlyEqual(clear_color.b, 0.0f)) {
        std::cerr << "expected zero exposure to produce black clear color\n";
        return false;
    }

    return true;
}

bool RunMaterialTextureSemanticsChecks() {
    karma::renderer::MaterialDesc material{};
    material.base_color = glm::vec4(1.0f);
    material.metallic_factor = 0.8f;
    material.roughness_factor = 0.9f;
    material.normal_scale = 1.5f;
    material.occlusion_strength = 0.75f;
    material.emissive_color = glm::vec3(2.0f, 1.0f, 0.5f);
    material.alpha_mode = karma::renderer::MaterialAlphaMode::Mask;
    material.alpha_cutoff = 0.5f;

    karma::renderer::MeshData::TextureData metallic_roughness{};
    metallic_roughness.width = 4;
    metallic_roughness.height = 4;
    metallic_roughness.channels = 4;
    metallic_roughness.pixels.assign(4u * 4u * 4u, 0u);
    {
        const std::size_t center = ((2u * 4u) + 2u) * 4u;
        metallic_roughness.pixels[center + 1u] = 255u;
        metallic_roughness.pixels[center + 2u] = 255u;
    }
    material.metallic_roughness = metallic_roughness;

    karma::renderer::MeshData::TextureData emissive{};
    emissive.width = 4;
    emissive.height = 4;
    emissive.channels = 4;
    emissive.pixels.assign(4u * 4u * 4u, 0u);
    {
        const std::size_t center = ((2u * 4u) + 2u) * 4u;
        emissive.pixels[center + 0u] = 255u;
        emissive.pixels[center + 1u] = 255u;
        emissive.pixels[center + 2u] = 255u;
    }
    material.emissive = emissive;

    karma::renderer::MeshData::TextureData normal{};
    normal.width = 4;
    normal.height = 4;
    normal.channels = 3;
    normal.pixels.assign(4u * 4u * 3u, 0u);
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            const std::size_t base = ((y * 4u) + x) * 3u;
            const bool high_cell = (((x + y) & 1u) == 0u);
            normal.pixels[base + 0u] = high_cell ? 255u : 128u;
            normal.pixels[base + 1u] = high_cell ? 0u : 128u;
            normal.pixels[base + 2u] = 255u;
        }
    }
    material.normal = normal;

    karma::renderer::MeshData::TextureData occlusion{};
    occlusion.width = 4;
    occlusion.height = 4;
    occlusion.channels = 1;
    occlusion.pixels.assign(4u * 4u, 255u);
    occlusion.pixels[(2u * 4u) + 2u] = 0u;
    material.occlusion = occlusion;

    karma::renderer::MeshData::TextureData albedo{};
    albedo.width = 1;
    albedo.height = 1;
    albedo.channels = 2;
    albedo.pixels = {255u, 0u};
    material.albedo = albedo;

    const ResolvedMaterialSemantics semantics = ResolveMaterialSemantics(material);
    if (!ValidateResolvedMaterialSemantics(semantics)) {
        std::cerr << "resolved material semantics failed validation\n";
        return false;
    }
    if (!semantics.used_metallic_roughness_texture || !semantics.used_emissive_texture ||
        !semantics.used_normal_texture || !semantics.used_occlusion_texture) {
        std::cerr << "expected material texture usage flags to be set\n";
        return false;
    }
    if (semantics.draw) {
        std::cerr << "expected LA albedo alpha to suppress mask draw\n";
        return false;
    }
    if (!NearlyEqual(semantics.metallic, 0.16f, 1e-3f)) {
        std::cerr << "metallic representative sample mismatch, got " << semantics.metallic << "\n";
        return false;
    }
    if (!NearlyEqual(semantics.roughness, 0.18f, 1e-3f)) {
        std::cerr << "roughness representative sample mismatch, got " << semantics.roughness << "\n";
        return false;
    }
    if (!NearlyEqual(semantics.emissive.r, 0.4f, 1e-3f) ||
        !NearlyEqual(semantics.emissive.g, 0.2f, 1e-3f) ||
        !NearlyEqual(semantics.emissive.b, 0.1f, 1e-3f)) {
        std::cerr << "emissive representative sample mismatch\n";
        return false;
    }
    if (!(semantics.normal_variation > 0.05f && semantics.normal_variation < 0.6f)) {
        std::cerr << "normal variation out of expected bounded range, got " << semantics.normal_variation << "\n";
        return false;
    }
    if (!(semantics.occlusion > 0.90f && semantics.occlusion <= 1.0f)) {
        std::cerr << "occlusion value out of expected bounded range, got " << semantics.occlusion << "\n";
        return false;
    }

    const std::vector<uint8_t> expanded = ExpandTextureToRgba8(albedo);
    if (expanded.size() != 4u) {
        std::cerr << "expanded albedo texture should contain one RGBA texel\n";
        return false;
    }
    if (expanded[0] != 255u || expanded[1] != 255u || expanded[2] != 255u || expanded[3] != 0u) {
        std::cerr << "LA texture expansion mismatch\n";
        return false;
    }
    const std::vector<uint8_t> expanded_occlusion = ExpandTextureToRgba8(occlusion);
    if (expanded_occlusion.size() != (4u * 4u * 4u)) {
        std::cerr << "expanded occlusion texture should contain 4x4 RGBA texels\n";
        return false;
    }
    if (expanded_occlusion[0] != 255u || expanded_occlusion[1] != 255u ||
        expanded_occlusion[2] != 255u || expanded_occlusion[3] != 255u) {
        std::cerr << "single-channel texture expansion mismatch\n";
        return false;
    }
    return true;
}

bool RunMaterialTextureStabilityChecks() {
    auto make_checker = [](int width,
                           int height,
                           int channels,
                           uint8_t low,
                           uint8_t high,
                           bool phase_shift) {
        karma::renderer::MeshData::TextureData texture{};
        texture.width = width;
        texture.height = height;
        texture.channels = channels;
        texture.pixels.assign(static_cast<std::size_t>(width * height * channels), 0u);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool high_cell = (((x + y + (phase_shift ? 1 : 0)) & 1) == 0);
                const std::size_t base = static_cast<std::size_t>((y * width + x) * channels);
                if (channels == 1) {
                    const uint8_t value = high_cell ? high : low;
                    texture.pixels[base] = value;
                } else if (channels == 3) {
                    if (high_cell) {
                        texture.pixels[base + 0u] = 255u;
                        texture.pixels[base + 1u] = 0u;
                        texture.pixels[base + 2u] = 255u;
                    } else {
                        texture.pixels[base + 0u] = 128u;
                        texture.pixels[base + 1u] = 128u;
                        texture.pixels[base + 2u] = 255u;
                    }
                }
            }
        }
        return texture;
    };

    karma::renderer::MaterialDesc a{};
    a.normal_scale = 1.0f;
    a.occlusion_strength = 1.0f;
    a.normal = make_checker(16, 16, 3, 0u, 255u, false);
    a.occlusion = make_checker(16, 16, 1, 0u, 255u, false);

    karma::renderer::MaterialDesc b = a;
    b.normal = make_checker(16, 16, 3, 0u, 255u, true);
    b.occlusion = make_checker(16, 16, 1, 0u, 255u, true);

    const ResolvedMaterialSemantics sem_a = ResolveMaterialSemantics(a);
    const ResolvedMaterialSemantics sem_b = ResolveMaterialSemantics(b);
    if (!ValidateResolvedMaterialSemantics(sem_a) || !ValidateResolvedMaterialSemantics(sem_b)) {
        std::cerr << "high-frequency semantics validation failed\n";
        return false;
    }

    if (std::fabs(sem_a.normal_variation - sem_b.normal_variation) > 0.12f) {
        std::cerr << "high-frequency normal semantics unstable across phase shift: "
                  << sem_a.normal_variation << " vs " << sem_b.normal_variation << "\n";
        return false;
    }
    if (std::fabs(sem_a.occlusion - sem_b.occlusion) > 0.12f) {
        std::cerr << "high-frequency occlusion semantics unstable across phase shift: "
                  << sem_a.occlusion << " vs " << sem_b.occlusion << "\n";
        return false;
    }

    if (!(sem_a.normal_variation > 0.25f && sem_a.normal_variation < 0.65f)) {
        std::cerr << "expected checker normal variation to stay near balanced mean, got "
                  << sem_a.normal_variation << "\n";
        return false;
    }
    if (!(sem_a.occlusion > 0.35f && sem_a.occlusion < 0.65f)) {
        std::cerr << "expected checker occlusion to stay near balanced mean, got "
                  << sem_a.occlusion << "\n";
        return false;
    }
    return true;
}

bool RunMaterialTextureClampPolicyChecks() {
    karma::renderer::MaterialDesc clamped{};
    clamped.normal_scale = 100.0f;
    clamped.occlusion_strength = 5.0f;
    karma::renderer::MeshData::TextureData clamped_normal{};
    clamped_normal.width = 1;
    clamped_normal.height = 1;
    clamped_normal.channels = 3;
    clamped_normal.pixels = {255u, 0u, 255u};
    clamped.normal = clamped_normal;
    karma::renderer::MeshData::TextureData clamped_occlusion{};
    clamped_occlusion.width = 1;
    clamped_occlusion.height = 1;
    clamped_occlusion.channels = 1;
    clamped_occlusion.pixels = {0u};
    clamped.occlusion = clamped_occlusion;
    const ResolvedMaterialSemantics clamped_sem = ResolveMaterialSemantics(clamped);
    if (!ValidateResolvedMaterialSemantics(clamped_sem)) {
        std::cerr << "clamped semantics unexpectedly invalid\n";
        return false;
    }
    if (!NearlyEqual(clamped_sem.normal_variation, 1.0f, 1e-4f)) {
        std::cerr << "expected normal_scale clamp to cap variation at 1.0, got " << clamped_sem.normal_variation << "\n";
        return false;
    }
    if (!NearlyEqual(clamped_sem.occlusion, 0.0f, 1e-4f)) {
        std::cerr << "expected occlusion_strength clamp to allow full attenuation, got " << clamped_sem.occlusion << "\n";
        return false;
    }

    karma::renderer::MaterialDesc fallback{};
    fallback.normal_scale = -2.0f;
    fallback.occlusion_strength = -5.0f;
    fallback.normal = clamped.normal;
    fallback.occlusion = clamped.occlusion;
    const ResolvedMaterialSemantics fallback_sem = ResolveMaterialSemantics(fallback);
    if (!ValidateResolvedMaterialSemantics(fallback_sem)) {
        std::cerr << "fallback semantics unexpectedly invalid\n";
        return false;
    }
    if (!NearlyEqual(fallback_sem.normal_variation, 0.0f, 1e-4f)) {
        std::cerr << "expected negative normal_scale to clamp to 0.0, got " << fallback_sem.normal_variation << "\n";
        return false;
    }
    if (!NearlyEqual(fallback_sem.occlusion, 1.0f, 1e-4f)) {
        std::cerr << "expected negative occlusion_strength to clamp to 0.0 influence, got " << fallback_sem.occlusion << "\n";
        return false;
    }
    return true;
}

bool RunMaterialTextureLifecycleIngestionChecks() {
    karma::renderer::MaterialDesc material{};
    karma::renderer::MeshData::TextureData normal{};
    normal.width = 1600;
    normal.height = 900;
    normal.channels = 3;
    normal.pixels.assign(static_cast<std::size_t>(normal.width * normal.height * normal.channels), 0u);
    for (int y = 0; y < normal.height; ++y) {
        for (int x = 0; x < normal.width; ++x) {
            const std::size_t base = static_cast<std::size_t>((y * normal.width + x) * normal.channels);
            normal.pixels[base + 0u] = static_cast<uint8_t>((x * 255) / std::max(1, normal.width - 1));
            normal.pixels[base + 1u] = static_cast<uint8_t>((y * 255) / std::max(1, normal.height - 1));
            normal.pixels[base + 2u] = 255u;
        }
    }
    material.normal = normal;

    karma::renderer::MeshData::TextureData occlusion{};
    occlusion.width = 1536;
    occlusion.height = 1024;
    occlusion.channels = 4;
    occlusion.pixels.assign(static_cast<std::size_t>(occlusion.width * occlusion.height * occlusion.channels), 0u);
    for (int y = 0; y < occlusion.height; ++y) {
        for (int x = 0; x < occlusion.width; ++x) {
            const std::size_t base = static_cast<std::size_t>((y * occlusion.width + x) * occlusion.channels);
            const uint8_t ao = (((x + y) & 1) == 0) ? 255u : 32u;
            occlusion.pixels[base + 0u] = ao;
            occlusion.pixels[base + 1u] = 0u;
            occlusion.pixels[base + 2u] = 0u;
            occlusion.pixels[base + 3u] = 255u;
        }
    }
    material.occlusion = occlusion;

    const auto ingestion = IngestMaterialTextureSetForLifecycle(material);
    if (!ingestion.normal.texture || !ingestion.occlusion.texture) {
        std::cerr << "expected lifecycle ingestion to preserve usable normal/occlusion textures\n";
        return false;
    }
    if (!ingestion.normal.bounded || !ingestion.occlusion.bounded) {
        std::cerr << "expected oversized lifecycle textures to be bounded\n";
        return false;
    }
    if (ingestion.normal.texture->channels != 3 || ingestion.occlusion.texture->channels != 1) {
        std::cerr << "expected lifecycle ingestion to normalize normal->RGB and occlusion->R channel layouts\n";
        return false;
    }
    if (ingestion.normal.texture->width > kTextureLifecycleMaxDimension ||
        ingestion.normal.texture->height > kTextureLifecycleMaxDimension ||
        ingestion.occlusion.texture->width > kTextureLifecycleMaxDimension ||
        ingestion.occlusion.texture->height > kTextureLifecycleMaxDimension) {
        std::cerr << "lifecycle ingestion exceeded max texture dimension bounds\n";
        return false;
    }
    const std::size_t normal_texels = static_cast<std::size_t>(ingestion.normal.texture->width) *
                                      static_cast<std::size_t>(ingestion.normal.texture->height);
    const std::size_t occlusion_texels = static_cast<std::size_t>(ingestion.occlusion.texture->width) *
                                         static_cast<std::size_t>(ingestion.occlusion.texture->height);
    if (normal_texels > kTextureLifecycleMaxTexels || occlusion_texels > kTextureLifecycleMaxTexels) {
        std::cerr << "lifecycle ingestion exceeded max texture texel budget\n";
        return false;
    }
    if (!IsUsableTexture(*ingestion.normal.texture) || !IsUsableTexture(*ingestion.occlusion.texture)) {
        std::cerr << "lifecycle ingestion produced unusable normalized texture payloads\n";
        return false;
    }

    const ResolvedMaterialSemantics semantics = ResolveMaterialSemantics(material);
    if (!ValidateResolvedMaterialSemantics(semantics)) {
        std::cerr << "lifecycle-ingested semantics failed validation\n";
        return false;
    }
    if (!semantics.used_normal_texture || !semantics.used_occlusion_texture) {
        std::cerr << "expected lifecycle semantics to preserve normal/occlusion usage flags\n";
        return false;
    }

    karma::renderer::MeshData::TextureData checker_1c{};
    checker_1c.width = 8;
    checker_1c.height = 8;
    checker_1c.channels = 1;
    checker_1c.pixels.assign(8u * 8u, 0u);
    for (std::size_t y = 0; y < 8u; ++y) {
        for (std::size_t x = 0; x < 8u; ++x) {
            checker_1c.pixels[(y * 8u) + x] = (((x + y) & 1u) == 0u) ? 255u : 0u;
        }
    }
    karma::renderer::MeshData::TextureData checker_4c{};
    checker_4c.width = 8;
    checker_4c.height = 8;
    checker_4c.channels = 4;
    checker_4c.pixels.assign(8u * 8u * 4u, 0u);
    for (std::size_t y = 0; y < 8u; ++y) {
        for (std::size_t x = 0; x < 8u; ++x) {
            const std::size_t dst = ((y * 8u) + x) * 4u;
            checker_4c.pixels[dst + 0u] = checker_1c.pixels[(y * 8u) + x];
            checker_4c.pixels[dst + 1u] = 0u;
            checker_4c.pixels[dst + 2u] = 0u;
            checker_4c.pixels[dst + 3u] = 255u;
        }
    }
    const auto checker_1c_ingestion =
        IngestMaterialTextureLifecycle(checker_1c, MaterialTextureSemantic::Occlusion);
    const auto checker_4c_ingestion =
        IngestMaterialTextureLifecycle(checker_4c, MaterialTextureSemantic::Occlusion);
    if (!checker_1c_ingestion.texture || !checker_4c_ingestion.texture) {
        std::cerr << "checker occlusion lifecycle ingestion unexpectedly failed\n";
        return false;
    }
    if (checker_1c_ingestion.texture->pixels != checker_4c_ingestion.texture->pixels) {
        std::cerr << "expected occlusion lifecycle ingestion to preserve channel-layout parity\n";
        return false;
    }
    return true;
}

bool RunShaderPathLifecycleConsumptionChecks() {
    karma::renderer::MaterialDesc material{};
    karma::renderer::MeshData::TextureData albedo{};
    albedo.width = 4;
    albedo.height = 4;
    albedo.channels = 4;
    albedo.pixels.assign(4u * 4u * 4u, 0u);
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            const std::size_t dst = ((y * 4u) + x) * 4u;
            albedo.pixels[dst + 0u] = 200u;
            albedo.pixels[dst + 1u] = 180u;
            albedo.pixels[dst + 2u] = 160u;
            albedo.pixels[dst + 3u] = 255u;
        }
    }
    material.albedo = albedo;

    karma::renderer::MeshData::TextureData normal{};
    normal.width = 4;
    normal.height = 4;
    normal.channels = 3;
    normal.pixels.assign(4u * 4u * 3u, 0u);
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            const std::size_t dst = ((y * 4u) + x) * 3u;
            const bool strong = (x >= 2u);
            normal.pixels[dst + 0u] = strong ? 255u : 128u;
            normal.pixels[dst + 1u] = strong ? 0u : 128u;
            normal.pixels[dst + 2u] = 255u;
        }
    }
    material.normal = normal;

    karma::renderer::MeshData::TextureData occlusion_1c{};
    occlusion_1c.width = 4;
    occlusion_1c.height = 4;
    occlusion_1c.channels = 1;
    occlusion_1c.pixels.assign(4u * 4u, 0u);
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            occlusion_1c.pixels[(y * 4u) + x] = (y < 2u) ? 255u : 64u;
        }
    }
    material.occlusion = occlusion_1c;

    const auto lifecycle_ingestion = IngestMaterialTextureSetForLifecycle(material);
    const auto direct_contract = ResolveMaterialShaderInputContract(
        material, lifecycle_ingestion, true);
    if (direct_contract.path != MaterialShaderTextureInputPath::DirectMultiSampler) {
        std::cerr << "expected direct multi-sampler path when capability is available\n";
        return false;
    }
    if (!direct_contract.used_albedo_texture ||
        !direct_contract.used_normal_lifecycle_texture ||
        !direct_contract.used_occlusion_lifecycle_texture) {
        std::cerr << "direct path did not report expected consumed texture sources\n";
        return false;
    }
    if (!direct_contract.direct_normal_texture || !direct_contract.direct_occlusion_texture) {
        std::cerr << "direct path missing expected normal/occlusion textures\n";
        return false;
    }

    const auto* direct_albedo = direct_contract.direct_albedo_texture
        ? &(*direct_contract.direct_albedo_texture)
        : nullptr;
    const auto* direct_normal = direct_contract.direct_normal_texture
        ? &(*direct_contract.direct_normal_texture)
        : nullptr;
    const auto* direct_occlusion = direct_contract.direct_occlusion_texture
        ? &(*direct_contract.direct_occlusion_texture)
        : nullptr;
    const uint8_t top_flat = ToU8(
        ResolveShaderPathDirectSample(direct_albedo, direct_normal, direct_occlusion, 0u, 0u, 4u, 4u).r);
    const uint8_t top_strong = ToU8(
        ResolveShaderPathDirectSample(direct_albedo, direct_normal, direct_occlusion, 3u, 0u, 4u, 4u).r);
    const uint8_t bottom_flat = ToU8(
        ResolveShaderPathDirectSample(direct_albedo, direct_normal, direct_occlusion, 0u, 3u, 4u, 4u).r);
    if (!(top_strong > top_flat)) {
        std::cerr << "expected direct normal sampler path to brighten stronger normal response\n";
        return false;
    }
    if (!(bottom_flat < top_flat)) {
        std::cerr << "expected direct occlusion sampler path to darken occluded areas\n";
        return false;
    }

    const auto fallback_contract = ResolveMaterialShaderInputContract(
        material, lifecycle_ingestion, false);
    if (fallback_contract.path != MaterialShaderTextureInputPath::CompositeFallback ||
        !fallback_contract.fallback_composite_texture) {
        std::cerr << "expected deterministic composite fallback when direct path is unavailable\n";
        return false;
    }
    if (!ValidateCompositeMatchesDirectPath(
            *fallback_contract.fallback_composite_texture,
            direct_albedo,
            direct_normal,
            direct_occlusion,
            1,
            "base direct/fallback parity")) {
        return false;
    }
    const auto fallback_contract_repeat = ResolveMaterialShaderInputContract(
        material, lifecycle_ingestion, false);
    if (fallback_contract_repeat.path != fallback_contract.path ||
        !fallback_contract_repeat.fallback_composite_texture ||
        fallback_contract_repeat.fallback_composite_texture->pixels !=
            fallback_contract.fallback_composite_texture->pixels) {
        std::cerr << "composite fallback path should be deterministic for identical material inputs\n";
        return false;
    }

    karma::renderer::MaterialDesc material_4c = material;
    karma::renderer::MeshData::TextureData occlusion_4c{};
    occlusion_4c.width = 4;
    occlusion_4c.height = 4;
    occlusion_4c.channels = 4;
    occlusion_4c.pixels.assign(4u * 4u * 4u, 0u);
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            const std::size_t dst = ((y * 4u) + x) * 4u;
            occlusion_4c.pixels[dst + 0u] = (y < 2u) ? 255u : 64u;
            occlusion_4c.pixels[dst + 1u] = 0u;
            occlusion_4c.pixels[dst + 2u] = 0u;
            occlusion_4c.pixels[dst + 3u] = 255u;
        }
    }
    material_4c.occlusion = occlusion_4c;
    const auto lifecycle_ingestion_4c = IngestMaterialTextureSetForLifecycle(material_4c);
    const auto direct_contract_4c =
        ResolveMaterialShaderInputContract(material_4c, lifecycle_ingestion_4c, true);
    const auto fallback_contract_4c =
        ResolveMaterialShaderInputContract(material_4c, lifecycle_ingestion_4c, false);
    if (direct_contract_4c.path != MaterialShaderTextureInputPath::DirectMultiSampler ||
        !direct_contract_4c.direct_occlusion_texture ||
        fallback_contract_4c.path != MaterialShaderTextureInputPath::CompositeFallback ||
        !fallback_contract_4c.fallback_composite_texture) {
        std::cerr << "failed to build shader-input contracts for RGBA occlusion variant\n";
        return false;
    }

    const auto* direct_albedo_4c = direct_contract_4c.direct_albedo_texture
        ? &(*direct_contract_4c.direct_albedo_texture)
        : nullptr;
    const auto* direct_normal_4c = direct_contract_4c.direct_normal_texture
        ? &(*direct_contract_4c.direct_normal_texture)
        : nullptr;
    const auto* direct_occlusion_4c = direct_contract_4c.direct_occlusion_texture
        ? &(*direct_contract_4c.direct_occlusion_texture)
        : nullptr;
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) {
            const glm::vec4 direct_1c =
                ResolveShaderPathDirectSample(direct_albedo, direct_normal, direct_occlusion, x, y, 4u, 4u);
            const glm::vec4 direct_4c =
                ResolveShaderPathDirectSample(direct_albedo_4c, direct_normal_4c, direct_occlusion_4c, x, y, 4u, 4u);
            if (std::abs(static_cast<int>(ToU8(direct_1c.r)) - static_cast<int>(ToU8(direct_4c.r))) > 1 ||
                std::abs(static_cast<int>(ToU8(direct_1c.g)) - static_cast<int>(ToU8(direct_4c.g))) > 1 ||
                std::abs(static_cast<int>(ToU8(direct_1c.b)) - static_cast<int>(ToU8(direct_4c.b))) > 1) {
                std::cerr << "expected direct sampler path parity across 1-channel and 4-channel occlusion inputs\n";
                return false;
            }
        }
    }
    if (!ValidateCompositeMatchesDirectPath(
            *fallback_contract_4c.fallback_composite_texture,
            direct_albedo_4c,
            direct_normal_4c,
            direct_occlusion_4c,
            1,
            "rgba occlusion direct/fallback parity")) {
        return false;
    }
    if (fallback_contract.fallback_composite_texture->pixels !=
        fallback_contract_4c.fallback_composite_texture->pixels) {
        std::cerr << "expected fallback composite parity across 1-channel and 4-channel occlusion inputs\n";
        return false;
    }

    karma::renderer::MaterialDesc missing_normal_material = material;
    missing_normal_material.normal.reset();
    const auto missing_normal_lifecycle = IngestMaterialTextureSetForLifecycle(missing_normal_material);
    const auto missing_normal_direct =
        ResolveMaterialShaderInputContract(missing_normal_material, missing_normal_lifecycle, true);
    const auto missing_normal_fallback =
        ResolveMaterialShaderInputContract(missing_normal_material, missing_normal_lifecycle, false);
    if (missing_normal_direct.path != MaterialShaderTextureInputPath::DirectMultiSampler ||
        missing_normal_direct.direct_normal_texture ||
        !missing_normal_direct.direct_occlusion_texture) {
        std::cerr << "missing-normal direct contract should consume only occlusion path inputs\n";
        return false;
    }
    if (missing_normal_fallback.path != MaterialShaderTextureInputPath::CompositeFallback ||
        !missing_normal_fallback.fallback_composite_texture) {
        std::cerr << "missing-normal fallback contract failed\n";
        return false;
    }
    if (!ValidateCompositeMatchesDirectPath(
            *missing_normal_fallback.fallback_composite_texture,
            missing_normal_direct.direct_albedo_texture ? &(*missing_normal_direct.direct_albedo_texture) : nullptr,
            nullptr,
            missing_normal_direct.direct_occlusion_texture ? &(*missing_normal_direct.direct_occlusion_texture) : nullptr,
            1,
            "missing-normal direct/fallback parity")) {
        return false;
    }

    karma::renderer::MaterialDesc missing_occlusion_material = material;
    missing_occlusion_material.occlusion.reset();
    const auto missing_occlusion_lifecycle = IngestMaterialTextureSetForLifecycle(missing_occlusion_material);
    const auto missing_occlusion_direct =
        ResolveMaterialShaderInputContract(missing_occlusion_material, missing_occlusion_lifecycle, true);
    const auto missing_occlusion_fallback =
        ResolveMaterialShaderInputContract(missing_occlusion_material, missing_occlusion_lifecycle, false);
    if (missing_occlusion_direct.path != MaterialShaderTextureInputPath::DirectMultiSampler ||
        missing_occlusion_direct.direct_occlusion_texture ||
        !missing_occlusion_direct.direct_normal_texture) {
        std::cerr << "missing-occlusion direct contract should consume only normal path inputs\n";
        return false;
    }
    if (missing_occlusion_fallback.path != MaterialShaderTextureInputPath::CompositeFallback ||
        !missing_occlusion_fallback.fallback_composite_texture) {
        std::cerr << "missing-occlusion fallback contract failed\n";
        return false;
    }
    if (!ValidateCompositeMatchesDirectPath(
            *missing_occlusion_fallback.fallback_composite_texture,
            missing_occlusion_direct.direct_albedo_texture ? &(*missing_occlusion_direct.direct_albedo_texture) : nullptr,
            missing_occlusion_direct.direct_normal_texture ? &(*missing_occlusion_direct.direct_normal_texture) : nullptr,
            nullptr,
            1,
            "missing-occlusion direct/fallback parity")) {
        return false;
    }

    karma::renderer::MaterialDesc oversized{};
    oversized.normal = material.normal;
    oversized.normal->width = 1600;
    oversized.normal->height = 1200;
    oversized.normal->channels = 3;
    oversized.normal->pixels.assign(static_cast<std::size_t>(1600 * 1200 * 3), 255u);
    oversized.occlusion = material.occlusion;
    oversized.occlusion->width = 2000;
    oversized.occlusion->height = 1500;
    oversized.occlusion->channels = 1;
    oversized.occlusion->pixels.assign(static_cast<std::size_t>(2000 * 1500), 255u);
    const auto oversized_lifecycle = IngestMaterialTextureSetForLifecycle(oversized);
    const auto oversized_shader_ingestion =
        BuildShaderPathTextureIngestion(oversized, oversized_lifecycle);
    const auto oversized_fallback_contract =
        ResolveMaterialShaderInputContract(oversized, oversized_lifecycle, false);
    if (!oversized_shader_ingestion.texture ||
        oversized_fallback_contract.path != MaterialShaderTextureInputPath::CompositeFallback ||
        !oversized_fallback_contract.fallback_composite_texture) {
        std::cerr << "expected shader-path ingestion to produce composite without albedo when lifecycle textures exist\n";
        return false;
    }
    if (oversized_shader_ingestion.texture->width > kTextureLifecycleMaxDimension ||
        oversized_shader_ingestion.texture->height > kTextureLifecycleMaxDimension) {
        std::cerr << "expected shader-path lifecycle composition to stay within bounded dimensions\n";
        return false;
    }
    if (!oversized_shader_ingestion.bounded) {
        std::cerr << "expected shader-path ingestion to report bounded lifecycle downscale for oversized inputs\n";
        return false;
    }
    if (oversized_fallback_contract.fallback_composite_texture->width > kTextureLifecycleMaxDimension ||
        oversized_fallback_contract.fallback_composite_texture->height > kTextureLifecycleMaxDimension ||
        !oversized_fallback_contract.bounded) {
        std::cerr << "expected bounded fallback contract to keep oversized lifecycle textures within limits\n";
        return false;
    }
    return true;
}

bool RunTextureMipChainChecks() {
    karma::renderer::MeshData::TextureData checker{};
    checker.width = 2;
    checker.height = 2;
    checker.channels = 4;
    checker.pixels = {
        0u,   0u,   0u,   255u,
        255u, 0u,   0u,   255u,
        0u,   255u, 0u,   255u,
        0u,   0u,   255u, 255u,
    };

    const auto checker_mips = BuildRgba8MipChain(checker);
    if (checker_mips.size() != ComputeRgba8MipLevelCount(checker.width, checker.height)) {
        std::cerr << "unexpected mip count for checker texture\n";
        return false;
    }
    if (checker_mips.size() != 2u ||
        checker_mips[0].width != 2 || checker_mips[0].height != 2 ||
        checker_mips[1].width != 1 || checker_mips[1].height != 1) {
        std::cerr << "unexpected checker mip dimensions\n";
        return false;
    }
    if (checker_mips[1].pixels.size() != 4u ||
        checker_mips[1].pixels[0] != 64u ||
        checker_mips[1].pixels[1] != 64u ||
        checker_mips[1].pixels[2] != 64u ||
        checker_mips[1].pixels[3] != 255u) {
        std::cerr << "unexpected checker mip averaging result\n";
        return false;
    }

    karma::renderer::MeshData::TextureData odd{};
    odd.width = 3;
    odd.height = 5;
    odd.channels = 4;
    odd.pixels.resize(static_cast<std::size_t>(odd.width) *
                      static_cast<std::size_t>(odd.height) * 4u);
    for (std::size_t i = 0; i < odd.pixels.size(); ++i) {
        odd.pixels[i] = static_cast<uint8_t>((i * 17u) % 251u);
    }

    const auto odd_mips = BuildRgba8MipChain(odd);
    if (odd_mips.size() != ComputeRgba8MipLevelCount(odd.width, odd.height)) {
        std::cerr << "unexpected mip count for odd-dimension texture\n";
        return false;
    }
    const std::array<std::pair<int, int>, 3u> expected_dims{{
        {3, 5},
        {1, 2},
        {1, 1},
    }};
    if (odd_mips.size() != expected_dims.size()) {
        std::cerr << "unexpected odd-dimension mip level count\n";
        return false;
    }
    for (std::size_t i = 0; i < odd_mips.size(); ++i) {
        if (odd_mips[i].width != expected_dims[i].first ||
            odd_mips[i].height != expected_dims[i].second) {
            std::cerr << "unexpected odd-dimension mip size at level " << i << "\n";
            return false;
        }
        const std::size_t expected_bytes =
            static_cast<std::size_t>(odd_mips[i].width) *
            static_cast<std::size_t>(odd_mips[i].height) * 4u;
        if (odd_mips[i].pixels.size() != expected_bytes) {
            std::cerr << "unexpected odd-dimension mip byte count at level " << i << "\n";
            return false;
        }
    }

    const auto odd_mips_repeat = BuildRgba8MipChain(odd);
    if (odd_mips_repeat.size() != odd_mips.size()) {
        std::cerr << "mip chain should be deterministic for identical texture input\n";
        return false;
    }
    for (std::size_t i = 0; i < odd_mips.size(); ++i) {
        if (odd_mips_repeat[i].width != odd_mips[i].width ||
            odd_mips_repeat[i].height != odd_mips[i].height ||
            odd_mips_repeat[i].pixels != odd_mips[i].pixels) {
            std::cerr << "mip chain determinism mismatch at level " << i << "\n";
            return false;
        }
    }

    karma::renderer::MeshData::TextureData invalid{};
    invalid.width = 0;
    invalid.height = 0;
    invalid.channels = 4;
    if (!BuildRgba8MipChain(invalid).empty()) {
        std::cerr << "invalid texture input should not produce mip chain\n";
        return false;
    }

    return true;
}

bool RunDirectSamplerObservabilityContractChecks() {
    std::array<SamplerVariableAvailability, kDiligentMaterialVariantCount> material_variants{};
    for (auto& variant : material_variants) {
        variant.srb_ready = true;
        variant.has_s_tex = true;
        variant.has_s_normal = true;
        variant.has_s_occlusion = true;
    }

    SamplerVariableAvailability line_partial{};
    line_partial.srb_ready = true;
    line_partial.has_s_tex = true;
    line_partial.has_s_normal = false;
    line_partial.has_s_occlusion = false;
    const auto ready_with_partial_line =
        EvaluateDiligentDirectSamplerContract(material_variants, line_partial);
    if (!ready_with_partial_line.ready_for_direct_path ||
        !ready_with_partial_line.material_pipeline_contract_ready ||
        ready_with_partial_line.line_pipeline_contract_ready ||
        ready_with_partial_line.reason != "ok_line_sampler_contract_unavailable") {
        std::cerr << "expected Diligent direct sampler readiness to remain enabled when only line sampler contract is partial\n";
        return false;
    }

    auto broken_material_variants = material_variants;
    broken_material_variants[2].has_s_normal = false;
    const auto disabled_material_contract =
        EvaluateDiligentDirectSamplerContract(broken_material_variants, line_partial);
    if (disabled_material_contract.ready_for_direct_path ||
        disabled_material_contract.material_pipeline_contract_ready ||
        disabled_material_contract.reason.find("variant2_missing_s_normal") == std::string::npos) {
        std::cerr << "expected Diligent direct sampler readiness to disable when material sampler contract is missing\n";
        return false;
    }

    const auto bgfx_source_present_alignment =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                true,   // source_exists
                true,   // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                true,   // binary_up_to_date
                true,   // source_absent_integrity_ready
                "source_present_integrity_not_required",
            });
    if (!bgfx_source_present_alignment.ready ||
        !bgfx_source_present_alignment.source_present_mode ||
        bgfx_source_present_alignment.source_absent_compat_mode ||
        bgfx_source_present_alignment.reason != "ok") {
        std::cerr << "expected BGFX source-present alignment policy to require strict fresh source/binary contract\n";
        return false;
    }

    const auto bgfx_source_present_stale =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                true,   // source_exists
                true,   // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                true,   // source_absent_integrity_ready
                "source_present_integrity_not_required",
            });
    if (bgfx_source_present_stale.ready ||
        bgfx_source_present_stale.reason != "binary_stale_vs_source") {
        std::cerr << "expected BGFX source-present alignment policy to disable stale binaries\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_pass =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
            });
    if (!bgfx_source_absent_integrity_pass.ready ||
        bgfx_source_absent_integrity_pass.reason != "ok_source_absent_binary_integrity") {
        std::cerr << "expected BGFX source-absent integrity policy to allow manifest/hash validated deployed binaries\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_parse_fail =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_invalid_token_form",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_parse_fail.ready ||
        bgfx_source_absent_integrity_parse_fail.reason !=
            "source_missing_and_integrity_manifest_invalid_token_form") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on manifest parse failure\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_duplicate_key =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_duplicate_key",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_duplicate_key.ready ||
        bgfx_source_absent_integrity_duplicate_key.reason !=
            "source_missing_and_integrity_manifest_duplicate_key") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on duplicate manifest keys\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_noncanonical_line_endings =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_noncanonical_line_endings",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_noncanonical_line_endings.ready ||
        bgfx_source_absent_integrity_noncanonical_line_endings.reason !=
            "source_missing_and_integrity_manifest_noncanonical_line_endings") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on noncanonical line endings\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_noncanonical_whitespace =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_noncanonical_whitespace",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_noncanonical_whitespace.ready ||
        bgfx_source_absent_integrity_noncanonical_whitespace.reason !=
            "source_missing_and_integrity_manifest_noncanonical_whitespace") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on noncanonical whitespace\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_unknown_key =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_unknown_key",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_unknown_key.ready ||
        bgfx_source_absent_integrity_unknown_key.reason !=
            "source_missing_and_integrity_manifest_unknown_key") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on unknown manifest keys\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_invalid_value_form =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_invalid_value_form",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_invalid_value_form.ready ||
        bgfx_source_absent_integrity_invalid_value_form.reason !=
            "source_missing_and_integrity_manifest_invalid_value_form") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on invalid manifest value forms\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_version_unsupported =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                false,  // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_version_unsupported.ready ||
        bgfx_source_absent_integrity_version_unsupported.reason !=
            "source_missing_and_integrity_manifest_version_unsupported") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on manifest version mismatch\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_algorithm_unsupported =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                false,  // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_algorithm_unsupported.ready ||
        bgfx_source_absent_integrity_algorithm_unsupported.reason !=
            "source_missing_and_integrity_manifest_algorithm_unsupported") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on unsupported manifest algorithm\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_missing_manifest =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                false,  // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_parse_failed",
                false,  // manifest_version_supported
                false,  // manifest_algorithm_supported
                false,  // manifest_hash_present
                false,  // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_missing_manifest.ready ||
        bgfx_source_absent_integrity_missing_manifest.reason !=
            "source_missing_and_integrity_manifest_missing") {
        std::cerr << "expected BGFX source-absent integrity policy to disable when manifest is missing\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_hash_mismatch =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                false,  // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_hash_mismatch.ready ||
        bgfx_source_absent_integrity_hash_mismatch.reason !=
            "source_missing_and_integrity_hash_mismatch") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on manifest/hash mismatch\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_parse_fail =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                false,  // manifest_parse_ready
                "source_missing_and_integrity_manifest_signed_envelope_metadata_missing",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
            });
    if (bgfx_source_absent_integrity_signed_envelope_parse_fail.ready ||
        bgfx_source_absent_integrity_signed_envelope_parse_fail.reason !=
            "source_missing_and_integrity_manifest_signed_envelope_metadata_missing") {
        std::cerr << "expected BGFX source-absent integrity policy to propagate signed-envelope parse guardrail reasons\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_deferred =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                false,  // signed_envelope_verification_available
            });
    if (bgfx_source_absent_integrity_signed_envelope_deferred.ready ||
        bgfx_source_absent_integrity_signed_envelope_deferred.reason !=
            "source_missing_and_integrity_signed_envelope_verification_deferred") {
        std::cerr << "expected BGFX source-absent integrity policy to defer signed-envelope verification deterministically\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_mode_unsupported =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                false,  // signed_envelope_mode_supported
            });
    if (bgfx_source_absent_integrity_signed_envelope_mode_unsupported.ready ||
        bgfx_source_absent_integrity_signed_envelope_mode_unsupported.reason !=
            "source_missing_and_integrity_signed_envelope_verification_mode_unsupported") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on signed-envelope mode mismatch\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_trust_chain_invalid =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                false,  // signed_envelope_trust_root_available
                false,  // signed_envelope_trust_chain_valid
            });
    if (bgfx_source_absent_integrity_signed_envelope_trust_chain_invalid.ready ||
        bgfx_source_absent_integrity_signed_envelope_trust_chain_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_trust_chain_material_invalid") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on signed-envelope trust-chain material mismatch\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_trust_root_missing =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                false,  // signed_envelope_trust_root_available
                true,   // signed_envelope_trust_chain_valid
            });
    if (bgfx_source_absent_integrity_signed_envelope_trust_root_missing.ready ||
        bgfx_source_absent_integrity_signed_envelope_trust_root_missing.reason !=
            "source_missing_and_integrity_signed_envelope_trust_root_missing") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on missing signed-envelope trust root\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_signature_material_invalid =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                true,   // signed_envelope_trust_root_available
                true,   // signed_envelope_trust_chain_valid
                false,  // signed_envelope_signature_material_valid
            });
    if (bgfx_source_absent_integrity_signed_envelope_signature_material_invalid.ready ||
        bgfx_source_absent_integrity_signed_envelope_signature_material_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_signature_material_invalid") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on invalid signed-envelope signature material\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_verification_inputs_invalid =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                true,   // signed_envelope_trust_root_available
                true,   // signed_envelope_trust_chain_valid
                true,   // signed_envelope_signature_material_valid
                true,   // signed_envelope_signature_verified
                true,   // signed_envelope_verification_inputs_checked
                false,  // signed_envelope_verification_inputs_valid
            });
    if (bgfx_source_absent_integrity_signed_envelope_verification_inputs_invalid.ready ||
        bgfx_source_absent_integrity_signed_envelope_verification_inputs_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_verification_inputs_invalid") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on invalid signed-envelope verification inputs\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_signature_verification_failed =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                true,   // signed_envelope_trust_root_available
                true,   // signed_envelope_trust_chain_valid
                true,   // signed_envelope_signature_material_valid
                false,  // signed_envelope_signature_verified
            });
    if (bgfx_source_absent_integrity_signed_envelope_signature_verification_failed.ready ||
        bgfx_source_absent_integrity_signed_envelope_signature_verification_failed.reason !=
            "source_missing_and_integrity_signed_envelope_signature_verification_failed") {
        std::cerr << "expected BGFX source-absent integrity policy to disable on signed-envelope signature verification failure\n";
        return false;
    }

    const auto bgfx_source_absent_integrity_signed_envelope_verified =
        karma::renderer_backend::detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            karma::renderer_backend::detail::BgfxSourceAbsentIntegrityInput{
                true,   // manifest_exists
                true,   // manifest_parse_ready
                "ok",
                true,   // manifest_version_supported
                true,   // manifest_algorithm_supported
                true,   // manifest_hash_present
                true,   // binary_hash_available
                true,   // hash_matches_manifest
                true,   // signed_envelope_declared
                true,   // signed_envelope_verification_available
                true,   // signed_envelope_mode_supported
                true,   // signed_envelope_trust_root_available
                true,   // signed_envelope_trust_chain_valid
                true,   // signed_envelope_signature_material_valid
                true,   // signed_envelope_signature_verified
            });
    if (!bgfx_source_absent_integrity_signed_envelope_verified.ready ||
        bgfx_source_absent_integrity_signed_envelope_verified.reason !=
            "ok_source_absent_signed_envelope_verified") {
        std::cerr << "expected BGFX source-absent integrity policy to enable when signed-envelope verification succeeds\n";
        return false;
    }

    const auto bgfx_source_absent_alignment =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_pass.ready,
                bgfx_source_absent_integrity_pass.reason,
            });
    if (!bgfx_source_absent_alignment.ready ||
        bgfx_source_absent_alignment.source_present_mode ||
        !bgfx_source_absent_alignment.source_absent_compat_mode ||
        bgfx_source_absent_alignment.reason != "ok_source_absent_binary_integrity") {
        std::cerr << "expected BGFX source-absent alignment compatibility to allow valid deployed binaries\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_integrity_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_hash_mismatch.ready,
                bgfx_source_absent_integrity_hash_mismatch.reason,
            });
    if (bgfx_source_absent_alignment_integrity_fail.ready ||
        bgfx_source_absent_alignment_integrity_fail.reason !=
            "source_missing_and_integrity_hash_mismatch") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate integrity failure reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_parse_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_parse_fail.ready,
                bgfx_source_absent_integrity_signed_envelope_parse_fail.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_parse_fail.ready ||
        bgfx_source_absent_alignment_signed_envelope_parse_fail.reason !=
            "source_missing_and_integrity_manifest_signed_envelope_metadata_missing") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate signed-envelope parse guardrail reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_deferred =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_deferred.ready,
                bgfx_source_absent_integrity_signed_envelope_deferred.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_deferred.ready ||
        bgfx_source_absent_alignment_signed_envelope_deferred.reason !=
            "source_missing_and_integrity_signed_envelope_verification_deferred") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate signed-envelope verification-deferred reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_mode_unsupported =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_mode_unsupported.ready,
                bgfx_source_absent_integrity_signed_envelope_mode_unsupported.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_mode_unsupported.ready ||
        bgfx_source_absent_alignment_signed_envelope_mode_unsupported.reason !=
            "source_missing_and_integrity_signed_envelope_verification_mode_unsupported") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate signed-envelope mode mismatch reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_trust_root_missing =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_trust_root_missing.ready,
                bgfx_source_absent_integrity_signed_envelope_trust_root_missing.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_trust_root_missing.ready ||
        bgfx_source_absent_alignment_signed_envelope_trust_root_missing.reason !=
            "source_missing_and_integrity_signed_envelope_trust_root_missing") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate missing trust-root reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_trust_chain_invalid =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_trust_chain_invalid.ready,
                bgfx_source_absent_integrity_signed_envelope_trust_chain_invalid.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_trust_chain_invalid.ready ||
        bgfx_source_absent_alignment_signed_envelope_trust_chain_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_trust_chain_material_invalid") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate invalid trust-chain-material reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_signature_material_invalid =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_signature_material_invalid.ready,
                bgfx_source_absent_integrity_signed_envelope_signature_material_invalid.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_signature_material_invalid.ready ||
        bgfx_source_absent_alignment_signed_envelope_signature_material_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_signature_material_invalid") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate invalid signature-material reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_verification_inputs_invalid =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_verification_inputs_invalid.ready,
                bgfx_source_absent_integrity_signed_envelope_verification_inputs_invalid.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_verification_inputs_invalid.ready ||
        bgfx_source_absent_alignment_signed_envelope_verification_inputs_invalid.reason !=
            "source_missing_and_integrity_signed_envelope_verification_inputs_invalid") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate signed-envelope verification-inputs reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_signature_verification_failed =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_signature_verification_failed.ready,
                bgfx_source_absent_integrity_signed_envelope_signature_verification_failed.reason,
            });
    if (bgfx_source_absent_alignment_signed_envelope_signature_verification_failed.ready ||
        bgfx_source_absent_alignment_signed_envelope_signature_verification_failed.reason !=
            "source_missing_and_integrity_signed_envelope_signature_verification_failed") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate signed-envelope verification-failure reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_signed_envelope_verified =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_signed_envelope_verified.ready,
                bgfx_source_absent_integrity_signed_envelope_verified.reason,
            });
    if (!bgfx_source_absent_alignment_signed_envelope_verified.ready ||
        bgfx_source_absent_alignment_signed_envelope_verified.reason !=
            "ok_source_absent_signed_envelope_verified") {
        std::cerr << "expected BGFX source-absent alignment policy to allow verified signed-envelope deployment path\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_duplicate_key_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_duplicate_key.ready,
                bgfx_source_absent_integrity_duplicate_key.reason,
            });
    if (bgfx_source_absent_alignment_duplicate_key_fail.ready ||
        bgfx_source_absent_alignment_duplicate_key_fail.reason !=
            "source_missing_and_integrity_manifest_duplicate_key") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate duplicate-key parse reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_noncanonical_line_endings_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_noncanonical_line_endings.ready,
                bgfx_source_absent_integrity_noncanonical_line_endings.reason,
            });
    if (bgfx_source_absent_alignment_noncanonical_line_endings_fail.ready ||
        bgfx_source_absent_alignment_noncanonical_line_endings_fail.reason !=
            "source_missing_and_integrity_manifest_noncanonical_line_endings") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate noncanonical line-ending reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_noncanonical_whitespace_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_noncanonical_whitespace.ready,
                bgfx_source_absent_integrity_noncanonical_whitespace.reason,
            });
    if (bgfx_source_absent_alignment_noncanonical_whitespace_fail.ready ||
        bgfx_source_absent_alignment_noncanonical_whitespace_fail.reason !=
            "source_missing_and_integrity_manifest_noncanonical_whitespace") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate noncanonical-whitespace reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_unknown_key_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_unknown_key.ready,
                bgfx_source_absent_integrity_unknown_key.reason,
            });
    if (bgfx_source_absent_alignment_unknown_key_fail.ready ||
        bgfx_source_absent_alignment_unknown_key_fail.reason !=
            "source_missing_and_integrity_manifest_unknown_key") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate unknown-key parse reason\n";
        return false;
    }

    const auto bgfx_source_absent_alignment_invalid_token_fail =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                true,   // binary_exists
                true,   // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_parse_fail.ready,
                bgfx_source_absent_integrity_parse_fail.reason,
            });
    if (bgfx_source_absent_alignment_invalid_token_fail.ready ||
        bgfx_source_absent_alignment_invalid_token_fail.reason !=
            "source_missing_and_integrity_manifest_invalid_token_form") {
        std::cerr << "expected BGFX source-absent alignment policy to propagate invalid-token parse reason\n";
        return false;
    }

    const auto bgfx_source_absent_missing_binary =
        karma::renderer_backend::detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            karma::renderer_backend::detail::BgfxDirectSamplerAlignmentInput{
                false,  // source_exists
                false,  // source_declares_direct_contract
                false,  // binary_exists
                false,  // binary_non_empty
                false,  // binary_up_to_date
                bgfx_source_absent_integrity_pass.ready,
                bgfx_source_absent_integrity_pass.reason,
            });
    if (bgfx_source_absent_missing_binary.ready ||
        bgfx_source_absent_missing_binary.reason != "source_missing_and_binary_missing") {
        std::cerr << "expected BGFX source-absent alignment policy to disable missing deployed binaries\n";
        return false;
    }

    const auto bgfx_ready_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_present_alignment);
    if (!bgfx_ready_contract.ready_for_direct_path ||
        !bgfx_ready_contract.uniform_contract_ready ||
        !bgfx_ready_contract.shader_alignment_ready ||
        bgfx_ready_contract.reason != "ok") {
        std::cerr << "expected BGFX direct sampler readiness to enable when uniform and alignment contracts are ready\n";
        return false;
    }

    const auto bgfx_uniform_unavailable = EvaluateBgfxDirectSamplerContract(
        false,  // uniform_contract_ready
        bgfx_source_present_alignment);
    if (bgfx_uniform_unavailable.ready_for_direct_path ||
        bgfx_uniform_unavailable.reason != "uniform_contract_unavailable") {
        std::cerr << "expected BGFX direct sampler readiness to disable when uniform contract is unavailable\n";
        return false;
    }

    const auto bgfx_alignment_unavailable = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_present_stale);
    if (bgfx_alignment_unavailable.ready_for_direct_path ||
        bgfx_alignment_unavailable.reason != "binary_stale_vs_source") {
        std::cerr << "expected BGFX direct sampler readiness to disable with strict source-present stale reason\n";
        return false;
    }

    const auto bgfx_source_absent_ready_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment);
    if (!bgfx_source_absent_ready_contract.ready_for_direct_path ||
        bgfx_source_absent_ready_contract.reason != "ok_source_absent_binary_integrity") {
        std::cerr << "expected BGFX direct sampler readiness to remain enabled for source-absent valid deployment path\n";
        return false;
    }

    const auto bgfx_source_absent_disabled_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment_integrity_fail);
    if (bgfx_source_absent_disabled_contract.ready_for_direct_path ||
        bgfx_source_absent_disabled_contract.reason != "source_missing_and_integrity_hash_mismatch") {
        std::cerr << "expected BGFX direct sampler readiness to propagate source-absent integrity disable reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_parse_fail_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment_signed_envelope_parse_fail);
    if (bgfx_source_absent_signed_envelope_parse_fail_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_parse_fail_contract.reason !=
            "source_missing_and_integrity_manifest_signed_envelope_metadata_missing") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope parse guardrail reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_deferred_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment_signed_envelope_deferred);
    if (bgfx_source_absent_signed_envelope_deferred_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_deferred_contract.reason !=
            "source_missing_and_integrity_signed_envelope_verification_deferred") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope verification-deferred reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_mode_unsupported_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_mode_unsupported);
    if (bgfx_source_absent_signed_envelope_mode_unsupported_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_mode_unsupported_contract.reason !=
            "source_missing_and_integrity_signed_envelope_verification_mode_unsupported") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope verification-mode mismatch reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_trust_root_missing_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_trust_root_missing);
    if (bgfx_source_absent_signed_envelope_trust_root_missing_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_trust_root_missing_contract.reason !=
            "source_missing_and_integrity_signed_envelope_trust_root_missing") {
        std::cerr << "expected BGFX direct sampler readiness to propagate missing signed-envelope trust-root reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_trust_chain_invalid_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_trust_chain_invalid);
    if (bgfx_source_absent_signed_envelope_trust_chain_invalid_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_trust_chain_invalid_contract.reason !=
            "source_missing_and_integrity_signed_envelope_trust_chain_material_invalid") {
        std::cerr << "expected BGFX direct sampler readiness to propagate invalid signed-envelope trust-chain-material reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_signature_material_invalid_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_signature_material_invalid);
    if (bgfx_source_absent_signed_envelope_signature_material_invalid_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_signature_material_invalid_contract.reason !=
            "source_missing_and_integrity_signed_envelope_signature_material_invalid") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope signature-material mismatch reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_verification_inputs_invalid_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_verification_inputs_invalid);
    if (bgfx_source_absent_signed_envelope_verification_inputs_invalid_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_verification_inputs_invalid_contract.reason !=
            "source_missing_and_integrity_signed_envelope_verification_inputs_invalid") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope verification-inputs reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_signature_verification_failed_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_signature_verification_failed);
    if (bgfx_source_absent_signed_envelope_signature_verification_failed_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_signature_verification_failed_contract.reason !=
            "source_missing_and_integrity_signed_envelope_signature_verification_failed") {
        std::cerr << "expected BGFX direct sampler readiness to propagate signed-envelope verification-failure reason\n";
        return false;
    }

    const auto bgfx_source_absent_signed_envelope_verified_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_signed_envelope_verified);
    if (!bgfx_source_absent_signed_envelope_verified_contract.ready_for_direct_path ||
        bgfx_source_absent_signed_envelope_verified_contract.reason !=
            "ok_source_absent_signed_envelope_verified") {
        std::cerr << "expected BGFX direct sampler readiness to allow verified signed-envelope deployment path\n";
        return false;
    }

    const auto bgfx_source_absent_duplicate_disabled_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment_duplicate_key_fail);
    if (bgfx_source_absent_duplicate_disabled_contract.ready_for_direct_path ||
        bgfx_source_absent_duplicate_disabled_contract.reason !=
            "source_missing_and_integrity_manifest_duplicate_key") {
        std::cerr << "expected BGFX direct sampler readiness to propagate duplicate-key parse disable reason\n";
        return false;
    }

    const auto bgfx_source_absent_noncanonical_line_endings_disabled_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_noncanonical_line_endings_fail);
    if (bgfx_source_absent_noncanonical_line_endings_disabled_contract.ready_for_direct_path ||
        bgfx_source_absent_noncanonical_line_endings_disabled_contract.reason !=
            "source_missing_and_integrity_manifest_noncanonical_line_endings") {
        std::cerr << "expected BGFX direct sampler readiness to propagate noncanonical line-ending disable reason\n";
        return false;
    }

    const auto bgfx_source_absent_noncanonical_whitespace_disabled_contract =
        EvaluateBgfxDirectSamplerContract(
            true,   // uniform_contract_ready
            bgfx_source_absent_alignment_noncanonical_whitespace_fail);
    if (bgfx_source_absent_noncanonical_whitespace_disabled_contract.ready_for_direct_path ||
        bgfx_source_absent_noncanonical_whitespace_disabled_contract.reason !=
            "source_missing_and_integrity_manifest_noncanonical_whitespace") {
        std::cerr << "expected BGFX direct sampler readiness to propagate noncanonical-whitespace disable reason\n";
        return false;
    }

    const auto bgfx_source_absent_unknown_key_disabled_contract = EvaluateBgfxDirectSamplerContract(
        true,   // uniform_contract_ready
        bgfx_source_absent_alignment_unknown_key_fail);
    if (bgfx_source_absent_unknown_key_disabled_contract.ready_for_direct_path ||
        bgfx_source_absent_unknown_key_disabled_contract.reason !=
            "source_missing_and_integrity_manifest_unknown_key") {
        std::cerr << "expected BGFX direct sampler readiness to propagate unknown-key parse disable reason\n";
        return false;
    }

    const auto enabled_invariants = EvaluateDirectSamplerDrawInvariants(
        DirectSamplerDrawInvariantInput{
            true,   // direct_path_enabled
            8u,     // total_draws
            3u,     // direct_contract_draws
            3u,     // direct_draws
            5u,     // fallback_draws
            0u,     // forced_fallback_draws
            0u,     // unexpected_direct_draws
        });
    if (!enabled_invariants.ok || enabled_invariants.reason != "ok") {
        std::cerr << "expected enabled direct sampler invariants to pass\n";
        return false;
    }

    const auto disabled_invariants = EvaluateDirectSamplerDrawInvariants(
        DirectSamplerDrawInvariantInput{
            false,  // direct_path_enabled
            8u,     // total_draws
            3u,     // direct_contract_draws
            0u,     // direct_draws
            8u,     // fallback_draws
            3u,     // forced_fallback_draws
            0u,     // unexpected_direct_draws
        });
    if (!disabled_invariants.ok || disabled_invariants.reason != "ok") {
        std::cerr << "expected disabled direct sampler invariants to pass\n";
        return false;
    }

    const auto forced_fallback_violation = EvaluateDirectSamplerDrawInvariants(
        DirectSamplerDrawInvariantInput{
            true,   // direct_path_enabled
            8u,     // total_draws
            3u,     // direct_contract_draws
            2u,     // direct_draws
            6u,     // fallback_draws
            1u,     // forced_fallback_draws
            0u,     // unexpected_direct_draws
        });
    if (forced_fallback_violation.ok ||
        forced_fallback_violation.reason.find("forced_fallback_while_enabled") == std::string::npos ||
        forced_fallback_violation.reason.find("direct_contract_draw_mismatch_enabled") == std::string::npos) {
        std::cerr << "expected enabled direct sampler invariant violations to be reported explicitly\n";
        return false;
    }

    const auto disabled_direct_violation = EvaluateDirectSamplerDrawInvariants(
        DirectSamplerDrawInvariantInput{
            false,  // direct_path_enabled
            4u,     // total_draws
            2u,     // direct_contract_draws
            1u,     // direct_draws
            3u,     // fallback_draws
            1u,     // forced_fallback_draws
            1u,     // unexpected_direct_draws
        });
    if (disabled_direct_violation.ok ||
        disabled_direct_violation.reason.find("direct_draws_while_disabled") == std::string::npos ||
        disabled_direct_violation.reason.find("unexpected_direct_draws_nonzero") == std::string::npos ||
        disabled_direct_violation.reason.find("forced_fallback_draw_mismatch_disabled") == std::string::npos) {
        std::cerr << "expected disabled direct sampler invariant violations to be reported explicitly\n";
        return false;
    }

    return true;
}

bool RunOcclusionEdgeCaseParityChecks() {
    auto resolve_occlusion = [](uint8_t value, int channels, float strength) -> ResolvedMaterialSemantics {
        karma::renderer::MaterialDesc material{};
        material.occlusion_strength = strength;
        karma::renderer::MeshData::TextureData texture{};
        texture.width = 1;
        texture.height = 1;
        texture.channels = channels;
        texture.pixels.assign(static_cast<std::size_t>(channels), 0u);
        texture.pixels[0] = value;
        if (channels > 1) {
            texture.pixels[channels - 1] = 255u;
        }
        material.occlusion = texture;
        return ResolveMaterialSemantics(material);
    };

    const ResolvedMaterialSemantics black_1c = resolve_occlusion(1u, 1, 1.0f);
    const ResolvedMaterialSemantics black_4c = resolve_occlusion(1u, 4, 1.0f);
    if (!ValidateResolvedMaterialSemantics(black_1c) || !ValidateResolvedMaterialSemantics(black_4c)) {
        std::cerr << "black-edge occlusion semantics invalid\n";
        return false;
    }
    if (!NearlyEqual(black_1c.occlusion, 0.0f, 1e-4f) || !NearlyEqual(black_4c.occlusion, 0.0f, 1e-4f)) {
        std::cerr << "expected near-black occlusion edge case to clamp to full attenuation\n";
        return false;
    }
    if (!NearlyEqual(black_1c.occlusion_edge, 0.0f, 1e-4f) ||
        !NearlyEqual(black_4c.occlusion_edge, 0.0f, 1e-4f)) {
        std::cerr << "expected single-sample black occlusion edge weight to remain zero\n";
        return false;
    }

    const ResolvedMaterialSemantics white_1c = resolve_occlusion(254u, 1, 1.0f);
    const ResolvedMaterialSemantics white_4c = resolve_occlusion(254u, 4, 1.0f);
    if (!ValidateResolvedMaterialSemantics(white_1c) || !ValidateResolvedMaterialSemantics(white_4c)) {
        std::cerr << "white-edge occlusion semantics invalid\n";
        return false;
    }
    if (!NearlyEqual(white_1c.occlusion, 1.0f, 1e-4f) || !NearlyEqual(white_4c.occlusion, 1.0f, 1e-4f)) {
        std::cerr << "expected near-white occlusion edge case to clamp to no attenuation\n";
        return false;
    }
    if (!NearlyEqual(white_1c.occlusion_edge, 0.0f, 1e-4f) ||
        !NearlyEqual(white_4c.occlusion_edge, 0.0f, 1e-4f)) {
        std::cerr << "expected single-sample white occlusion edge weight to remain zero\n";
        return false;
    }

    auto resolve_checker_occlusion = [](int channels) -> ResolvedMaterialSemantics {
        karma::renderer::MaterialDesc material{};
        material.occlusion_strength = 1.0f;
        karma::renderer::MeshData::TextureData texture{};
        texture.width = 8;
        texture.height = 8;
        texture.channels = channels;
        texture.pixels.assign(static_cast<std::size_t>(texture.width * texture.height * channels), 0u);
        for (int y = 0; y < texture.height; ++y) {
            for (int x = 0; x < texture.width; ++x) {
                const bool high = (((x + y) & 1) == 0);
                const std::size_t base = static_cast<std::size_t>((y * texture.width + x) * channels);
                texture.pixels[base] = high ? 255u : 0u;
                if (channels > 1) {
                    texture.pixels[base + static_cast<std::size_t>(channels - 1)] = 255u;
                }
            }
        }
        material.occlusion = texture;
        return ResolveMaterialSemantics(material);
    };

    const ResolvedMaterialSemantics checker_1c = resolve_checker_occlusion(1);
    const ResolvedMaterialSemantics checker_4c = resolve_checker_occlusion(4);
    if (!ValidateResolvedMaterialSemantics(checker_1c) || !ValidateResolvedMaterialSemantics(checker_4c)) {
        std::cerr << "checker occlusion semantics invalid\n";
        return false;
    }
    if (!(checker_1c.occlusion_edge > 0.20f) || !(checker_4c.occlusion_edge > 0.20f)) {
        std::cerr << "expected checker occlusion to produce non-trivial edge weight\n";
        return false;
    }
    if (std::fabs(checker_1c.occlusion - checker_4c.occlusion) > 0.06f ||
        std::fabs(checker_1c.occlusion_edge - checker_4c.occlusion_edge) > 0.06f) {
        std::cerr << "expected checker occlusion semantics to stay channel-layout stable\n";
        return false;
    }

    const ResolvedMaterialSemantics zero_strength = resolve_occlusion(0u, 1, 0.0f);
    if (!ValidateResolvedMaterialSemantics(zero_strength) || !NearlyEqual(zero_strength.occlusion, 1.0f, 1e-4f)) {
        std::cerr << "expected zero occlusion strength to preserve full ambient\n";
        return false;
    }

    karma::renderer::MaterialDesc no_occlusion{};
    const ResolvedMaterialSemantics no_occlusion_sem = ResolveMaterialSemantics(no_occlusion);
    if (!ValidateResolvedMaterialSemantics(no_occlusion_sem)) {
        std::cerr << "no-occlusion semantics invalid\n";
        return false;
    }
    if (no_occlusion_sem.used_occlusion_texture || !NearlyEqual(no_occlusion_sem.occlusion, 1.0f, 1e-4f) ||
        !NearlyEqual(no_occlusion_sem.occlusion_edge, 0.0f, 1e-4f)) {
        std::cerr << "expected no-occlusion path to keep default occlusion=1.0\n";
        return false;
    }
    return true;
}

bool RunNormalDetailPolicyRefinementChecks() {
    karma::renderer::MeshData::TextureData strong_normal{};
    strong_normal.width = 1;
    strong_normal.height = 1;
    strong_normal.channels = 3;
    strong_normal.pixels = {255u, 0u, 255u};

    karma::renderer::MeshData::TextureData flat_normal{};
    flat_normal.width = 1;
    flat_normal.height = 1;
    flat_normal.channels = 3;
    flat_normal.pixels = {128u, 128u, 255u};

    karma::renderer::MaterialDesc flat_material{};
    flat_material.normal_scale = 1.0f;
    flat_material.normal = flat_normal;
    const ResolvedMaterialSemantics flat_semantics = ResolveMaterialSemantics(flat_material);
    if (!ValidateResolvedMaterialSemantics(flat_semantics)) {
        std::cerr << "flat-normal semantics invalid\n";
        return false;
    }
    if (!(flat_semantics.normal_variation < 0.02f)) {
        std::cerr << "expected flat normal map to stay near zero detail variation, got "
                  << flat_semantics.normal_variation << "\n";
        return false;
    }

    karma::renderer::MaterialDesc smooth_base{};
    smooth_base.base_color = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    smooth_base.metallic_factor = 0.2f;
    smooth_base.roughness_factor = 0.1f;
    karma::renderer::MaterialDesc smooth_detail = smooth_base;
    smooth_detail.normal_scale = 1.0f;
    smooth_detail.normal = strong_normal;

    karma::renderer::MaterialDesc rough_base = smooth_base;
    rough_base.roughness_factor = 0.9f;
    karma::renderer::MaterialDesc rough_detail = rough_base;
    rough_detail.normal_scale = 1.0f;
    rough_detail.normal = strong_normal;

    const ResolvedMaterialSemantics smooth_base_sem = ResolveMaterialSemantics(smooth_base);
    const ResolvedMaterialSemantics smooth_detail_sem = ResolveMaterialSemantics(smooth_detail);
    const ResolvedMaterialSemantics rough_base_sem = ResolveMaterialSemantics(rough_base);
    const ResolvedMaterialSemantics rough_detail_sem = ResolveMaterialSemantics(rough_detail);
    if (!ValidateResolvedMaterialSemantics(smooth_base_sem) ||
        !ValidateResolvedMaterialSemantics(smooth_detail_sem) ||
        !ValidateResolvedMaterialSemantics(rough_base_sem) ||
        !ValidateResolvedMaterialSemantics(rough_detail_sem)) {
        std::cerr << "normal-detail refinement semantics invalid\n";
        return false;
    }

    karma::renderer::DirectionalLightData light{};
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.color = glm::vec4(1.0f);
    light.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);

    karma::renderer::EnvironmentLightingData environment{};
    const ResolvedEnvironmentLightingSemantics env_sem =
        ResolveEnvironmentLightingSemantics(environment);

    const ResolvedMaterialLighting smooth_base_light =
        ResolveMaterialLighting(smooth_base_sem, light, env_sem, 1.0f);
    const ResolvedMaterialLighting smooth_detail_light =
        ResolveMaterialLighting(smooth_detail_sem, light, env_sem, 1.0f);
    const ResolvedMaterialLighting rough_base_light =
        ResolveMaterialLighting(rough_base_sem, light, env_sem, 1.0f);
    const ResolvedMaterialLighting rough_detail_light =
        ResolveMaterialLighting(rough_detail_sem, light, env_sem, 1.0f);

    if (!ValidateResolvedMaterialLighting(smooth_base_light) ||
        !ValidateResolvedMaterialLighting(smooth_detail_light) ||
        !ValidateResolvedMaterialLighting(rough_base_light) ||
        !ValidateResolvedMaterialLighting(rough_detail_light)) {
        std::cerr << "normal-detail refinement lighting invalid\n";
        return false;
    }

    const float smooth_boost = smooth_detail_light.light_color.r - smooth_base_light.light_color.r;
    const float rough_boost = rough_detail_light.light_color.r - rough_base_light.light_color.r;
    if (!(smooth_boost > rough_boost)) {
        std::cerr << "expected normal/detail boost to favor smoother surfaces (smooth="
                  << smooth_boost << ", rough=" << rough_boost << ")\n";
        return false;
    }
    return true;
}

bool RunNormalDetailResponseAndOcclusionIntegrationChecks() {
    karma::renderer::MeshData::TextureData strong_normal{};
    strong_normal.width = 1;
    strong_normal.height = 1;
    strong_normal.channels = 3;
    strong_normal.pixels = {255u, 0u, 255u};

    karma::renderer::DirectionalLightData light{};
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.color = glm::vec4(1.0f);
    light.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);

    karma::renderer::EnvironmentLightingData environment{};
    const ResolvedEnvironmentLightingSemantics env_sem = ResolveEnvironmentLightingSemantics(environment);

    karma::renderer::MaterialDesc base{};
    base.base_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    base.metallic_factor = 0.2f;
    base.roughness_factor = 0.25f;

    const ResolvedMaterialSemantics base_sem = ResolveMaterialSemantics(base);
    const ResolvedMaterialLighting base_light = ResolveMaterialLighting(base_sem, light, env_sem, 1.0f);
    if (!ValidateResolvedMaterialSemantics(base_sem) || !ValidateResolvedMaterialLighting(base_light)) {
        std::cerr << "baseline material response invalid\n";
        return false;
    }

    auto resolve_detail = [&](float normal_scale) -> std::pair<ResolvedMaterialSemantics, ResolvedMaterialLighting> {
        karma::renderer::MaterialDesc detail = base;
        detail.normal_scale = normal_scale;
        detail.normal = strong_normal;
        const ResolvedMaterialSemantics sem = ResolveMaterialSemantics(detail);
        const ResolvedMaterialLighting lit = ResolveMaterialLighting(sem, light, env_sem, 1.0f);
        return {sem, lit};
    };

    const auto [low_sem, low_light] = resolve_detail(0.25f);
    const auto [mid_sem, mid_light] = resolve_detail(0.60f);
    const auto [high_sem, high_light] = resolve_detail(1.0f);
    if (!ValidateResolvedMaterialSemantics(low_sem) || !ValidateResolvedMaterialLighting(low_light) ||
        !ValidateResolvedMaterialSemantics(mid_sem) || !ValidateResolvedMaterialLighting(mid_light) ||
        !ValidateResolvedMaterialSemantics(high_sem) || !ValidateResolvedMaterialLighting(high_light)) {
        std::cerr << "normal-detail response tuning outputs invalid\n";
        return false;
    }

    if (!(low_sem.normal_variation < mid_sem.normal_variation && mid_sem.normal_variation < high_sem.normal_variation)) {
        std::cerr << "expected normal variation to increase with normal scale\n";
        return false;
    }

    const float low_boost = low_light.light_color.r - base_light.light_color.r;
    const float mid_boost = mid_light.light_color.r - base_light.light_color.r;
    const float high_boost = high_light.light_color.r - base_light.light_color.r;
    if (!(low_boost > 0.0f && mid_boost > low_boost && high_boost > mid_boost)) {
        std::cerr << "expected bounded monotonic normal/detail direct-light boosts, got "
                  << low_boost << ", " << mid_boost << ", " << high_boost << "\n";
        return false;
    }
    if (!(high_boost < 0.35f)) {
        std::cerr << "expected high-end normal/detail boost to stay bounded, got " << high_boost << "\n";
        return false;
    }

    karma::renderer::MaterialDesc checker_occlusion = base;
    checker_occlusion.occlusion_strength = 1.0f;
    karma::renderer::MeshData::TextureData occlusion_checker{};
    occlusion_checker.width = 8;
    occlusion_checker.height = 8;
    occlusion_checker.channels = 1;
    occlusion_checker.pixels.assign(8u * 8u, 0u);
    for (std::size_t y = 0; y < 8u; ++y) {
        for (std::size_t x = 0; x < 8u; ++x) {
            occlusion_checker.pixels[(y * 8u) + x] = (((x + y) & 1u) == 0u) ? 255u : 0u;
        }
    }
    checker_occlusion.occlusion = occlusion_checker;

    const ResolvedMaterialSemantics checker_sem = ResolveMaterialSemantics(checker_occlusion);
    if (!ValidateResolvedMaterialSemantics(checker_sem)) {
        std::cerr << "checker occlusion integration semantics invalid\n";
        return false;
    }
    if (!(checker_sem.occlusion_edge > 0.20f)) {
        std::cerr << "expected checker occlusion integration path to capture edge weight\n";
        return false;
    }

    const ResolvedMaterialLighting checker_edge_light = ResolveMaterialLighting(checker_sem, light, env_sem, 1.0f);
    ResolvedMaterialSemantics checker_no_edge_sem = checker_sem;
    checker_no_edge_sem.occlusion_edge = 0.0f;
    const ResolvedMaterialLighting checker_no_edge_light =
        ResolveMaterialLighting(checker_no_edge_sem, light, env_sem, 1.0f);
    if (!ValidateResolvedMaterialLighting(checker_edge_light) ||
        !ValidateResolvedMaterialLighting(checker_no_edge_light)) {
        std::cerr << "checker occlusion integration lighting invalid\n";
        return false;
    }

    const float edge_lift = checker_edge_light.ambient_color.r - checker_no_edge_light.ambient_color.r;
    if (!(edge_lift > 1e-4f && edge_lift < 0.08f)) {
        std::cerr << "expected occlusion edge integration lift to be bounded, got " << edge_lift << "\n";
        return false;
    }
    if (!(checker_edge_light.ambient_color.r < base_light.ambient_color.r)) {
        std::cerr << "expected occluded material to remain below unoccluded ambient response\n";
        return false;
    }

    return true;
}

bool RunMaterialLightingChecks() {
    karma::renderer::MaterialDesc smooth_material{};
    smooth_material.base_color = glm::vec4(0.8f, 0.7f, 0.6f, 1.0f);
    smooth_material.metallic_factor = 0.3f;
    smooth_material.roughness_factor = 0.1f;
    smooth_material.emissive_color = glm::vec3(0.1f, 0.0f, 0.0f);

    karma::renderer::MaterialDesc rough_material = smooth_material;
    rough_material.roughness_factor = 0.9f;

    const ResolvedMaterialSemantics smooth_semantics = ResolveMaterialSemantics(smooth_material);
    const ResolvedMaterialSemantics rough_semantics = ResolveMaterialSemantics(rough_material);
    if (!ValidateResolvedMaterialSemantics(smooth_semantics) ||
        !ValidateResolvedMaterialSemantics(rough_semantics)) {
        std::cerr << "material semantics invalid in lighting checks\n";
        return false;
    }

    karma::renderer::DirectionalLightData light{};
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.color = glm::vec4(1.0f);
    light.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    light.unlit = 0.0f;

    karma::renderer::EnvironmentLightingData environment{};
    environment.enabled = true;
    const ResolvedEnvironmentLightingSemantics environment_semantics =
        ResolveEnvironmentLightingSemantics(environment);

    const ResolvedMaterialLighting smooth_lighting =
        ResolveMaterialLighting(smooth_semantics, light, environment_semantics, 1.0f);
    const ResolvedMaterialLighting rough_lighting =
        ResolveMaterialLighting(rough_semantics, light, environment_semantics, 1.0f);
    if (!ValidateResolvedMaterialLighting(smooth_lighting) ||
        !ValidateResolvedMaterialLighting(rough_lighting)) {
        std::cerr << "material lighting validation failed\n";
        return false;
    }
    if (!(smooth_lighting.light_color.r > rough_lighting.light_color.r)) {
        std::cerr << "expected smoother material to receive stronger direct light\n";
        return false;
    }

    karma::renderer::MaterialDesc occluded_material = smooth_material;
    occluded_material.occlusion_strength = 1.0f;
    karma::renderer::MeshData::TextureData occlusion{};
    occlusion.width = 1;
    occlusion.height = 1;
    occlusion.channels = 1;
    occlusion.pixels = {0u};
    occluded_material.occlusion = occlusion;

    karma::renderer::MaterialDesc normal_material = smooth_material;
    normal_material.normal_scale = 1.0f;
    karma::renderer::MeshData::TextureData normal{};
    normal.width = 1;
    normal.height = 1;
    normal.channels = 3;
    normal.pixels = {255u, 0u, 255u};
    normal_material.normal = normal;

    const ResolvedMaterialSemantics occluded_semantics = ResolveMaterialSemantics(occluded_material);
    const ResolvedMaterialSemantics normal_semantics = ResolveMaterialSemantics(normal_material);
    if (!ValidateResolvedMaterialSemantics(occluded_semantics) ||
        !ValidateResolvedMaterialSemantics(normal_semantics)) {
        std::cerr << "material semantics invalid for normal/occlusion lighting checks\n";
        return false;
    }

    const ResolvedMaterialLighting occluded_lighting =
        ResolveMaterialLighting(occluded_semantics, light, environment_semantics, 1.0f);
    const ResolvedMaterialLighting normal_lighting =
        ResolveMaterialLighting(normal_semantics, light, environment_semantics, 1.0f);
    if (!ValidateResolvedMaterialLighting(occluded_lighting) ||
        !ValidateResolvedMaterialLighting(normal_lighting)) {
        std::cerr << "material lighting invalid for normal/occlusion lighting checks\n";
        return false;
    }
    if (!(occluded_lighting.ambient_color.r < smooth_lighting.ambient_color.r)) {
        std::cerr << "expected occlusion texture semantics to reduce ambient contribution\n";
        return false;
    }
    if (!(normal_lighting.light_color.r > smooth_lighting.light_color.r)) {
        std::cerr << "expected normal texture semantics to increase direct-light detail gain\n";
        return false;
    }

    const ResolvedMaterialLighting fully_shadowed =
        ResolveMaterialLighting(smooth_semantics, light, environment_semantics, 0.0f);
    if (!NearlyEqual(fully_shadowed.light_color.r, 0.0f, 1e-5f) ||
        !NearlyEqual(fully_shadowed.light_color.g, 0.0f, 1e-5f) ||
        !NearlyEqual(fully_shadowed.light_color.b, 0.0f, 1e-5f)) {
        std::cerr << "expected zero shadow factor to suppress direct light contribution\n";
        return false;
    }
    return true;
}

bool RunDebugLineSemanticsChecks() {
    karma::renderer::DebugLineItem valid{};
    valid.start = glm::vec3(1.0f, 2.0f, 3.0f);
    valid.end = glm::vec3(4.0f, 2.0f, 3.0f);
    valid.color = glm::vec4(2.0f, -1.0f, 0.5f, std::numeric_limits<float>::quiet_NaN());
    valid.layer = 17u;

    const ResolvedDebugLineSemantics resolved = ResolveDebugLineSemantics(valid);
    if (!resolved.draw || !ValidateResolvedDebugLineSemantics(resolved)) {
        std::cerr << "resolved debug line should be drawable and valid\n";
        return false;
    }
    if (resolved.layer != 17u) {
        std::cerr << "debug line layer was not preserved\n";
        return false;
    }
    if (!NearlyEqual(resolved.color.r, 1.0f) ||
        !NearlyEqual(resolved.color.g, 0.0f) ||
        !NearlyEqual(resolved.color.b, 0.5f) ||
        !NearlyEqual(resolved.color.a, 1.0f)) {
        std::cerr << "debug line color clamp/fallback mismatch\n";
        return false;
    }

    karma::renderer::DebugLineItem degenerate{};
    degenerate.start = glm::vec3(1.0f, 1.0f, 1.0f);
    degenerate.end = degenerate.start;
    const ResolvedDebugLineSemantics degenerate_resolved = ResolveDebugLineSemantics(degenerate);
    if (degenerate_resolved.draw) {
        std::cerr << "degenerate debug line should be suppressed\n";
        return false;
    }

    return true;
}

} // namespace

int main() {
    if (!RunShadowSemanticsClampChecks()) {
        return 1;
    }
    if (!RunShadowMapBuildAndSampleChecks()) {
        return 1;
    }
    if (!RunShadowReceiverVisibilityChecks()) {
        return 1;
    }
    if (!RunEnvironmentSemanticsChecks()) {
        return 1;
    }
    if (!RunMaterialTextureSemanticsChecks()) {
        return 1;
    }
    if (!RunMaterialTextureStabilityChecks()) {
        return 1;
    }
    if (!RunMaterialTextureClampPolicyChecks()) {
        return 1;
    }
    if (!RunMaterialTextureLifecycleIngestionChecks()) {
        return 1;
    }
    if (!RunShaderPathLifecycleConsumptionChecks()) {
        return 1;
    }
    if (!RunTextureMipChainChecks()) {
        return 1;
    }
    if (!RunDirectSamplerObservabilityContractChecks()) {
        return 1;
    }
    if (!RunOcclusionEdgeCaseParityChecks()) {
        return 1;
    }
    if (!RunNormalDetailPolicyRefinementChecks()) {
        return 1;
    }
    if (!RunNormalDetailResponseAndOcclusionIntegrationChecks()) {
        return 1;
    }
    if (!RunMaterialLightingChecks()) {
        return 1;
    }
    if (!RunDebugLineSemanticsChecks()) {
        return 1;
    }
    return 0;
}

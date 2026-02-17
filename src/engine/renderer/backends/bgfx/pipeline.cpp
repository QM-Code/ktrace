#include "internal.hpp"

#include "karma/common/data_path_resolver.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <string>

namespace karma::renderer_backend {
namespace {

bgfx::ShaderHandle loadShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return BGFX_INVALID_HANDLE;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size + 1));
    if (!file.read(reinterpret_cast<char*>(mem->data), size)) {
        return BGFX_INVALID_HANDLE;
    }
    mem->data[mem->size - 1] = '\0';
    return bgfx::createShader(mem);
}

} // namespace

BgfxProgramHandles CreateBgfxProgramHandles() {
    BgfxProgramHandles handles{};
    const auto vs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/vs_mesh.bin").string();
    const auto fs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/fs_mesh.bin").string();
    auto vsh = loadShader(vs_path);
    auto fsh = loadShader(fs_path);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        spdlog::error("Failed to load BGFX shaders: {} {}", vs_path, fs_path);
    } else {
        handles.program = bgfx::createProgram(vsh, fsh, true);
    }

    const auto shadow_vs_path =
        karma::data::Resolve("bgfx/shaders/bin/vk/shadow/vs_shadow_depth.bin").string();
    const auto shadow_fs_path =
        karma::data::Resolve("bgfx/shaders/bin/vk/shadow/fs_shadow_depth.bin").string();
    auto shadow_vsh = loadShader(shadow_vs_path);
    auto shadow_fsh = loadShader(shadow_fs_path);
    if (bgfx::isValid(shadow_vsh) && bgfx::isValid(shadow_fsh)) {
        handles.shadow_depth_program = bgfx::createProgram(shadow_vsh, shadow_fsh, true);
    } else {
        if (bgfx::isValid(shadow_vsh)) {
            bgfx::destroy(shadow_vsh);
        }
        if (bgfx::isValid(shadow_fsh)) {
            bgfx::destroy(shadow_fsh);
        }
        spdlog::warn("Graphics(Bgfx): failed to load shadow depth shaders: {} {}", shadow_vs_path, shadow_fs_path);
    }
    return handles;
}

BgfxUniformSamplerHandles CreateBgfxUniformSamplerHandles(
    uint16_t directional_shadow_cascade_count,
    uint16_t max_local_lights,
    uint16_t max_point_shadow_matrices) {
    BgfxUniformSamplerHandles handles{};
    handles.u_color = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
    handles.u_light_dir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    handles.u_light_color = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    handles.u_ambient_color = bgfx::createUniform("u_ambientColor", bgfx::UniformType::Vec4);
    handles.u_unlit = bgfx::createUniform("u_unlit", bgfx::UniformType::Vec4);
    handles.u_texture_mode = bgfx::createUniform("u_textureMode", bgfx::UniformType::Vec4);
    handles.u_shadow_params0 = bgfx::createUniform("u_shadowParams0", bgfx::UniformType::Vec4);
    handles.u_shadow_params1 = bgfx::createUniform("u_shadowParams1", bgfx::UniformType::Vec4);
    handles.u_shadow_params2 = bgfx::createUniform("u_shadowParams2", bgfx::UniformType::Vec4);
    handles.u_shadow_bias_params = bgfx::createUniform("u_shadowBiasParams", bgfx::UniformType::Vec4);
    handles.u_shadow_axis_right = bgfx::createUniform("u_shadowAxisRight", bgfx::UniformType::Vec4);
    handles.u_shadow_axis_up = bgfx::createUniform("u_shadowAxisUp", bgfx::UniformType::Vec4);
    handles.u_shadow_axis_forward = bgfx::createUniform("u_shadowAxisForward", bgfx::UniformType::Vec4);
    handles.u_shadow_cascade_splits =
        bgfx::createUniform("u_shadowCascadeSplits", bgfx::UniformType::Vec4);
    handles.u_shadow_cascade_world_texel =
        bgfx::createUniform("u_shadowCascadeWorldTexel", bgfx::UniformType::Vec4);
    handles.u_shadow_cascade_params =
        bgfx::createUniform("u_shadowCascadeParams", bgfx::UniformType::Vec4);
    handles.u_shadow_camera_position =
        bgfx::createUniform("u_shadowCameraPosition", bgfx::UniformType::Vec4);
    handles.u_shadow_camera_forward =
        bgfx::createUniform("u_shadowCameraForward", bgfx::UniformType::Vec4);
    handles.u_shadow_cascade_uv_proj =
        bgfx::createUniform(
            "u_shadowCascadeUvProj",
            bgfx::UniformType::Mat4,
            directional_shadow_cascade_count);
    handles.u_local_light_count = bgfx::createUniform("u_localLightCount", bgfx::UniformType::Vec4);
    handles.u_local_light_params = bgfx::createUniform("u_localLightParams", bgfx::UniformType::Vec4);
    handles.u_local_light_pos_range =
        bgfx::createUniform("u_localLightPosRange", bgfx::UniformType::Vec4, max_local_lights);
    handles.u_local_light_color_intensity =
        bgfx::createUniform("u_localLightColorIntensity", bgfx::UniformType::Vec4, max_local_lights);
    handles.u_local_light_shadow_slot =
        bgfx::createUniform("u_localLightShadowSlot", bgfx::UniformType::Vec4, max_local_lights);
    handles.u_point_shadow_params = bgfx::createUniform("u_pointShadowParams", bgfx::UniformType::Vec4);
    handles.u_point_shadow_atlas_texel = bgfx::createUniform("u_pointShadowAtlasTexel", bgfx::UniformType::Vec4);
    handles.u_point_shadow_tuning = bgfx::createUniform("u_pointShadowTuning", bgfx::UniformType::Vec4);
    handles.u_point_shadow_uv_proj =
        bgfx::createUniform("u_pointShadowUvProj", bgfx::UniformType::Mat4, max_point_shadow_matrices);
    handles.s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    handles.s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
    handles.s_occlusion = bgfx::createUniform("s_occlusion", bgfx::UniformType::Sampler);
    handles.s_shadow = bgfx::createUniform("s_shadow", bgfx::UniformType::Sampler);
    handles.s_point_shadow = bgfx::createUniform("s_pointShadow", bgfx::UniformType::Sampler);
    return handles;
}

} // namespace karma::renderer_backend

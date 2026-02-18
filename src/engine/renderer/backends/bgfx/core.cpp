#include "karma/renderer/backend.hpp"
#include "internal.hpp"

#include "../internal/backend_factory.hpp"
#include "../internal/direct_sampler_observability.hpp"
#include "../internal/debug_line.hpp"
#include "../internal/directional_shadow.hpp"
#include "../internal/environment_lighting.hpp"
#include "../internal/material_semantics.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/layers.hpp"

#include <spdlog/spdlog.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {
namespace {

constexpr bgfx::ViewId kBgfxShadowViewIdBase = 0;
constexpr std::size_t kDirectionalShadowCascadeCount = kBgfxDirectionalShadowCascadeCount;
constexpr bgfx::ViewId kBgfxMainViewId =
    static_cast<bgfx::ViewId>(kBgfxShadowViewIdBase + kDirectionalShadowCascadeCount);
constexpr std::size_t kMaxLocalLights = kBgfxMaxLocalLights;
constexpr std::size_t kMaxPointShadowLights = kBgfxMaxPointShadowLights;
constexpr std::size_t kMaxPointShadowMatrices = kBgfxMaxPointShadowMatrices;

using Mesh = BgfxMesh;
using Material = BgfxMaterial;
using RenderableDraw = BgfxRenderableDraw;

class BgfxCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override {
        spdlog::error("BGFX fatal: {}:{} code={} {}", filePath ? filePath : "(null)", line, static_cast<int>(code),
                      str ? str : "");
        std::abort();
    }

    void traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList) override {
        if (!karma::common::logging::ShouldTraceChannel("render.bgfx.internal")) {
            return;
        }
        std::array<char, 2048> stack_buf{};
        va_list args_copy;
        va_copy(args_copy, argList);
        int needed = std::vsnprintf(stack_buf.data(), stack_buf.size(), format, args_copy);
        va_end(args_copy);
        if (needed < 0) {
            return;
        }
        std::string message;
        if (static_cast<size_t>(needed) < stack_buf.size()) {
            message.assign(stack_buf.data(), static_cast<size_t>(needed));
        } else {
            std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
            va_list args_copy2;
            va_copy(args_copy2, argList);
            std::vsnprintf(heap_buf.data(), heap_buf.size(), format, args_copy2);
            va_end(args_copy2);
            message.assign(heap_buf.data(), static_cast<size_t>(needed));
        }
        KARMA_TRACE("render.bgfx.internal", "{}:{}: {}", filePath ? filePath : "(null)", line, message.c_str());
    }

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

} // namespace

class BgfxBackend final : public Backend {
 public:
    explicit BgfxBackend(karma::platform::Window& window) {
        karma::platform::NativeWindowHandle native = window.nativeHandle();
        bgfx::PlatformData pd{};
        if (native.is_wayland && native.wayland_surface && native.display) {
            pd.nwh = native.wayland_surface;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Wayland;
            KARMA_TRACE("render.bgfx", "using Wayland surface handle");
        } else if (native.window) {
            pd.nwh = native.window;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Default;
            KARMA_TRACE("render.bgfx", "using default native window handle");
        } else {
            spdlog::error("Graphics(Bgfx): failed to resolve native window handle");
        }
        KARMA_TRACE("render.bgfx",
                    "platform data type={} nwh={} ndt={}",
                    static_cast<int>(pd.type), pd.nwh, pd.ndt);
        if (!pd.ndt || !pd.nwh) {
            spdlog::error("Graphics(Bgfx): missing native display/window handle (ndt={}, nwh={})", pd.ndt, pd.nwh);
            return;
        }

        bgfx::Init init{};
        init.type = bgfx::RendererType::Vulkan;
        init.vendorId = BGFX_PCI_ID_NONE;
        init.platformData = pd;
        init.callback = &callback_;
        window.getFramebufferSize(width_, height_);
        init.resolution.width = static_cast<uint32_t>(width_);
        init.resolution.height = static_cast<uint32_t>(height_);
        init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER;
        KARMA_TRACE("render.bgfx",
                    "init renderer=Vulkan size={}x{} reset=0x{:08x}",
                    init.resolution.width, init.resolution.height, init.resolution.reset);
        if (!bgfx::init(init)) {
            spdlog::error("BGFX init failed");
            return;
        }
        KARMA_TRACE("render.bgfx", "init success renderer={}",
                    bgfx::getRendererName(bgfx::getRendererType()));
        const bgfx::Caps* init_caps = bgfx::getCaps();
        if (init_caps) {
            const uint16_t d32f_caps = init_caps->formats[bgfx::TextureFormat::D32F];
            shadow_depth_attachment_supported_ =
                (0 != (d32f_caps & BGFX_CAPS_FORMAT_TEXTURE_2D)) &&
                (0 != (d32f_caps & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER));
        }
        KARMA_TRACE("render.bgfx",
                    "shadow depth attachment support={}",
                    shadow_depth_attachment_supported_ ? 1 : 0);
        initialized_ = true;
        bgfx::setViewClear(kBgfxMainViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        const BgfxProgramHandles program_handles = CreateBgfxProgramHandles();
        program_ = program_handles.program;
        shadow_depth_program_ = program_handles.shadow_depth_program;
        KARMA_TRACE("render.bgfx", "program valid={}", bgfx::isValid(program_) ? 1 : 0);
        KARMA_TRACE("render.bgfx", "shadow depth program valid={}", bgfx::isValid(shadow_depth_program_) ? 1 : 0);
        layout_ = PosNormalVertex::layout();
        const BgfxUniformSamplerHandles uniform_handles = CreateBgfxUniformSamplerHandles(
            static_cast<uint16_t>(kDirectionalShadowCascadeCount),
            static_cast<uint16_t>(kMaxLocalLights),
            static_cast<uint16_t>(kMaxPointShadowMatrices));
        u_color_ = uniform_handles.u_color;
        u_light_dir_ = uniform_handles.u_light_dir;
        u_light_color_ = uniform_handles.u_light_color;
        u_ambient_color_ = uniform_handles.u_ambient_color;
        u_unlit_ = uniform_handles.u_unlit;
        u_texture_mode_ = uniform_handles.u_texture_mode;
        u_shadow_params0_ = uniform_handles.u_shadow_params0;
        u_shadow_params1_ = uniform_handles.u_shadow_params1;
        u_shadow_params2_ = uniform_handles.u_shadow_params2;
        u_shadow_bias_params_ = uniform_handles.u_shadow_bias_params;
        u_shadow_axis_right_ = uniform_handles.u_shadow_axis_right;
        u_shadow_axis_up_ = uniform_handles.u_shadow_axis_up;
        u_shadow_axis_forward_ = uniform_handles.u_shadow_axis_forward;
        u_shadow_cascade_splits_ = uniform_handles.u_shadow_cascade_splits;
        u_shadow_cascade_world_texel_ = uniform_handles.u_shadow_cascade_world_texel;
        u_shadow_cascade_params_ = uniform_handles.u_shadow_cascade_params;
        u_shadow_camera_position_ = uniform_handles.u_shadow_camera_position;
        u_shadow_camera_forward_ = uniform_handles.u_shadow_camera_forward;
        u_shadow_cascade_uv_proj_ = uniform_handles.u_shadow_cascade_uv_proj;
        u_local_light_count_ = uniform_handles.u_local_light_count;
        u_local_light_params_ = uniform_handles.u_local_light_params;
        u_local_light_pos_range_ = uniform_handles.u_local_light_pos_range;
        u_local_light_color_intensity_ = uniform_handles.u_local_light_color_intensity;
        u_local_light_shadow_slot_ = uniform_handles.u_local_light_shadow_slot;
        u_point_shadow_params_ = uniform_handles.u_point_shadow_params;
        u_point_shadow_atlas_texel_ = uniform_handles.u_point_shadow_atlas_texel;
        u_point_shadow_tuning_ = uniform_handles.u_point_shadow_tuning;
        u_point_shadow_uv_proj_ = uniform_handles.u_point_shadow_uv_proj;
        s_tex_ = uniform_handles.s_tex;
        s_normal_ = uniform_handles.s_normal;
        s_occlusion_ = uniform_handles.s_occlusion;
        s_shadow_ = uniform_handles.s_shadow;
        s_point_shadow_ = uniform_handles.s_point_shadow;
        white_tex_ = createWhiteTexture();
        const bool uniform_contract_ready =
            bgfx::isValid(program_) &&
            bgfx::isValid(s_tex_) &&
            bgfx::isValid(s_normal_) &&
            bgfx::isValid(s_occlusion_) &&
            bgfx::isValid(u_texture_mode_);
        direct_sampler_shader_alignment_ = EvaluateBgfxDirectSamplerShaderAlignment();
        direct_sampler_contract_report_ = detail::EvaluateBgfxDirectSamplerContract(
            uniform_contract_ready,
            detail::BgfxDirectSamplerAlignmentReport{
                direct_sampler_shader_alignment_.source_present_mode,
                direct_sampler_shader_alignment_.source_absent_compat_mode,
                direct_sampler_shader_alignment_.aligned,
                direct_sampler_shader_alignment_.reason,
            });
        supports_direct_multi_sampler_inputs_ = direct_sampler_contract_report_.ready_for_direct_path;
        direct_sampler_disable_reason_ = direct_sampler_contract_report_.reason;
        KARMA_TRACE("render.bgfx",
                    "direct sampler readiness uniforms={} aligned={} sourceModePresent={} sourceModeCompat={} sourceAbsentIntegrity={} enabled={} reason={} integrityReason={} source='{}' binary='{}' manifest='{}'",
                    direct_sampler_contract_report_.uniform_contract_ready ? 1 : 0,
                    direct_sampler_contract_report_.shader_alignment_ready ? 1 : 0,
                    direct_sampler_shader_alignment_.source_present_mode ? 1 : 0,
                    direct_sampler_shader_alignment_.source_absent_compat_mode ? 1 : 0,
                    direct_sampler_shader_alignment_.source_absent_integrity_ready ? 1 : 0,
                    supports_direct_multi_sampler_inputs_ ? 1 : 0,
                    direct_sampler_disable_reason_,
                    direct_sampler_shader_alignment_.source_absent_integrity_reason,
                    direct_sampler_shader_alignment_.source_path,
                    direct_sampler_shader_alignment_.binary_path,
                    direct_sampler_shader_alignment_.integrity_manifest_path);
        if (!supports_direct_multi_sampler_inputs_) {
            spdlog::warn("Graphics(Bgfx): direct sampler path disabled (reason={}, integrityReason={}, source='{}', binary='{}', manifest='{}')",
                         direct_sampler_disable_reason_,
                         direct_sampler_shader_alignment_.source_absent_integrity_reason,
                         direct_sampler_shader_alignment_.source_path,
                         direct_sampler_shader_alignment_.binary_path,
                         direct_sampler_shader_alignment_.integrity_manifest_path);
        }
    }

    ~BgfxBackend() override {
        if (!initialized_) {
            return;
        }
        for (auto& [id, mesh] : meshes_) {
            if (bgfx::isValid(mesh.vbh)) bgfx::destroy(mesh.vbh);
            if (bgfx::isValid(mesh.ibh)) bgfx::destroy(mesh.ibh);
        }
        for (auto& [id, material] : materials_) {
            if (bgfx::isValid(material.tex)) bgfx::destroy(material.tex);
            if (bgfx::isValid(material.normal_tex)) bgfx::destroy(material.normal_tex);
            if (bgfx::isValid(material.occlusion_tex)) bgfx::destroy(material.occlusion_tex);
        }
        if (bgfx::isValid(program_)) bgfx::destroy(program_);
        if (bgfx::isValid(shadow_depth_program_)) bgfx::destroy(shadow_depth_program_);
        if (bgfx::isValid(u_color_)) bgfx::destroy(u_color_);
        if (bgfx::isValid(u_light_dir_)) bgfx::destroy(u_light_dir_);
        if (bgfx::isValid(u_light_color_)) bgfx::destroy(u_light_color_);
        if (bgfx::isValid(u_ambient_color_)) bgfx::destroy(u_ambient_color_);
        if (bgfx::isValid(u_unlit_)) bgfx::destroy(u_unlit_);
        if (bgfx::isValid(u_texture_mode_)) bgfx::destroy(u_texture_mode_);
        if (bgfx::isValid(u_shadow_params0_)) bgfx::destroy(u_shadow_params0_);
        if (bgfx::isValid(u_shadow_params1_)) bgfx::destroy(u_shadow_params1_);
        if (bgfx::isValid(u_shadow_params2_)) bgfx::destroy(u_shadow_params2_);
        if (bgfx::isValid(u_shadow_bias_params_)) bgfx::destroy(u_shadow_bias_params_);
        if (bgfx::isValid(u_shadow_axis_right_)) bgfx::destroy(u_shadow_axis_right_);
        if (bgfx::isValid(u_shadow_axis_up_)) bgfx::destroy(u_shadow_axis_up_);
        if (bgfx::isValid(u_shadow_axis_forward_)) bgfx::destroy(u_shadow_axis_forward_);
        if (bgfx::isValid(u_shadow_cascade_splits_)) bgfx::destroy(u_shadow_cascade_splits_);
        if (bgfx::isValid(u_shadow_cascade_world_texel_)) bgfx::destroy(u_shadow_cascade_world_texel_);
        if (bgfx::isValid(u_shadow_cascade_params_)) bgfx::destroy(u_shadow_cascade_params_);
        if (bgfx::isValid(u_shadow_camera_position_)) bgfx::destroy(u_shadow_camera_position_);
        if (bgfx::isValid(u_shadow_camera_forward_)) bgfx::destroy(u_shadow_camera_forward_);
        if (bgfx::isValid(u_shadow_cascade_uv_proj_)) bgfx::destroy(u_shadow_cascade_uv_proj_);
        if (bgfx::isValid(u_local_light_count_)) bgfx::destroy(u_local_light_count_);
        if (bgfx::isValid(u_local_light_params_)) bgfx::destroy(u_local_light_params_);
        if (bgfx::isValid(u_local_light_pos_range_)) bgfx::destroy(u_local_light_pos_range_);
        if (bgfx::isValid(u_local_light_color_intensity_)) bgfx::destroy(u_local_light_color_intensity_);
        if (bgfx::isValid(u_local_light_shadow_slot_)) bgfx::destroy(u_local_light_shadow_slot_);
        if (bgfx::isValid(u_point_shadow_params_)) bgfx::destroy(u_point_shadow_params_);
        if (bgfx::isValid(u_point_shadow_atlas_texel_)) bgfx::destroy(u_point_shadow_atlas_texel_);
        if (bgfx::isValid(u_point_shadow_tuning_)) bgfx::destroy(u_point_shadow_tuning_);
        if (bgfx::isValid(u_point_shadow_uv_proj_)) bgfx::destroy(u_point_shadow_uv_proj_);
        if (bgfx::isValid(s_tex_)) bgfx::destroy(s_tex_);
        if (bgfx::isValid(s_normal_)) bgfx::destroy(s_normal_);
        if (bgfx::isValid(s_occlusion_)) bgfx::destroy(s_occlusion_);
        if (bgfx::isValid(s_shadow_)) bgfx::destroy(s_shadow_);
        if (bgfx::isValid(s_point_shadow_)) bgfx::destroy(s_point_shadow_);
        if (bgfx::isValid(shadow_fb_)) bgfx::destroy(shadow_fb_);
        if (bgfx::isValid(shadow_tex_)) bgfx::destroy(shadow_tex_);
        if (bgfx::isValid(point_shadow_tex_)) bgfx::destroy(point_shadow_tex_);
        if (bgfx::isValid(white_tex_)) bgfx::destroy(white_tex_);
        bgfx::shutdown();
    }

    void beginFrame(int width, int height, float dt) override {
        (void)dt;
        if (!initialized_) {
            return;
        }
        draw_items_.clear();
        debug_lines_.clear();
        width_ = width;
        height_ = height;
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER);
        const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
            detail::ResolveEnvironmentLightingSemantics(environment_);
        const glm::vec4 clear_color = detail::ComputeEnvironmentClearColor(environment_semantics);
        bgfx::setViewClear(kBgfxMainViewId,
                           BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           PackBgfxColorRgba8(clear_color),
                           1.0f,
                           0);
        bgfx::setViewRect(kBgfxMainViewId, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        bgfx::touch(kBgfxMainViewId);
    }

    void endFrame() override {
        if (!initialized_) {
            return;
        }
        bgfx::frame();
    }

    renderer::MeshId createMesh(const renderer::MeshData& mesh) override {
        if (!initialized_) {
            return renderer::kInvalidMesh;
        }
        return CreateBgfxMeshAsset(mesh, layout_, next_mesh_id_, meshes_);
    }

    void destroyMesh(renderer::MeshId mesh) override {
        DestroyBgfxMeshAsset(mesh, meshes_);
    }

    renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) override {
        if (!initialized_) {
            return renderer::kInvalidMaterial;
        }
        return CreateBgfxMaterialAsset(
            material,
            supports_direct_multi_sampler_inputs_,
            next_material_id_,
            materials_);
    }

    void destroyMaterial(renderer::MaterialId material) override {
        DestroyBgfxMaterialAsset(material, materials_);
    }

    void submit(const renderer::DrawItem& item) override {
        if (!initialized_) {
            return;
        }
        draw_items_.push_back(item);
    }

    void submitDebugLine(const renderer::DebugLineItem& line) override {
        if (!initialized_) {
            return;
        }
        const detail::ResolvedDebugLineSemantics semantics = detail::ResolveDebugLineSemantics(line);
        if (!semantics.draw || !detail::ValidateResolvedDebugLineSemantics(semantics)) {
            return;
        }
        debug_lines_.push_back(semantics);
    }

    void renderFrame() override {
        renderLayer(0);
    }

    void renderLayer(renderer::LayerId layer) override {
        if (!initialized_ || !bgfx::isValid(program_)) {
            return;
        }

        float view[16];
        float proj[16];
        bx::mtxLookAt(view, bx::Vec3(camera_.position.x, camera_.position.y, camera_.position.z),
                      bx::Vec3(camera_.target.x, camera_.target.y, camera_.target.z));
        // BGFX uses left-handed view/projection by default; mirror X to match the
        // engine's right-handed camera conventions, then flip culling below.
        float mirror[16];
        float view_mirror[16];
        bx::mtxScale(mirror, -1.0f, 1.0f, 1.0f);
        bx::mtxMul(view_mirror, view, mirror);
        bx::mtxProj(proj, camera_.fov_y_degrees, float(width_) / float(height_),
                    camera_.near_clip, camera_.far_clip, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(kBgfxMainViewId, view_mirror, proj);

        const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
            detail::ResolveDirectionalShadowSemantics(light_);
        const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
            detail::ResolveEnvironmentLightingSemantics(environment_);
        const bool gpu_shadow_requested =
            light_.shadow.execution_mode == renderer::DirectionalLightData::ShadowExecutionMode::GpuDefault;
        const bool world_layer = (layer == renderer::kLayerWorld);

        std::vector<RenderableDraw> renderables{};
        renderables.reserve(draw_items_.size());
        std::vector<detail::DirectionalShadowCaster> shadow_casters{};
        shadow_casters.reserve(draw_items_.size());
        std::vector<renderer::DrawItem> current_shadow_items{};
        if (world_layer) {
            current_shadow_items.reserve(draw_items_.size());
        }
        for (const auto& item : draw_items_) {
            if (item.layer != layer) {
                continue;
            }
            auto mesh_it = meshes_.find(item.mesh);
            if (mesh_it == meshes_.end()) {
                continue;
            }
            auto mat_it = materials_.find(item.material);
            if (mat_it == materials_.end()) {
                continue;
            }
            if (!mat_it->second.semantics.draw) {
                continue;
            }
            renderables.push_back({&item, &mesh_it->second, &mat_it->second});
            if (world_layer) {
                current_shadow_items.push_back(item);
            }
            if (!mesh_it->second.shadow_positions.empty() &&
                !mesh_it->second.shadow_indices.empty()) {
                detail::DirectionalShadowCaster caster{};
                caster.transform = item.transform;
                caster.positions = &mesh_it->second.shadow_positions;
                caster.indices = &mesh_it->second.shadow_indices;
                caster.sample_center = mesh_it->second.shadow_center;
                caster.casts_shadow = item.casts_shadow && !mat_it->second.semantics.alpha_blend;
                shadow_casters.push_back(caster);
            }
        }

        const float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : 1.0f;
        const detail::DirectionalShadowView shadow_view{
            camera_,
            aspect,
        };
        BgfxShadowState shadow_state{};
        shadow_state.shadow_depth_program = shadow_depth_program_;
        shadow_state.u_shadow_params0 = u_shadow_params0_;
        shadow_state.u_shadow_params1 = u_shadow_params1_;
        shadow_state.u_shadow_params2 = u_shadow_params2_;
        shadow_state.u_shadow_bias_params = u_shadow_bias_params_;
        shadow_state.u_shadow_axis_right = u_shadow_axis_right_;
        shadow_state.u_shadow_axis_up = u_shadow_axis_up_;
        shadow_state.u_shadow_axis_forward = u_shadow_axis_forward_;
        shadow_state.shadow_tex = &shadow_tex_;
        shadow_state.point_shadow_tex = &point_shadow_tex_;
        shadow_state.shadow_fb = &shadow_fb_;
        shadow_state.shadow_tex_size = &shadow_tex_size_;
        shadow_state.point_shadow_tex_width = &point_shadow_tex_width_;
        shadow_state.point_shadow_tex_height = &point_shadow_tex_height_;
        shadow_state.shadow_tex_is_rt = &shadow_tex_is_rt_;
        shadow_state.shadow_tex_rt_uses_depth = &shadow_tex_rt_uses_depth_;
        shadow_state.shadow_depth_attachment_supported = shadow_depth_attachment_supported_;
        shadow_state.shadow_tex_ready = &shadow_tex_ready_;
        shadow_state.point_shadow_tex_ready = &point_shadow_tex_ready_;
        shadow_state.cached_shadow_map = &cached_shadow_map_;
        shadow_state.cached_shadow_cascades = &cached_shadow_cascades_;
        shadow_state.cached_point_shadow_map = &cached_point_shadow_map_;
        shadow_state.shadow_update_every_frames = &shadow_update_every_frames_;
        shadow_state.shadow_frames_until_update = &shadow_frames_until_update_;
        shadow_state.point_shadow_frames_until_update = &point_shadow_frames_until_update_;
        shadow_state.point_shadow_slot_source_index = &point_shadow_slot_source_index_;
        shadow_state.point_shadow_slot_position = &point_shadow_slot_position_;
        shadow_state.point_shadow_slot_range = &point_shadow_slot_range_;
        shadow_state.point_shadow_face_dirty = &point_shadow_face_dirty_;
        shadow_state.point_shadow_face_cursor = &point_shadow_face_cursor_;
        shadow_state.point_shadow_cache_initialized = &point_shadow_cache_initialized_;
        shadow_state.point_shadow_position_threshold = point_shadow_position_threshold_;
        shadow_state.point_shadow_range_threshold = point_shadow_range_threshold_;
        shadow_state.previous_shadow_items = &previous_shadow_items_;
        shadow_state.shadow_cache_inputs_valid = &shadow_cache_inputs_valid_;
        shadow_state.cached_shadow_camera_position = &cached_shadow_camera_position_;
        shadow_state.cached_shadow_camera_forward = &cached_shadow_camera_forward_;
        shadow_state.cached_shadow_light_direction = &cached_shadow_light_direction_;
        shadow_state.cached_shadow_camera_aspect = &cached_shadow_camera_aspect_;
        shadow_state.cached_shadow_camera_fov_y_degrees = &cached_shadow_camera_fov_y_degrees_;
        shadow_state.cached_shadow_camera_near = &cached_shadow_camera_near_;
        shadow_state.cached_shadow_camera_far = &cached_shadow_camera_far_;
        shadow_state.directional_shadow_position_threshold = directional_shadow_position_threshold_;
        shadow_state.directional_shadow_angle_threshold_deg = directional_shadow_angle_threshold_deg_;

        const BgfxDirectionalShadowUpdateResult directional_shadow_update =
            UpdateBgfxDirectionalShadowCache(
                shadow_state,
                layer,
                camera_,
                light_,
                world_layer,
                gpu_shadow_requested,
                shadow_semantics,
                aspect,
                shadow_view,
                renderables,
                shadow_casters,
                current_shadow_items,
                meshes_,
                materials_);
        const glm::vec3 current_camera_forward = directional_shadow_update.current_camera_forward;
        const bool point_shadow_structural_change =
            directional_shadow_update.point_shadow_structural_change;
        const std::vector<glm::vec4>& moved_point_shadow_caster_bounds =
            directional_shadow_update.moved_point_shadow_caster_bounds;

        const detail::DirectionalShadowCascadeSet active_shadow_cascades =
            cached_shadow_cascades_.ready
                ? cached_shadow_cascades_
                : detail::BuildDirectionalShadowCascadeSetFromSingleMap(cached_shadow_map_);
        const detail::DirectionalShadowMap& shadow_map = cached_shadow_map_;
        const detail::DirectionalShadowCascade& primary_cascade =
            active_shadow_cascades.cascades[0];
        const bgfx::Caps* caps = bgfx::getCaps();
        const detail::ShadowClipDepthTransform clip_depth =
            detail::ResolveShadowClipDepthTransform(caps && caps->homogeneousDepth);
        const float inv_depth_range = primary_cascade.depth_range > 1e-6f
            ? (1.0f / primary_cascade.depth_range)
            : 1.0f;
        std::array<glm::mat4, kDirectionalShadowCascadeCount> shadow_cascade_uv_proj{};
        for (glm::mat4& matrix : shadow_cascade_uv_proj) {
            matrix = glm::mat4(1.0f);
        }
        std::array<float, kDirectionalShadowCascadeCount> shadow_cascade_splits{};
        shadow_cascade_splits.fill(std::numeric_limits<float>::max());
        std::array<float, kDirectionalShadowCascadeCount> shadow_cascade_world_texel{};
        shadow_cascade_world_texel.fill(0.0f);
        for (std::size_t cascade_idx = 0; cascade_idx < kDirectionalShadowCascadeCount; ++cascade_idx) {
            const detail::DirectionalShadowCascade& cascade =
                active_shadow_cascades.cascades[cascade_idx];
            if (cascade.ready) {
                shadow_cascade_uv_proj[cascade_idx] = cascade.shadow_uv_proj;
                shadow_cascade_world_texel[cascade_idx] = cascade.world_texel;
            }
            shadow_cascade_splits[cascade_idx] = active_shadow_cascades.splits[cascade_idx];
        }
        const float active_shadow_atlas_inv_size =
            active_shadow_cascades.atlas_size > 0
                ? (1.0f / static_cast<float>(active_shadow_cascades.atlas_size))
                : 0.0f;
        const float shadow_params0[4] = {
            shadow_tex_ready_ && active_shadow_cascades.ready ? 1.0f : 0.0f,
            active_shadow_cascades.strength,
            active_shadow_cascades.bias,
            active_shadow_cascades.map_size > 0
                ? (1.0f / static_cast<float>(active_shadow_cascades.map_size))
                : 0.0f,
        };
        const float shadow_params1[4] = {
            primary_cascade.extent,
            primary_cascade.center_x,
            primary_cascade.center_y,
            primary_cascade.min_depth,
        };
        const float shadow_params2[4] = {
            inv_depth_range,
            static_cast<float>(active_shadow_cascades.pcf_radius),
            clip_depth.scale,
            clip_depth.bias,
        };
        const float shadow_bias_params[4] = {
            shadow_semantics.receiver_bias_scale,
            shadow_semantics.normal_bias_scale,
            shadow_semantics.raster_depth_bias,
            shadow_semantics.raster_slope_bias,
        };
        const float shadow_axis_right[4] = {
            active_shadow_cascades.axis_right.x,
            active_shadow_cascades.axis_right.y,
            active_shadow_cascades.axis_right.z,
            0.0f,
        };
        const float shadow_axis_up[4] = {
            active_shadow_cascades.axis_up.x,
            active_shadow_cascades.axis_up.y,
            active_shadow_cascades.axis_up.z,
            0.0f,
        };
        const float shadow_axis_forward[4] = {
            active_shadow_cascades.axis_forward.x,
            active_shadow_cascades.axis_forward.y,
            active_shadow_cascades.axis_forward.z,
            0.0f,
        };
        const float shadow_cascade_splits_uniform[4] = {
            shadow_cascade_splits[0],
            shadow_cascade_splits[1],
            shadow_cascade_splits[2],
            shadow_cascade_splits[3],
        };
        const float shadow_cascade_world_texel_uniform[4] = {
            shadow_cascade_world_texel[0],
            shadow_cascade_world_texel[1],
            shadow_cascade_world_texel[2],
            shadow_cascade_world_texel[3],
        };
        const float shadow_cascade_params_uniform[4] = {
            active_shadow_cascades.transition_fraction,
            static_cast<float>(std::max(active_shadow_cascades.cascade_count, 1)),
            active_shadow_atlas_inv_size,
            0.0f,
        };
        const float shadow_camera_position[4] = {
            camera_.position.x,
            camera_.position.y,
            camera_.position.z,
            0.0f,
        };
        const float shadow_camera_forward[4] = {
            current_camera_forward.x,
            current_camera_forward.y,
            current_camera_forward.z,
            0.0f,
        };
        std::array<float, kMaxLocalLights * 4u> local_light_pos_range{};
        std::array<float, kMaxLocalLights * 4u> local_light_color_intensity{};
        std::array<float, kMaxLocalLights * 4u> local_light_shadow_slot{};
        std::array<int, kMaxLocalLights> local_light_source_indices{};
        local_light_source_indices.fill(-1);
        for (std::size_t slot = 0; slot < kMaxLocalLights; ++slot) {
            local_light_shadow_slot[slot * 4u] = -1.0f;
        }
        int local_light_count = 0;
        int point_shadow_requested = 0;
        for (std::size_t light_idx = 0; light_idx < lights_.size(); ++light_idx) {
            const renderer::LightData& light = lights_[light_idx];
            if (!light.enabled || light.type != renderer::LightType::Point) {
                continue;
            }
            const float clamped_range = std::max(light.range, 0.0f);
            const float clamped_intensity = std::max(light.intensity, 0.0f);
            if (light.casts_shadows && clamped_range > 0.001f && clamped_intensity > 0.0f) {
                ++point_shadow_requested;
            }
            if (local_light_count >= static_cast<int>(kMaxLocalLights) ||
                clamped_range <= 0.001f ||
                clamped_intensity <= 0.0f) {
                continue;
            }
            const std::size_t base = static_cast<std::size_t>(local_light_count) * 4u;
            local_light_pos_range[base + 0u] = light.position.x;
            local_light_pos_range[base + 1u] = light.position.y;
            local_light_pos_range[base + 2u] = light.position.z;
            local_light_pos_range[base + 3u] = clamped_range;
            local_light_color_intensity[base + 0u] = light.color.r;
            local_light_color_intensity[base + 1u] = light.color.g;
            local_light_color_intensity[base + 2u] = light.color.b;
            local_light_color_intensity[base + 3u] = clamped_intensity;
            local_light_source_indices[static_cast<std::size_t>(local_light_count)] = static_cast<int>(light_idx);
            ++local_light_count;
        }
        const std::vector<int> point_shadow_selected_indices =
            detail::SelectPointShadowLightIndices(shadow_semantics, lights_);
        const BgfxPointShadowUpdateResult point_shadow_update =
            UpdateBgfxPointShadowCache(
                shadow_state,
                shadow_semantics,
                world_layer,
                lights_,
                shadow_casters,
                point_shadow_selected_indices,
                point_shadow_structural_change,
                moved_point_shadow_caster_bounds);
        const int point_shadow_selected = point_shadow_update.point_shadow_selected;
        const int point_shadow_dirty_faces = point_shadow_update.point_shadow_dirty_faces;
        const int point_shadow_updated_faces = point_shadow_update.point_shadow_updated_faces;
        const int point_shadow_budget = point_shadow_update.point_shadow_budget;
        const bool point_shadow_full_refresh = point_shadow_update.point_shadow_full_refresh;
        int point_shadow_active = 0;
        std::array<glm::mat4, kMaxPointShadowMatrices> point_shadow_uv_proj{};
        for (glm::mat4& matrix : point_shadow_uv_proj) {
            matrix = glm::mat4(1.0f);
        }
        if (point_shadow_tex_ready_ && cached_point_shadow_map_.ready) {
            point_shadow_active = std::max(0, cached_point_shadow_map_.light_count);
            const std::size_t matrix_count = std::min(
                kMaxPointShadowMatrices,
                cached_point_shadow_map_.uv_proj.size());
            for (std::size_t i = 0; i < matrix_count; ++i) {
                point_shadow_uv_proj[i] = cached_point_shadow_map_.uv_proj[i];
            }
            const std::size_t source_count = cached_point_shadow_map_.source_light_indices.size();
            for (int local_slot = 0; local_slot < local_light_count; ++local_slot) {
                const int source_idx = local_light_source_indices[static_cast<std::size_t>(local_slot)];
                if (source_idx < 0) {
                    continue;
                }
                int shadow_slot = -1;
                for (int slot = 0; slot < point_shadow_active; ++slot) {
                    const std::size_t source_slot = static_cast<std::size_t>(slot);
                    if (source_slot >= source_count) {
                        break;
                    }
                    if (cached_point_shadow_map_.source_light_indices[source_slot] == source_idx) {
                        shadow_slot = slot;
                        break;
                    }
                }
                const std::size_t base = static_cast<std::size_t>(local_slot) * 4u;
                local_light_shadow_slot[base + 0u] = static_cast<float>(shadow_slot);
            }
        }
        const float local_light_count_uniform[4] = {
            static_cast<float>(local_light_count),
            0.0f,
            0.0f,
            0.0f,
        };
        const float local_light_params_uniform[4] = {
            shadow_semantics.local_light_distance_damping,
            shadow_semantics.local_light_range_falloff_exponent,
            shadow_semantics.ao_affects_local_lights ? 1.0f : 0.0f,
            shadow_semantics.local_light_directional_shadow_lift_strength,
        };
        const bool point_shadow_enabled_uniform = world_layer && point_shadow_active > 0;
        const float point_shadow_params_uniform[4] = {
            point_shadow_enabled_uniform ? 1.0f : 0.0f,
            (point_shadow_active > 0) ? cached_point_shadow_map_.texel_size : 0.0f,
            static_cast<float>(std::clamp(shadow_semantics.pcf_radius, 0, 1)),
            point_shadow_enabled_uniform ? static_cast<float>(point_shadow_active) : 0.0f,
        };
        const float point_shadow_atlas_texel_uniform[4] = {
            (point_shadow_enabled_uniform && cached_point_shadow_map_.atlas_width > 0)
                ? (1.0f / static_cast<float>(cached_point_shadow_map_.atlas_width))
                : 0.0f,
            (point_shadow_enabled_uniform && cached_point_shadow_map_.atlas_height > 0)
                ? (1.0f / static_cast<float>(cached_point_shadow_map_.atlas_height))
                : 0.0f,
            0.0f,
            0.0f,
        };
        const float point_shadow_tuning_uniform[4] = {
            shadow_semantics.point_constant_bias,
            shadow_semantics.point_slope_bias_scale,
            shadow_semantics.point_normal_bias_scale,
            shadow_semantics.point_receiver_bias_scale,
        };
        const char* point_shadow_reason = "point_shadow_cache_hit_clean";
        if (shadow_semantics.point_max_shadow_lights <= 0) {
            point_shadow_reason = "point_shadows_disabled_by_cap";
        } else if (!shadow_semantics.enabled) {
            point_shadow_reason = "point_shadows_disabled";
        } else if (!world_layer) {
            point_shadow_reason = "point_shadow_non_world_layer";
        } else if (point_shadow_selected <= 0) {
            point_shadow_reason = "point_shadow_no_shadow_lights";
        } else if (shadow_casters.empty()) {
            point_shadow_reason = "point_shadow_no_casters";
        } else if (!cached_point_shadow_map_.ready) {
            point_shadow_reason = "point_shadow_map_build_failed";
        } else if (!point_shadow_tex_ready_) {
            point_shadow_reason = "point_shadow_upload_failed";
        } else if (point_shadow_active <= 0) {
            point_shadow_reason = "point_shadow_no_active_slots";
        } else if (point_shadow_updated_faces > 0) {
            point_shadow_reason = "point_shadow_faces_updated";
        } else if (point_shadow_dirty_faces > 0) {
            point_shadow_reason = "point_shadow_waiting_budget";
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(point_shadow_requested) + ":" +
                std::to_string(point_shadow_selected) + ":" +
                std::to_string(point_shadow_active) + ":" +
                point_shadow_reason,
            "point shadow status layer={} requested={} selected={} active={} reason={}",
            layer,
            point_shadow_requested,
            point_shadow_selected,
            point_shadow_active,
            point_shadow_reason);
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(point_shadow_selected) + ":" +
                std::to_string(point_shadow_dirty_faces) + ":" +
                std::to_string(point_shadow_updated_faces) + ":" +
                std::to_string(point_shadow_budget) + ":" +
                std::to_string(point_shadow_full_refresh ? 1 : 0) + ":" +
                std::to_string(point_shadow_structural_change ? 1 : 0) + ":" +
                std::to_string(moved_point_shadow_caster_bounds.size()),
            "point shadow refresh layer={} selected={} dirtyFaces={} updatedFaces={} budget={} fullRefresh={} structural={} movedCasters={}",
            layer,
            point_shadow_selected,
            point_shadow_dirty_faces,
            point_shadow_updated_faces,
            point_shadow_budget,
            point_shadow_full_refresh ? 1 : 0,
            point_shadow_structural_change ? 1 : 0,
            moved_point_shadow_caster_bounds.size());
        BgfxUniformSamplerHandles render_uniforms{};
        render_uniforms.u_color = u_color_;
        render_uniforms.u_light_dir = u_light_dir_;
        render_uniforms.u_light_color = u_light_color_;
        render_uniforms.u_ambient_color = u_ambient_color_;
        render_uniforms.u_unlit = u_unlit_;
        render_uniforms.u_texture_mode = u_texture_mode_;
        render_uniforms.u_shadow_params0 = u_shadow_params0_;
        render_uniforms.u_shadow_params1 = u_shadow_params1_;
        render_uniforms.u_shadow_params2 = u_shadow_params2_;
        render_uniforms.u_shadow_bias_params = u_shadow_bias_params_;
        render_uniforms.u_shadow_axis_right = u_shadow_axis_right_;
        render_uniforms.u_shadow_axis_up = u_shadow_axis_up_;
        render_uniforms.u_shadow_axis_forward = u_shadow_axis_forward_;
        render_uniforms.u_shadow_cascade_splits = u_shadow_cascade_splits_;
        render_uniforms.u_shadow_cascade_world_texel = u_shadow_cascade_world_texel_;
        render_uniforms.u_shadow_cascade_params = u_shadow_cascade_params_;
        render_uniforms.u_shadow_camera_position = u_shadow_camera_position_;
        render_uniforms.u_shadow_camera_forward = u_shadow_camera_forward_;
        render_uniforms.u_shadow_cascade_uv_proj = u_shadow_cascade_uv_proj_;
        render_uniforms.u_local_light_count = u_local_light_count_;
        render_uniforms.u_local_light_params = u_local_light_params_;
        render_uniforms.u_local_light_pos_range = u_local_light_pos_range_;
        render_uniforms.u_local_light_color_intensity = u_local_light_color_intensity_;
        render_uniforms.u_local_light_shadow_slot = u_local_light_shadow_slot_;
        render_uniforms.u_point_shadow_params = u_point_shadow_params_;
        render_uniforms.u_point_shadow_atlas_texel = u_point_shadow_atlas_texel_;
        render_uniforms.u_point_shadow_tuning = u_point_shadow_tuning_;
        render_uniforms.u_point_shadow_uv_proj = u_point_shadow_uv_proj_;
        render_uniforms.s_tex = s_tex_;
        render_uniforms.s_normal = s_normal_;
        render_uniforms.s_occlusion = s_occlusion_;
        render_uniforms.s_shadow = s_shadow_;
        render_uniforms.s_point_shadow = s_point_shadow_;

        BgfxRenderSubmissionInput render_input{};
        render_input.layer = layer;
        render_input.light = &light_;
        render_input.environment_semantics = &environment_semantics;
        render_input.renderables = &renderables;
        render_input.debug_lines = &debug_lines_;
        render_input.shadow_map = &shadow_map;
        render_input.supports_direct_multi_sampler_inputs = supports_direct_multi_sampler_inputs_;
        render_input.direct_sampler_disable_reason = &direct_sampler_disable_reason_;
        render_input.program = program_;
        render_input.layout = &layout_;
        render_input.uniforms = render_uniforms;
        render_input.white_tex = white_tex_;
        render_input.shadow_tex = shadow_tex_;
        render_input.shadow_tex_ready = shadow_tex_ready_;
        render_input.point_shadow_tex = point_shadow_tex_;
        render_input.point_shadow_tex_ready = point_shadow_tex_ready_;
        render_input.shadow_params0 = shadow_params0;
        render_input.shadow_params1 = shadow_params1;
        render_input.shadow_params2 = shadow_params2;
        render_input.shadow_bias_params = shadow_bias_params;
        render_input.shadow_axis_right = shadow_axis_right;
        render_input.shadow_axis_up = shadow_axis_up;
        render_input.shadow_axis_forward = shadow_axis_forward;
        render_input.shadow_cascade_splits_uniform = shadow_cascade_splits_uniform;
        render_input.shadow_cascade_world_texel_uniform = shadow_cascade_world_texel_uniform;
        render_input.shadow_cascade_params_uniform = shadow_cascade_params_uniform;
        render_input.shadow_camera_position = shadow_camera_position;
        render_input.shadow_camera_forward = shadow_camera_forward;
        render_input.shadow_cascade_uv_proj = &shadow_cascade_uv_proj;
        render_input.local_light_count_uniform = local_light_count_uniform;
        render_input.local_light_params_uniform = local_light_params_uniform;
        render_input.local_light_pos_range = &local_light_pos_range;
        render_input.local_light_color_intensity = &local_light_color_intensity;
        render_input.local_light_shadow_slot = &local_light_shadow_slot;
        render_input.point_shadow_params_uniform = point_shadow_params_uniform;
        render_input.point_shadow_atlas_texel_uniform = point_shadow_atlas_texel_uniform;
        render_input.point_shadow_tuning_uniform = point_shadow_tuning_uniform;
        render_input.point_shadow_uv_proj = &point_shadow_uv_proj;
        SubmitBgfxRenderLayerDraws(render_input);

    }

    void setCamera(const renderer::CameraData& camera) override {
        camera_ = camera;
    }

    void setDirectionalLight(const renderer::DirectionalLightData& light) override {
        light_ = light;
    }

    void setLights(const std::vector<renderer::LightData>& lights) override {
        lights_ = lights;
    }

    void setEnvironmentLighting(const renderer::EnvironmentLightingData& environment) override {
        environment_ = environment;
    }

    bool isValid() const override {
        return initialized_;
    }

 private:
    BgfxCallback callback_{};
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadow_depth_program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_dir_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ambient_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_unlit_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texture_mode_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params0_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params1_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params2_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_bias_params_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_right_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_up_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_forward_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_splits_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_world_texel_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_params_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_camera_position_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_camera_forward_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_uv_proj_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_count_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_params_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_pos_range_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_color_intensity_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_shadow_slot_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_params_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_atlas_texel_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_tuning_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_uv_proj_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_occlusion_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadow_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_point_shadow_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle white_tex_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle shadow_tex_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle point_shadow_tex_ = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle shadow_fb_ = BGFX_INVALID_HANDLE;
    uint16_t shadow_tex_size_ = 0u;
    uint16_t point_shadow_tex_width_ = 0u;
    uint16_t point_shadow_tex_height_ = 0u;
    bool shadow_tex_is_rt_ = false;
    bool shadow_tex_rt_uses_depth_ = false;
    bool shadow_depth_attachment_supported_ = false;
    bool shadow_tex_ready_ = false;
    bool point_shadow_tex_ready_ = false;
    detail::DirectionalShadowMap cached_shadow_map_{};
    detail::DirectionalShadowCascadeSet cached_shadow_cascades_{};
    detail::PointShadowMap cached_point_shadow_map_{};
    int shadow_update_every_frames_ = 1;
    int shadow_frames_until_update_ = 0;
    int point_shadow_frames_until_update_ = 0;
    std::array<int, kMaxPointShadowLights> point_shadow_slot_source_index_{{-1, -1, -1, -1}};
    std::array<glm::vec3, kMaxPointShadowLights> point_shadow_slot_position_{};
    std::array<float, kMaxPointShadowLights> point_shadow_slot_range_{};
    std::array<uint8_t, kMaxPointShadowMatrices> point_shadow_face_dirty_{};
    uint32_t point_shadow_face_cursor_ = 0u;
    bool point_shadow_cache_initialized_ = false;
    float point_shadow_position_threshold_ = 0.12f;
    float point_shadow_range_threshold_ = 0.05f;
    std::vector<renderer::DrawItem> previous_shadow_items_{};
    bool shadow_cache_inputs_valid_ = false;
    glm::vec3 cached_shadow_camera_position_{0.0f, 0.0f, 0.0f};
    glm::vec3 cached_shadow_camera_forward_{0.0f, 0.0f, -1.0f};
    glm::vec3 cached_shadow_light_direction_{0.0f, -1.0f, 0.0f};
    float cached_shadow_camera_aspect_ = 1.0f;
    float cached_shadow_camera_fov_y_degrees_ = 60.0f;
    float cached_shadow_camera_near_ = 0.1f;
    float cached_shadow_camera_far_ = 1000.0f;
    float directional_shadow_position_threshold_ = 0.12f;
    float directional_shadow_angle_threshold_deg_ = 0.3f;
    bgfx::VertexLayout layout_{};

    std::unordered_map<renderer::MeshId, Mesh> meshes_;
    std::unordered_map<renderer::MaterialId, Material> materials_;
    std::vector<renderer::DrawItem> draw_items_;
    std::vector<detail::ResolvedDebugLineSemantics> debug_lines_;

    renderer::CameraData camera_{};
    renderer::DirectionalLightData light_{};
    std::vector<renderer::LightData> lights_{};
    renderer::EnvironmentLightingData environment_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    BgfxDirectSamplerShaderAlignment direct_sampler_shader_alignment_{};
    detail::BgfxDirectSamplerContractReport direct_sampler_contract_report_{};
    std::string direct_sampler_disable_reason_ = "not_initialized";
    bool supports_direct_multi_sampler_inputs_ = false;
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateBgfxBackend(karma::platform::Window& window) {
    return std::make_unique<BgfxBackend>(window);
}

} // namespace karma::renderer_backend

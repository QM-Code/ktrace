#include "karma/renderer/backend.hpp"
#include "internal.hpp"

#include "../backend_factory_internal.hpp"
#include "../direct_sampler_observability_internal.hpp"
#include "../debug_line_internal.hpp"
#include "../directional_shadow_internal.hpp"
#include "../environment_lighting_internal.hpp"
#include "../material_lighting_internal.hpp"
#include "../material_semantics_internal.hpp"

#include "karma/common/logging.hpp"
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
constexpr std::size_t kDirectionalShadowCascadeCount =
    static_cast<std::size_t>(detail::kDirectionalShadowCascadeCount);
constexpr bgfx::ViewId kBgfxMainViewId =
    static_cast<bgfx::ViewId>(kBgfxShadowViewIdBase + kDirectionalShadowCascadeCount);
constexpr std::size_t kMaxLocalLights = 4u;
constexpr std::size_t kMaxPointShadowLights = kMaxLocalLights;
constexpr std::size_t kPointShadowFaceCount = static_cast<std::size_t>(detail::kPointShadowFaceCount);
constexpr std::size_t kMaxPointShadowMatrices = kMaxPointShadowLights * kPointShadowFaceCount;

struct PosNormalVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;

    static bgfx::VertexLayout layout() {
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        return layout;
    }
};

bool directionChangedBeyondThreshold(const glm::vec3& a, const glm::vec3& b, float threshold_deg) {
    const float a_len = glm::length(a);
    const float b_len = glm::length(b);
    if (!std::isfinite(a_len) || !std::isfinite(b_len) || a_len <= 1e-6f || b_len <= 1e-6f) {
        return true;
    }
    const glm::vec3 an = a / a_len;
    const glm::vec3 bn = b / b_len;
    const float cos_threshold = std::cos(glm::radians(std::max(0.0f, threshold_deg)));
    const float dot_v = glm::dot(an, bn);
    return dot_v < cos_threshold;
}

bool matrixChangedBeyondEpsilon(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (std::abs(a[col][row] - b[col][row]) > eps) {
                return true;
            }
        }
    }
    return false;
}

glm::vec3 resolveCameraForward(const renderer::CameraData& camera) {
    const glm::vec3 forward = camera.target - camera.position;
    const float len = glm::length(forward);
    if (!std::isfinite(len) || len <= 1e-6f) {
        return glm::vec3{0.0f, 0.0f, -1.0f};
    }
    return forward / len;
}

glm::vec3 normalizeDirectionOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
    const float len = glm::length(value);
    if (!std::isfinite(len) || len <= 1e-6f) {
        return fallback;
    }
    return value / len;
}

float maxTransformScale(const glm::mat4& transform) {
    const float sx = glm::length(glm::vec3(transform[0]));
    const float sy = glm::length(glm::vec3(transform[1]));
    const float sz = glm::length(glm::vec3(transform[2]));
    return std::max(sx, std::max(sy, sz));
}

void computeWorldShadowSphere(const glm::mat4& transform,
                              const glm::vec3& local_center,
                              float local_radius,
                              glm::vec3& out_center,
                              float& out_radius) {
    const glm::vec4 world_center = transform * glm::vec4(local_center, 1.0f);
    out_center = glm::vec3(world_center);
    const float scale = maxTransformScale(transform);
    out_radius = std::max(0.0f, local_radius * std::max(scale, 0.0f));
}

using Mesh = BgfxMesh;
using Material = BgfxMaterial;

struct RenderableDraw {
    const renderer::DrawItem* item = nullptr;
    Mesh* mesh = nullptr;
    Material* material = nullptr;
};

class BgfxCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override {
        spdlog::error("BGFX fatal: {}:{} code={} {}", filePath ? filePath : "(null)", line, static_cast<int>(code),
                      str ? str : "");
        std::abort();
    }

    void traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList) override {
        if (!karma::logging::ShouldTraceChannel("render.bgfx.internal")) {
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

std::vector<float> BuildShadowDepthUploadData(const detail::DirectionalShadowMap& map) {
    const std::size_t expected =
        static_cast<std::size_t>(map.size) * static_cast<std::size_t>(map.size);
    std::vector<float> pixels(expected, 1.0f);
    if (map.depth.size() != expected) {
        return pixels;
    }
    for (std::size_t i = 0; i < expected; ++i) {
        const float value = map.depth[i];
        pixels[i] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    }
    return pixels;
}

std::vector<float> BuildPointShadowDepthUploadData(const detail::PointShadowMap& map) {
    const std::size_t expected =
        static_cast<std::size_t>(std::max(map.atlas_width, 0)) *
        static_cast<std::size_t>(std::max(map.atlas_height, 0));
    std::vector<float> pixels(expected, 1.0f);
    if (map.depth.size() != expected) {
        return pixels;
    }
    for (std::size_t i = 0; i < expected; ++i) {
        const float value = map.depth[i];
        pixels[i] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    }
    return pixels;
}

uint32_t PackColorRgba8(const glm::vec4& color) {
    const auto to_u8 = [](float value) -> uint32_t {
        const float scaled = std::clamp(value, 0.0f, 1.0f) * 255.0f;
        return static_cast<uint32_t>(scaled + 0.5f);
    };
    return (to_u8(color.r) << 24u) |
           (to_u8(color.g) << 16u) |
           (to_u8(color.b) << 8u) |
           to_u8(color.a);
}

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
                           PackColorRgba8(clear_color),
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
        const int requested_update_every_frames = std::max(1, light_.shadow.update_every_frames);
        if (shadow_update_every_frames_ != requested_update_every_frames) {
            shadow_update_every_frames_ = requested_update_every_frames;
            shadow_frames_until_update_ = 0;
            point_shadow_frames_until_update_ = 0;
            KARMA_TRACE_CHANGED(
                "render.bgfx",
                std::to_string(shadow_update_every_frames_),
                "shadow update cadence everyFrames={}",
                shadow_update_every_frames_);
        }
        const glm::vec3 current_camera_forward = resolveCameraForward(camera_);
        const glm::vec3 current_light_direction =
            normalizeDirectionOrFallback(light_.direction, cached_shadow_light_direction_);
        bool shadow_inputs_changed = false;
        bool point_shadow_structural_change = false;
        std::vector<glm::vec4> moved_point_shadow_caster_bounds{};
        if (world_layer) {
            if (!shadow_cache_inputs_valid_) {
                shadow_inputs_changed = true;
                point_shadow_structural_change = true;
            } else if (glm::length(camera_.position - cached_shadow_camera_position_) >
                       directional_shadow_position_threshold_) {
                shadow_inputs_changed = true;
            } else if (directionChangedBeyondThreshold(
                           current_camera_forward,
                           cached_shadow_camera_forward_,
                           directional_shadow_angle_threshold_deg_)) {
                shadow_inputs_changed = true;
            } else if (directionChangedBeyondThreshold(
                           current_light_direction,
                           cached_shadow_light_direction_,
                           directional_shadow_angle_threshold_deg_)) {
                shadow_inputs_changed = true;
            } else if (std::abs(aspect - cached_shadow_camera_aspect_) > 1e-3f) {
                shadow_inputs_changed = true;
            } else if (std::abs(camera_.fov_y_degrees - cached_shadow_camera_fov_y_degrees_) > 1e-3f) {
                shadow_inputs_changed = true;
            } else if (std::abs(camera_.near_clip - cached_shadow_camera_near_) > 1e-4f) {
                shadow_inputs_changed = true;
            } else if (std::abs(camera_.far_clip - cached_shadow_camera_far_) > 1e-2f) {
                shadow_inputs_changed = true;
            } else if (current_shadow_items.size() != previous_shadow_items_.size()) {
                shadow_inputs_changed = true;
                point_shadow_structural_change = true;
            } else {
                for (std::size_t i = 0; i < current_shadow_items.size(); ++i) {
                    const renderer::DrawItem& curr = current_shadow_items[i];
                    const renderer::DrawItem& prev = previous_shadow_items_[i];
                    if (curr.mesh != prev.mesh ||
                        curr.material != prev.material ||
                        curr.layer != prev.layer ||
                        curr.casts_shadow != prev.casts_shadow) {
                        shadow_inputs_changed = true;
                        point_shadow_structural_change = true;
                        break;
                    }
                    if (matrixChangedBeyondEpsilon(curr.transform, prev.transform)) {
                        shadow_inputs_changed = true;
                        if (!curr.casts_shadow) {
                            continue;
                        }
                        auto mesh_it = meshes_.find(curr.mesh);
                        auto mat_it = materials_.find(curr.material);
                        if (mesh_it == meshes_.end() ||
                            mat_it == materials_.end() ||
                            mat_it->second.semantics.alpha_blend ||
                            mesh_it->second.shadow_radius <= 0.0f) {
                            point_shadow_structural_change = true;
                            continue;
                        }
                        glm::vec3 world_center{0.0f, 0.0f, 0.0f};
                        float world_radius = 0.0f;
                        computeWorldShadowSphere(
                            curr.transform,
                            mesh_it->second.shadow_center,
                            mesh_it->second.shadow_radius,
                            world_center,
                            world_radius);
                        moved_point_shadow_caster_bounds.emplace_back(world_center, world_radius);
                    }
                }
            }
        }
        if (world_layer &&
            shadow_cache_inputs_valid_ &&
            current_shadow_items.size() == previous_shadow_items_.size() &&
            moved_point_shadow_caster_bounds.empty() &&
            !point_shadow_structural_change) {
            for (std::size_t i = 0; i < current_shadow_items.size(); ++i) {
                const renderer::DrawItem& curr = current_shadow_items[i];
                const renderer::DrawItem& prev = previous_shadow_items_[i];
                if (curr.mesh != prev.mesh ||
                    curr.material != prev.material ||
                    curr.layer != prev.layer ||
                    curr.casts_shadow != prev.casts_shadow) {
                    point_shadow_structural_change = true;
                    break;
                }
                if (!matrixChangedBeyondEpsilon(curr.transform, prev.transform) ||
                    !curr.casts_shadow) {
                    continue;
                }
                auto mesh_it = meshes_.find(curr.mesh);
                auto mat_it = materials_.find(curr.material);
                if (mesh_it == meshes_.end() ||
                    mat_it == materials_.end() ||
                    mat_it->second.semantics.alpha_blend ||
                    mesh_it->second.shadow_radius <= 0.0f) {
                    point_shadow_structural_change = true;
                    continue;
                }
                glm::vec3 world_center{0.0f, 0.0f, 0.0f};
                float world_radius = 0.0f;
                computeWorldShadowSphere(
                    curr.transform,
                    mesh_it->second.shadow_center,
                    mesh_it->second.shadow_radius,
                    world_center,
                    world_radius);
                moved_point_shadow_caster_bounds.emplace_back(world_center, world_radius);
            }
        }
        if (world_layer &&
            (!shadow_cache_inputs_valid_ ||
             current_shadow_items.size() != previous_shadow_items_.size())) {
            point_shadow_structural_change = true;
        }
        const bool gpu_shadow_capable =
            bgfx::isValid(shadow_depth_program_) && world_layer && !shadow_casters.empty();
        const bool use_gpu_shadow_path =
            gpu_shadow_requested && shadow_semantics.enabled && gpu_shadow_capable;
        if (gpu_shadow_requested && shadow_semantics.enabled && world_layer && !use_gpu_shadow_path) {
            const char* reason = "gpu_shadow_pass_not_implemented";
            if (!bgfx::isValid(shadow_depth_program_)) {
                reason = "gpu_shadow_shader_unavailable";
            } else if (shadow_casters.empty()) {
                reason = "gpu_shadow_no_casters";
            }
            KARMA_TRACE_CHANGED(
                "render.bgfx",
                std::to_string(layer) + ":" + reason,
                "shadow execution mode requested={} active={} reason={}",
                renderer::DirectionalLightData::ShadowExecutionModeToken(light_.shadow.execution_mode),
                renderer::DirectionalLightData::ShadowExecutionModeToken(
                    renderer::DirectionalLightData::ShadowExecutionMode::CpuReference),
                reason);
        }
        if (!shadow_semantics.enabled) {
            shadow_frames_until_update_ = 0;
            cached_shadow_map_ = detail::DirectionalShadowMap{};
            cached_shadow_cascades_ = detail::DirectionalShadowCascadeSet{};
            shadow_tex_ready_ = false;
            shadow_cache_inputs_valid_ = false;
            previous_shadow_items_.clear();
        } else if (world_layer && shadow_casters.empty()) {
            shadow_frames_until_update_ = 0;
            cached_shadow_map_ = detail::DirectionalShadowMap{};
            cached_shadow_cascades_ = detail::DirectionalShadowCascadeSet{};
            shadow_tex_ready_ = false;
            shadow_cache_inputs_valid_ = false;
            previous_shadow_items_.clear();
        } else if (world_layer && !shadow_casters.empty()) {
            if (shadow_frames_until_update_ <= 0 ||
                shadow_inputs_changed ||
                !cached_shadow_map_.ready ||
                cached_shadow_map_.size != shadow_semantics.map_size) {
                if (use_gpu_shadow_path) {
                    cached_shadow_cascades_ = detail::BuildDirectionalShadowCascades(
                        shadow_semantics,
                        light_.direction,
                        shadow_casters,
                        shadow_view);
                    shadow_tex_ready_ = renderGpuShadowMap(cached_shadow_cascades_, renderables);
                    if (shadow_tex_ready_) {
                        cached_shadow_map_ = detail::BuildDirectionalShadowProjection(
                            shadow_semantics,
                            light_.direction,
                            shadow_casters,
                            &shadow_view);
                    } else {
                        cached_shadow_map_ = detail::BuildDirectionalShadowMap(
                            shadow_semantics,
                            light_.direction,
                            shadow_casters,
                            &shadow_view);
                        updateShadowTexture(cached_shadow_map_);
                        cached_shadow_cascades_ =
                            detail::BuildDirectionalShadowCascadeSetFromSingleMap(cached_shadow_map_);
                    }
                } else {
                    cached_shadow_map_ = detail::BuildDirectionalShadowMap(
                        shadow_semantics,
                        light_.direction,
                        shadow_casters,
                        &shadow_view);
                    updateShadowTexture(cached_shadow_map_);
                    cached_shadow_cascades_ =
                        detail::BuildDirectionalShadowCascadeSetFromSingleMap(cached_shadow_map_);
                }
                shadow_frames_until_update_ = shadow_update_every_frames_ - 1;
            } else {
                --shadow_frames_until_update_;
            }
            previous_shadow_items_ = std::move(current_shadow_items);
            cached_shadow_camera_position_ = camera_.position;
            cached_shadow_camera_forward_ = current_camera_forward;
            cached_shadow_light_direction_ = current_light_direction;
            cached_shadow_camera_aspect_ = aspect;
            cached_shadow_camera_fov_y_degrees_ = camera_.fov_y_degrees;
            cached_shadow_camera_near_ = camera_.near_clip;
            cached_shadow_camera_far_ = camera_.far_clip;
            shadow_cache_inputs_valid_ = true;
        }
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
        const int point_shadow_selected = static_cast<int>(point_shadow_selected_indices.size());
        std::array<uint8_t, kMaxPointShadowMatrices> point_shadow_faces_to_update{};
        point_shadow_faces_to_update.fill(0u);
        int point_shadow_dirty_faces = 0;
        int point_shadow_updated_faces = 0;
        const int point_shadow_budget = std::clamp(shadow_semantics.point_faces_per_frame_budget, 1, 24);
        bool point_shadow_full_refresh = false;
        if (!shadow_semantics.enabled ||
            shadow_semantics.point_max_shadow_lights <= 0 ||
            point_shadow_selected <= 0 ||
            (world_layer && shadow_casters.empty())) {
            resetPointShadowCacheState();
        } else if (world_layer) {
            const int active_face_count = point_shadow_selected * detail::kPointShadowFaceCount;
            const bool layout_compatible = detail::IsPointShadowMapLayoutCompatible(
                cached_point_shadow_map_,
                shadow_semantics.point_map_size,
                point_shadow_selected_indices);
            if (!layout_compatible) {
                point_shadow_cache_initialized_ = false;
                point_shadow_face_dirty_.fill(1u);
                point_shadow_full_refresh = true;
            }

            auto mark_point_shadow_slot_dirty = [&](int slot) {
                if (slot < 0 || slot >= static_cast<int>(kMaxPointShadowLights)) {
                    return;
                }
                const int face_base = slot * detail::kPointShadowFaceCount;
                for (int face = 0; face < detail::kPointShadowFaceCount; ++face) {
                    const int matrix_idx = face_base + face;
                    if (matrix_idx < 0 ||
                        matrix_idx >= static_cast<int>(kMaxPointShadowMatrices)) {
                        continue;
                    }
                    point_shadow_face_dirty_[static_cast<std::size_t>(matrix_idx)] = 1u;
                }
            };

            for (int slot = 0; slot < point_shadow_selected; ++slot) {
                const int source_idx = point_shadow_selected_indices[static_cast<std::size_t>(slot)];
                if (source_idx < 0 || source_idx >= static_cast<int>(lights_.size())) {
                    continue;
                }
                const renderer::LightData& light = lights_[static_cast<std::size_t>(source_idx)];
                const bool slot_identity_changed =
                    point_shadow_slot_source_index_[static_cast<std::size_t>(slot)] != source_idx;
                const bool slot_position_changed =
                    glm::length(light.position - point_shadow_slot_position_[static_cast<std::size_t>(slot)]) >
                    point_shadow_position_threshold_;
                const bool slot_range_changed =
                    std::fabs(light.range - point_shadow_slot_range_[static_cast<std::size_t>(slot)]) >
                    point_shadow_range_threshold_;
                if (!point_shadow_cache_initialized_ || slot_identity_changed) {
                    mark_point_shadow_slot_dirty(slot);
                    point_shadow_full_refresh = true;
                } else if (slot_position_changed || slot_range_changed) {
                    mark_point_shadow_slot_dirty(slot);
                }
                point_shadow_slot_source_index_[static_cast<std::size_t>(slot)] = source_idx;
                point_shadow_slot_position_[static_cast<std::size_t>(slot)] = light.position;
                point_shadow_slot_range_[static_cast<std::size_t>(slot)] = light.range;
            }
            for (int slot = point_shadow_selected;
                 slot < static_cast<int>(kMaxPointShadowLights);
                 ++slot) {
                point_shadow_slot_source_index_[static_cast<std::size_t>(slot)] = -1;
                point_shadow_slot_position_[static_cast<std::size_t>(slot)] = glm::vec3(0.0f, 0.0f, 0.0f);
                point_shadow_slot_range_[static_cast<std::size_t>(slot)] = 0.0f;
            }

            if (point_shadow_structural_change) {
                point_shadow_full_refresh = true;
                for (int slot = 0; slot < point_shadow_selected; ++slot) {
                    mark_point_shadow_slot_dirty(slot);
                }
            } else if (!moved_point_shadow_caster_bounds.empty()) {
                for (int slot = 0; slot < point_shadow_selected; ++slot) {
                    const int source_idx =
                        point_shadow_selected_indices[static_cast<std::size_t>(slot)];
                    if (source_idx < 0 || source_idx >= static_cast<int>(lights_.size())) {
                        continue;
                    }
                    const renderer::LightData& point_light =
                        lights_[static_cast<std::size_t>(source_idx)];
                    const float light_range = std::max(point_light.range, 0.0f);
                    bool affects_slot = false;
                    for (const glm::vec4& caster_sphere : moved_point_shadow_caster_bounds) {
                        const glm::vec3 delta = glm::vec3(caster_sphere) - point_light.position;
                        const float influence_radius = light_range + std::max(caster_sphere.w, 0.0f);
                        if (influence_radius <= 0.0f) {
                            continue;
                        }
                        if (glm::dot(delta, delta) <= influence_radius * influence_radius) {
                            affects_slot = true;
                            break;
                        }
                    }
                    if (affects_slot) {
                        mark_point_shadow_slot_dirty(slot);
                    }
                }
            }

            for (int idx = 0; idx < active_face_count; ++idx) {
                point_shadow_dirty_faces +=
                    point_shadow_face_dirty_[static_cast<std::size_t>(idx)] != 0u ? 1 : 0;
            }

            if (point_shadow_full_refresh) {
                for (int idx = 0; idx < active_face_count; ++idx) {
                    if (point_shadow_face_dirty_[static_cast<std::size_t>(idx)] == 0u) {
                        continue;
                    }
                    point_shadow_faces_to_update[static_cast<std::size_t>(idx)] = 1u;
                    ++point_shadow_updated_faces;
                }
            } else {
                int visited = 0;
                const uint32_t cursor_mod =
                    static_cast<uint32_t>(std::max(active_face_count, 1));
                while (point_shadow_updated_faces < point_shadow_budget &&
                       visited < active_face_count) {
                    const int matrix_idx =
                        static_cast<int>(point_shadow_face_cursor_ % cursor_mod);
                    point_shadow_face_cursor_ = (point_shadow_face_cursor_ + 1u) % cursor_mod;
                    ++visited;
                    if (point_shadow_face_dirty_[static_cast<std::size_t>(matrix_idx)] == 0u) {
                        continue;
                    }
                    point_shadow_faces_to_update[static_cast<std::size_t>(matrix_idx)] = 1u;
                    ++point_shadow_updated_faces;
                }
            }

            if (!layout_compatible ||
                point_shadow_updated_faces > 0 ||
                !point_shadow_cache_initialized_) {
                std::vector<uint8_t> face_update_mask(
                    static_cast<std::size_t>(active_face_count), 0u);
                for (int idx = 0; idx < active_face_count; ++idx) {
                    face_update_mask[static_cast<std::size_t>(idx)] =
                        point_shadow_faces_to_update[static_cast<std::size_t>(idx)];
                }
                cached_point_shadow_map_ = detail::BuildPointShadowMap(
                    shadow_semantics,
                    lights_,
                    shadow_casters,
                    layout_compatible ? &cached_point_shadow_map_ : nullptr,
                    &face_update_mask);
                updatePointShadowTexture(cached_point_shadow_map_);
                if (cached_point_shadow_map_.ready && point_shadow_tex_ready_) {
                    for (int idx = 0; idx < active_face_count; ++idx) {
                        if (face_update_mask[static_cast<std::size_t>(idx)] != 0u) {
                            point_shadow_face_dirty_[static_cast<std::size_t>(idx)] = 0u;
                        }
                    }
                    point_shadow_cache_initialized_ = true;
                } else {
                    point_shadow_cache_initialized_ = false;
                }
            }
        }
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
        std::size_t direct_sampler_draws = 0u;
        std::size_t fallback_sampler_draws = 0u;
        std::size_t direct_contract_draws = 0u;
        std::size_t forced_fallback_draws = 0u;
        std::size_t unexpected_direct_draws = 0u;

        for (const RenderableDraw& renderable : renderables) {
            const auto& item = *renderable.item;
            const auto& semantics = renderable.material->semantics;

            bgfx::setVertexBuffer(0, renderable.mesh->vbh);
            bgfx::setIndexBuffer(renderable.mesh->ibh);
            bgfx::setTransform(&item.transform[0][0]);

            const detail::ResolvedMaterialLighting lighting =
                detail::ResolveMaterialLighting(semantics, light_, environment_semantics, 1.0f);
            if (!detail::ValidateResolvedMaterialLighting(lighting)) {
                continue;
            }

            const glm::vec3 lighting_direction = -light_.direction;
            const float light_dir[4] = {lighting_direction.x, lighting_direction.y, lighting_direction.z, 0.0f};
            const float material_color[4] = {
                lighting.color.r,
                lighting.color.g,
                lighting.color.b,
                lighting.color.a};
            const float light_color[4] = {
                lighting.light_color.r,
                lighting.light_color.g,
                lighting.light_color.b,
                lighting.light_color.a};
            const float ambient_color[4] = {
                lighting.ambient_color.r,
                lighting.ambient_color.g,
                lighting.ambient_color.b,
                lighting.ambient_color.a};
            const float unlit[4] = {light_.unlit, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(u_color_, material_color);
            bgfx::setUniform(u_light_dir_, light_dir);
            bgfx::setUniform(u_light_color_, light_color);
            bgfx::setUniform(u_ambient_color_, ambient_color);
            bgfx::setUniform(u_unlit_, unlit);
            bgfx::setUniform(u_shadow_params0_, shadow_params0);
            bgfx::setUniform(u_shadow_params1_, shadow_params1);
            bgfx::setUniform(u_shadow_params2_, shadow_params2);
            bgfx::setUniform(u_shadow_bias_params_, shadow_bias_params);
            bgfx::setUniform(u_shadow_axis_right_, shadow_axis_right);
            bgfx::setUniform(u_shadow_axis_up_, shadow_axis_up);
            bgfx::setUniform(u_shadow_axis_forward_, shadow_axis_forward);
            if (bgfx::isValid(u_shadow_cascade_splits_)) {
                bgfx::setUniform(u_shadow_cascade_splits_, shadow_cascade_splits_uniform);
            }
            if (bgfx::isValid(u_shadow_cascade_world_texel_)) {
                bgfx::setUniform(u_shadow_cascade_world_texel_, shadow_cascade_world_texel_uniform);
            }
            if (bgfx::isValid(u_shadow_cascade_params_)) {
                bgfx::setUniform(u_shadow_cascade_params_, shadow_cascade_params_uniform);
            }
            if (bgfx::isValid(u_shadow_camera_position_)) {
                bgfx::setUniform(u_shadow_camera_position_, shadow_camera_position);
            }
            if (bgfx::isValid(u_shadow_camera_forward_)) {
                bgfx::setUniform(u_shadow_camera_forward_, shadow_camera_forward);
            }
            if (bgfx::isValid(u_shadow_cascade_uv_proj_)) {
                bgfx::setUniform(
                    u_shadow_cascade_uv_proj_,
                    shadow_cascade_uv_proj.data(),
                    static_cast<uint16_t>(kDirectionalShadowCascadeCount));
            }
            if (bgfx::isValid(u_local_light_count_)) {
                bgfx::setUniform(u_local_light_count_, local_light_count_uniform);
            }
            if (bgfx::isValid(u_local_light_params_)) {
                bgfx::setUniform(u_local_light_params_, local_light_params_uniform);
            }
            if (bgfx::isValid(u_local_light_pos_range_)) {
                bgfx::setUniform(
                    u_local_light_pos_range_,
                    local_light_pos_range.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_local_light_color_intensity_)) {
                bgfx::setUniform(
                    u_local_light_color_intensity_,
                    local_light_color_intensity.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_local_light_shadow_slot_)) {
                bgfx::setUniform(
                    u_local_light_shadow_slot_,
                    local_light_shadow_slot.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_point_shadow_params_)) {
                bgfx::setUniform(u_point_shadow_params_, point_shadow_params_uniform);
            }
            if (bgfx::isValid(u_point_shadow_atlas_texel_)) {
                bgfx::setUniform(u_point_shadow_atlas_texel_, point_shadow_atlas_texel_uniform);
            }
            if (bgfx::isValid(u_point_shadow_tuning_)) {
                bgfx::setUniform(u_point_shadow_tuning_, point_shadow_tuning_uniform);
            }
            if (bgfx::isValid(u_point_shadow_uv_proj_)) {
                bgfx::setUniform(
                    u_point_shadow_uv_proj_,
                    point_shadow_uv_proj.data(),
                    static_cast<uint16_t>(kMaxPointShadowMatrices));
            }

            const bool use_direct_sampler_path =
                supports_direct_multi_sampler_inputs_ &&
                renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
            const bool contract_requests_direct =
                renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
            if (contract_requests_direct) {
                ++direct_contract_draws;
            }
            if (use_direct_sampler_path) {
                ++direct_sampler_draws;
            } else {
                ++fallback_sampler_draws;
            }
            if (use_direct_sampler_path && !contract_requests_direct) {
                ++unexpected_direct_draws;
            }
            if (!use_direct_sampler_path && contract_requests_direct) {
                ++forced_fallback_draws;
            }
            bgfx::TextureHandle direct_albedo_texture = white_tex_;
            if (bgfx::isValid(renderable.material->tex)) {
                direct_albedo_texture = renderable.material->tex;
            }
            bgfx::TextureHandle fallback_base_texture = white_tex_;
            if (bgfx::isValid(renderable.material->tex)) {
                fallback_base_texture = renderable.material->tex;
            } else if (bgfx::isValid(renderable.mesh->tex)) {
                fallback_base_texture = renderable.mesh->tex;
            }
            const bgfx::TextureHandle base_texture = use_direct_sampler_path ? direct_albedo_texture : fallback_base_texture;
            if (bgfx::isValid(base_texture)) {
                bgfx::setTexture(0, s_tex_, base_texture);
            }
            if (bgfx::isValid(s_normal_)) {
                const bgfx::TextureHandle normal_texture =
                    (use_direct_sampler_path && bgfx::isValid(renderable.material->normal_tex))
                        ? renderable.material->normal_tex
                        : white_tex_;
                if (bgfx::isValid(normal_texture)) {
                    bgfx::setTexture(1, s_normal_, normal_texture);
                }
            }
            if (bgfx::isValid(s_occlusion_)) {
                const bgfx::TextureHandle occlusion_texture =
                    (use_direct_sampler_path && bgfx::isValid(renderable.material->occlusion_tex))
                        ? renderable.material->occlusion_tex
                        : white_tex_;
                if (bgfx::isValid(occlusion_texture)) {
                    bgfx::setTexture(2, s_occlusion_, occlusion_texture);
                }
            }
            if (bgfx::isValid(s_shadow_)) {
                const bgfx::TextureHandle shadow_texture =
                    shadow_tex_ready_ ? shadow_tex_ : white_tex_;
                if (bgfx::isValid(shadow_texture)) {
                    bgfx::setTexture(3, s_shadow_, shadow_texture);
                }
            }
            if (bgfx::isValid(s_point_shadow_)) {
                const bgfx::TextureHandle point_shadow_texture =
                    point_shadow_tex_ready_ ? point_shadow_tex_ : white_tex_;
                if (bgfx::isValid(point_shadow_texture)) {
                    bgfx::setTexture(4, s_point_shadow_, point_shadow_texture);
                }
            }
            if (bgfx::isValid(u_texture_mode_)) {
                const float texture_mode[4] = {
                    (use_direct_sampler_path && renderable.material->shader_uses_normal_input) ? 1.0f : 0.0f,
                    (use_direct_sampler_path && renderable.material->shader_uses_occlusion_input) ? 1.0f : 0.0f,
                    0.0f,
                    0.0f,
                };
                bgfx::setUniform(u_texture_mode_, texture_mode);
            }
            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
            if (!semantics.double_sided) {
                state |= BGFX_STATE_CULL_CW;
            }
            if (semantics.alpha_blend) {
                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            }
            bgfx::setState(state);
            bgfx::submit(kBgfxMainViewId, program_);
        }
        if (shadow_map.ready) {
            const char* shadow_source = shadow_map.depth.empty() ? "gpu_projection" : "cpu_raster";
            int shadow_covered = -1;
            if (!shadow_map.depth.empty()) {
                int covered = 0;
                for (float value : shadow_map.depth) {
                    if (std::isfinite(value)) {
                        ++covered;
                    }
                }
                shadow_covered = covered;
            }
            KARMA_TRACE_CHANGED(
                "render.bgfx",
                std::to_string(layer) + ":" +
                    std::to_string(shadow_map.ready ? 1 : 0) + ":" +
                    shadow_source + ":" +
                    std::to_string(shadow_covered) + ":" +
                    std::to_string(shadow_map.size) + ":" +
                    std::to_string(shadow_tex_ready_ ? 1 : 0),
                "shadow map layer={} source={} mapReady={} covered={} size={} extent={:.1f} uploaded={}",
                layer,
                shadow_source,
                shadow_map.ready ? 1 : 0,
                shadow_covered,
                shadow_map.size,
                shadow_map.extent,
                shadow_tex_ready_ ? 1 : 0);
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(renderables.size()) + ":" +
                std::to_string(direct_sampler_draws) + ":" +
                std::to_string(fallback_sampler_draws) + ":" +
                std::to_string(supports_direct_multi_sampler_inputs_ ? 1 : 0) + ":" +
                direct_sampler_disable_reason_,
            "direct sampler frame layer={} draws={} direct={} fallback={} enabled={} reason={}",
            layer,
            renderables.size(),
            direct_sampler_draws,
            fallback_sampler_draws,
            supports_direct_multi_sampler_inputs_ ? 1 : 0,
            direct_sampler_disable_reason_);
        const detail::DirectSamplerDrawInvariantReport draw_invariants =
            detail::EvaluateDirectSamplerDrawInvariants(
                detail::DirectSamplerDrawInvariantInput{
                    supports_direct_multi_sampler_inputs_,
                    renderables.size(),
                    direct_contract_draws,
                    direct_sampler_draws,
                    fallback_sampler_draws,
                    forced_fallback_draws,
                    unexpected_direct_draws,
                });
        if (!draw_invariants.ok) {
            spdlog::error(
                "Graphics(Bgfx): direct sampler assertion failed (enabled={}, directContract={}, directDraws={}, fallbackDraws={}, forcedFallback={}, unexpectedDirect={}, reason={}, invariant={})",
                supports_direct_multi_sampler_inputs_ ? 1 : 0,
                direct_contract_draws,
                direct_sampler_draws,
                fallback_sampler_draws,
                forced_fallback_draws,
                unexpected_direct_draws,
                direct_sampler_disable_reason_,
                draw_invariants.reason);
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(renderables.size()) + ":" +
                std::to_string(direct_contract_draws) + ":" +
                std::to_string(direct_sampler_draws) + ":" +
                std::to_string(fallback_sampler_draws) + ":" +
                std::to_string(forced_fallback_draws) + ":" +
                std::to_string(unexpected_direct_draws) + ":" +
                std::to_string(draw_invariants.ok ? 1 : 0) + ":" +
                draw_invariants.reason,
            "direct sampler assertions layer={} draws={} contractDirect={} actualDirect={} fallback={} forcedFallback={} unexpectedDirect={} ok={} enabled={} reason={} invariant={}",
            layer,
            renderables.size(),
            direct_contract_draws,
            direct_sampler_draws,
            fallback_sampler_draws,
            forced_fallback_draws,
            unexpected_direct_draws,
            draw_invariants.ok ? 1 : 0,
            supports_direct_multi_sampler_inputs_ ? 1 : 0,
            direct_sampler_disable_reason_,
            draw_invariants.reason);

        for (const auto& line : debug_lines_) {
            if (line.layer != layer) {
                continue;
            }

            if (bgfx::getAvailTransientVertexBuffer(2, layout_) < 2) {
                continue;
            }
            bgfx::TransientVertexBuffer tvb{};
            bgfx::allocTransientVertexBuffer(&tvb, 2, layout_);

            PosNormalVertex vertices[2]{
                {line.start.x, line.start.y, line.start.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
                {line.end.x, line.end.y, line.end.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
            };
            std::memcpy(tvb.data, vertices, sizeof(vertices));
            bgfx::setVertexBuffer(0, &tvb, 0, 2);

            const float line_color[4] = {line.color.r, line.color.g, line.color.b, line.color.a};
            const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            const float unlit[4] = {1.0f, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(u_color_, line_color);
            bgfx::setUniform(u_light_dir_, zeros);
            bgfx::setUniform(u_light_color_, zeros);
            bgfx::setUniform(u_ambient_color_, zeros);
            bgfx::setUniform(u_unlit_, unlit);
            bgfx::setUniform(u_shadow_params0_, zeros);
            bgfx::setUniform(u_shadow_params1_, zeros);
            bgfx::setUniform(u_shadow_params2_, zeros);
            bgfx::setUniform(u_shadow_bias_params_, zeros);
            bgfx::setUniform(u_shadow_axis_right_, zeros);
            bgfx::setUniform(u_shadow_axis_up_, zeros);
            bgfx::setUniform(u_shadow_axis_forward_, zeros);
            if (bgfx::isValid(u_shadow_cascade_splits_)) {
                bgfx::setUniform(u_shadow_cascade_splits_, zeros);
            }
            if (bgfx::isValid(u_shadow_cascade_world_texel_)) {
                bgfx::setUniform(u_shadow_cascade_world_texel_, zeros);
            }
            if (bgfx::isValid(u_shadow_cascade_params_)) {
                bgfx::setUniform(u_shadow_cascade_params_, zeros);
            }
            if (bgfx::isValid(u_shadow_camera_position_)) {
                bgfx::setUniform(u_shadow_camera_position_, zeros);
            }
            if (bgfx::isValid(u_shadow_camera_forward_)) {
                bgfx::setUniform(u_shadow_camera_forward_, zeros);
            }
            if (bgfx::isValid(u_shadow_cascade_uv_proj_)) {
                std::array<glm::mat4, kDirectionalShadowCascadeCount> shadow_cascade_mats{};
                for (glm::mat4& matrix : shadow_cascade_mats) {
                    matrix = glm::mat4(1.0f);
                }
                bgfx::setUniform(
                    u_shadow_cascade_uv_proj_,
                    shadow_cascade_mats.data(),
                    static_cast<uint16_t>(kDirectionalShadowCascadeCount));
            }
            if (bgfx::isValid(u_local_light_count_)) {
                bgfx::setUniform(u_local_light_count_, zeros);
            }
            if (bgfx::isValid(u_local_light_params_)) {
                bgfx::setUniform(u_local_light_params_, zeros);
            }
            if (bgfx::isValid(u_local_light_pos_range_)) {
                const std::array<float, kMaxLocalLights * 4u> light_zero{};
                bgfx::setUniform(
                    u_local_light_pos_range_,
                    light_zero.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_local_light_color_intensity_)) {
                const std::array<float, kMaxLocalLights * 4u> light_zero{};
                bgfx::setUniform(
                    u_local_light_color_intensity_,
                    light_zero.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_local_light_shadow_slot_)) {
                std::array<float, kMaxLocalLights * 4u> shadow_slot_zero{};
                for (std::size_t slot = 0; slot < kMaxLocalLights; ++slot) {
                    shadow_slot_zero[slot * 4u] = -1.0f;
                }
                bgfx::setUniform(
                    u_local_light_shadow_slot_,
                    shadow_slot_zero.data(),
                    static_cast<uint16_t>(kMaxLocalLights));
            }
            if (bgfx::isValid(u_point_shadow_params_)) {
                bgfx::setUniform(u_point_shadow_params_, zeros);
            }
            if (bgfx::isValid(u_point_shadow_atlas_texel_)) {
                bgfx::setUniform(u_point_shadow_atlas_texel_, zeros);
            }
            if (bgfx::isValid(u_point_shadow_tuning_)) {
                bgfx::setUniform(u_point_shadow_tuning_, zeros);
            }
            if (bgfx::isValid(u_point_shadow_uv_proj_)) {
                std::array<glm::mat4, kMaxPointShadowMatrices> point_shadow_mats{};
                for (glm::mat4& matrix : point_shadow_mats) {
                    matrix = glm::mat4(1.0f);
                }
                bgfx::setUniform(
                    u_point_shadow_uv_proj_,
                    point_shadow_mats.data(),
                    static_cast<uint16_t>(kMaxPointShadowMatrices));
            }
            if (bgfx::isValid(u_texture_mode_)) {
                const float texture_mode[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                bgfx::setUniform(u_texture_mode_, texture_mode);
            }
            if (bgfx::isValid(white_tex_)) {
                bgfx::setTexture(0, s_tex_, white_tex_);
                if (bgfx::isValid(s_normal_)) {
                    bgfx::setTexture(1, s_normal_, white_tex_);
                }
                if (bgfx::isValid(s_occlusion_)) {
                    bgfx::setTexture(2, s_occlusion_, white_tex_);
                }
                if (bgfx::isValid(s_shadow_) && bgfx::isValid(shadow_tex_)) {
                    bgfx::setTexture(3, s_shadow_, shadow_tex_);
                }
                if (bgfx::isValid(s_point_shadow_) && bgfx::isValid(point_shadow_tex_)) {
                    bgfx::setTexture(4, s_point_shadow_, point_shadow_tex_);
                }
            }

            float identity[16];
            bx::mtxIdentity(identity);
            bgfx::setTransform(identity);

            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                             BGFX_STATE_PT_LINES | BGFX_STATE_MSAA;
            if (line.color.a < 0.999f) {
                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            }
            bgfx::setState(state);
            bgfx::submit(kBgfxMainViewId, program_);
        }

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
    void resetPointShadowCacheState() {
        cached_point_shadow_map_ = detail::PointShadowMap{};
        point_shadow_tex_ready_ = false;
        point_shadow_frames_until_update_ = 0;
        point_shadow_cache_initialized_ = false;
        point_shadow_face_cursor_ = 0u;
        point_shadow_face_dirty_.fill(1u);
        point_shadow_slot_source_index_.fill(-1);
        point_shadow_slot_position_.fill(glm::vec3(0.0f, 0.0f, 0.0f));
        point_shadow_slot_range_.fill(0.0f);
    }

    bool ensureShadowTexture(uint16_t size, bool render_target) {
        if (size == 0u) {
            return false;
        }
        const bool prefer_depth_attachment = render_target && shadow_depth_attachment_supported_;
        const bool needs_recreate =
            !bgfx::isValid(shadow_tex_) ||
            shadow_tex_size_ != size ||
            shadow_tex_is_rt_ != render_target ||
            (render_target && (shadow_tex_rt_uses_depth_ != prefer_depth_attachment));
        if (needs_recreate) {
            if (bgfx::isValid(shadow_fb_)) {
                bgfx::destroy(shadow_fb_);
                shadow_fb_ = BGFX_INVALID_HANDLE;
            }
            if (bgfx::isValid(shadow_tex_)) {
                bgfx::destroy(shadow_tex_);
                shadow_tex_ = BGFX_INVALID_HANDLE;
            }
            shadow_tex_rt_uses_depth_ = false;

            const auto create_shadow_texture = [&](bgfx::TextureFormat::Enum format, bool rt_uses_depth) -> bool {
                uint64_t flags =
                    BGFX_SAMPLER_U_CLAMP |
                    BGFX_SAMPLER_V_CLAMP |
                    BGFX_SAMPLER_COMPARE_LEQUAL;
                if (render_target) {
                    flags |= BGFX_TEXTURE_RT;
                }
                shadow_tex_ = bgfx::createTexture2D(
                    size,
                    size,
                    false,
                    1,
                    format,
                    flags,
                    nullptr);
                if (!bgfx::isValid(shadow_tex_)) {
                    return false;
                }
                shadow_tex_size_ = size;
                shadow_tex_is_rt_ = render_target;
                shadow_tex_rt_uses_depth_ = render_target && rt_uses_depth;
                if (render_target) {
                    bgfx::TextureHandle attachments[] = {shadow_tex_};
                    shadow_fb_ = bgfx::createFrameBuffer(1, attachments, false);
                    if (!bgfx::isValid(shadow_fb_)) {
                        bgfx::destroy(shadow_tex_);
                        shadow_tex_ = BGFX_INVALID_HANDLE;
                        shadow_tex_size_ = 0u;
                        shadow_tex_is_rt_ = false;
                        shadow_tex_rt_uses_depth_ = false;
                        return false;
                    }
                }
                return true;
            };

            bool created = false;
            if (render_target && prefer_depth_attachment) {
                created = create_shadow_texture(bgfx::TextureFormat::D32F, true);
                if (!created) {
                    KARMA_TRACE("render.bgfx", "shadow depth attachment create failed; using color-min fallback");
                }
            }
            if (!created) {
                created = create_shadow_texture(bgfx::TextureFormat::D32F, false);
            }
            if (!created) {
                created = create_shadow_texture(bgfx::TextureFormat::R32F, false);
            }
            if (!created) {
                shadow_tex_size_ = 0u;
                shadow_tex_is_rt_ = false;
                shadow_tex_rt_uses_depth_ = false;
            }
        }

        if (!bgfx::isValid(shadow_tex_)) {
            return false;
        }
        if (render_target && !bgfx::isValid(shadow_fb_)) {
            return false;
        }
        return true;
    }

    bool ensurePointShadowTexture(uint16_t width, uint16_t height) {
        if (width == 0u || height == 0u) {
            return false;
        }
        const bool needs_recreate =
            !bgfx::isValid(point_shadow_tex_) ||
            point_shadow_tex_width_ != width ||
            point_shadow_tex_height_ != height;
        if (needs_recreate) {
            if (bgfx::isValid(point_shadow_tex_)) {
                bgfx::destroy(point_shadow_tex_);
                point_shadow_tex_ = BGFX_INVALID_HANDLE;
            }
            const uint64_t flags =
                BGFX_SAMPLER_U_CLAMP |
                BGFX_SAMPLER_V_CLAMP |
                BGFX_SAMPLER_COMPARE_LEQUAL;
            point_shadow_tex_ = bgfx::createTexture2D(
                width,
                height,
                false,
                1,
                bgfx::TextureFormat::D32F,
                flags,
                nullptr);
            if (!bgfx::isValid(point_shadow_tex_)) {
                point_shadow_tex_width_ = 0u;
                point_shadow_tex_height_ = 0u;
                return false;
            }
            point_shadow_tex_width_ = width;
            point_shadow_tex_height_ = height;
        }
        return bgfx::isValid(point_shadow_tex_);
    }

    bool renderGpuShadowMap(const detail::DirectionalShadowCascadeSet& cascades,
                            const std::vector<RenderableDraw>& renderables) {
        if (!cascades.ready ||
            cascades.map_size <= 0 ||
            cascades.atlas_size <= 0 ||
            !bgfx::isValid(shadow_depth_program_)) {
            return false;
        }
        if (cascades.map_size > static_cast<int>(std::numeric_limits<uint16_t>::max()) ||
            cascades.atlas_size > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
            return false;
        }
        const uint16_t cascade_size = static_cast<uint16_t>(cascades.map_size);
        const uint16_t atlas_size = static_cast<uint16_t>(cascades.atlas_size);
        if (!ensureShadowTexture(atlas_size, true)) {
            return false;
        }

        const bgfx::Caps* caps = bgfx::getCaps();
        const detail::ShadowClipDepthTransform clip_depth =
            detail::ResolveShadowClipDepthTransform(caps && caps->homogeneousDepth);
        const float clip_y_sign =
            detail::ResolveShadowClipYSign(caps && caps->originBottomLeft);
        const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
            detail::ResolveDirectionalShadowSemantics(light_);
        const bool depth_attachment = shadow_tex_rt_uses_depth_;
        const int cascade_count = std::clamp(
            cascades.cascade_count,
            1,
            static_cast<int>(kDirectionalShadowCascadeCount));

        std::size_t submitted = 0u;
        std::size_t culled = 0u;
        for (int cascade_idx = 0; cascade_idx < cascade_count; ++cascade_idx) {
            const detail::DirectionalShadowCascade& cascade =
                cascades.cascades[static_cast<std::size_t>(cascade_idx)];
            if (!cascade.ready) {
                continue;
            }
            const bgfx::ViewId view_id =
                static_cast<bgfx::ViewId>(kBgfxShadowViewIdBase + cascade_idx);
            const uint16_t view_x = static_cast<uint16_t>(
                std::clamp<int>(static_cast<int>(std::floor(cascade.atlas_offset.x *
                                                            static_cast<float>(atlas_size) + 0.5f)),
                                0,
                                std::max(static_cast<int>(atlas_size) - 1, 0)));
            const uint16_t view_y = static_cast<uint16_t>(
                std::clamp<int>(static_cast<int>(std::floor(cascade.atlas_offset.y *
                                                            static_cast<float>(atlas_size) + 0.5f)),
                                0,
                                std::max(static_cast<int>(atlas_size) - 1, 0)));
            bgfx::setViewFrameBuffer(view_id, shadow_fb_);
            bgfx::setViewRect(view_id, view_x, view_y, cascade_size, cascade_size);
            bgfx::setViewTransform(view_id, nullptr, nullptr);
            bgfx::setViewClear(
                view_id,
                depth_attachment ? BGFX_CLEAR_DEPTH : BGFX_CLEAR_COLOR,
                0xffffffff,
                1.0f,
                0);
            bgfx::touch(view_id);

            const float inv_depth_range =
                cascade.depth_range > 1e-6f ? (1.0f / cascade.depth_range) : 1.0f;
            const float shadow_params1[4] = {
                cascade.extent,
                cascade.center_x,
                cascade.center_y,
                cascade.min_depth,
            };
            const float shadow_params0[4] = {
                1.0f,
                cascades.strength,
                cascades.bias,
                cascades.map_size > 0
                    ? (1.0f / static_cast<float>(cascades.map_size))
                    : 0.0f,
            };
            const float shadow_params2[4] = {
                inv_depth_range,
                static_cast<float>(cascades.pcf_radius),
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
                cascades.axis_right.x,
                cascades.axis_right.y,
                cascades.axis_right.z,
                0.0f,
            };
            const float shadow_axis_up[4] = {
                cascades.axis_up.x,
                cascades.axis_up.y,
                cascades.axis_up.z,
                clip_y_sign,
            };
            const float shadow_axis_forward[4] = {
                cascades.axis_forward.x,
                cascades.axis_forward.y,
                cascades.axis_forward.z,
                0.0f,
            };
            bgfx::setUniform(u_shadow_params0_, shadow_params0);
            bgfx::setUniform(u_shadow_params1_, shadow_params1);
            bgfx::setUniform(u_shadow_params2_, shadow_params2);
            bgfx::setUniform(u_shadow_bias_params_, shadow_bias_params);
            bgfx::setUniform(u_shadow_axis_right_, shadow_axis_right);
            bgfx::setUniform(u_shadow_axis_up_, shadow_axis_up);
            bgfx::setUniform(u_shadow_axis_forward_, shadow_axis_forward);

            for (const RenderableDraw& renderable : renderables) {
                const auto& item = *renderable.item;
                const auto& semantics = renderable.material->semantics;
                if (!item.casts_shadow || semantics.alpha_blend || !semantics.draw) {
                    continue;
                }
                if (renderable.mesh->shadow_radius > 0.0f) {
                    glm::vec3 world_center{0.0f, 0.0f, 0.0f};
                    float world_radius = 0.0f;
                    computeWorldShadowSphere(
                        item.transform,
                        renderable.mesh->shadow_center,
                        renderable.mesh->shadow_radius,
                        world_center,
                        world_radius);
                    if (!detail::IntersectsDirectionalShadowCascade(
                            cascades,
                            cascade_idx,
                            world_center,
                            world_radius)) {
                        ++culled;
                        continue;
                    }
                }

                bgfx::setVertexBuffer(0, renderable.mesh->vbh);
                bgfx::setIndexBuffer(renderable.mesh->ibh);
                bgfx::setTransform(&item.transform[0][0]);
                uint64_t state = BGFX_STATE_MSAA;
                if (depth_attachment) {
                    state |= BGFX_STATE_WRITE_Z |
                        BGFX_STATE_DEPTH_TEST_LESS;
                } else {
                    state |= BGFX_STATE_WRITE_RGB |
                        BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
                        BGFX_STATE_BLEND_EQUATION_MIN;
                }
                bgfx::setState(state);
                bgfx::submit(view_id, shadow_depth_program_);
                ++submitted;
            }
        }

        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(cascades.map_size) + ":" +
                std::to_string(cascade_count) + ":" +
                std::to_string(submitted) + ":" +
                std::to_string(culled) + ":" +
                (depth_attachment ? "depth" : "color_min"),
            "gpu shadow pass size={} cascades={} draws={} culled={} attachment={}",
            cascades.map_size,
            cascade_count,
            submitted,
            culled,
            depth_attachment ? "depth" : "color_min");
        return submitted > 0u;
    }

    void updateShadowTexture(const detail::DirectionalShadowMap& map) {
        shadow_tex_ready_ = false;
        if (!map.ready || map.size <= 0 || map.depth.empty()) {
            return;
        }
        const uint16_t size = static_cast<uint16_t>(map.size);
        if (!ensureShadowTexture(size, false)) {
            return;
        }

        std::vector<float> upload = BuildShadowDepthUploadData(map);
        if (upload.empty()) {
            return;
        }
        const bgfx::Memory* mem =
            bgfx::copy(upload.data(), static_cast<uint32_t>(upload.size() * sizeof(float)));
        bgfx::updateTexture2D(shadow_tex_, 0, 0, 0, 0, size, size, mem);
        shadow_tex_ready_ = true;
    }

    void updatePointShadowTexture(const detail::PointShadowMap& map) {
        point_shadow_tex_ready_ = false;
        if (!map.ready || map.atlas_width <= 0 || map.atlas_height <= 0 || map.depth.empty()) {
            return;
        }
        if (map.atlas_width > static_cast<int>(std::numeric_limits<uint16_t>::max()) ||
            map.atlas_height > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
            return;
        }
        const uint16_t width = static_cast<uint16_t>(map.atlas_width);
        const uint16_t height = static_cast<uint16_t>(map.atlas_height);
        if (!ensurePointShadowTexture(width, height)) {
            return;
        }
        std::vector<float> upload = BuildPointShadowDepthUploadData(map);
        if (upload.empty()) {
            return;
        }
        const bgfx::Memory* mem =
            bgfx::copy(upload.data(), static_cast<uint32_t>(upload.size() * sizeof(float)));
        bgfx::updateTexture2D(point_shadow_tex_, 0, 0, 0, 0, width, height, mem);
        point_shadow_tex_ready_ = true;
    }

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

#include "karma/renderer/backend.hpp"
#include "internal.hpp"

#include "../internal/backend_factory.hpp"
#include "../internal/direct_sampler_observability.hpp"
#include "../internal/debug_line.hpp"
#include "../internal/directional_shadow.hpp"
#include "../internal/environment_lighting.hpp"
#include "../internal/material_semantics.hpp"

#include "karma/common/logging.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/layers.hpp"

#include <spdlog/spdlog.h>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Platforms/interface/NativeWindow.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(KARMA_HAS_X11_XCB)
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#endif

namespace karma::renderer_backend {
namespace {

constexpr std::size_t kDirectionalShadowCascadeCount =
    kDiligentDirectionalShadowCascadeCount;
constexpr std::size_t kMaxLocalLights = kDiligentMaxLocalLights;
constexpr std::size_t kMaxPointShadowLights = kDiligentMaxPointShadowLights;
constexpr std::size_t kPointShadowMatrixCount = kDiligentPointShadowMatrixCount;

using Mesh = DiligentMesh;
using Material = DiligentMaterial;

using RenderableDraw = DiligentRenderableDraw;

void DILIGENT_CALL_TYPE DiligentMessageCallback(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                                const Diligent::Char* message,
                                                const Diligent::Char* /*function*/,
                                                const Diligent::Char* /*file*/,
                                                int /*line*/) {
    if (!message) {
        return;
    }
    switch (severity) {
        case Diligent::DEBUG_MESSAGE_SEVERITY_INFO:
            KARMA_TRACE("render.diligent.internal", "{}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_WARNING:
            spdlog::warn("Diligent: {}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_ERROR:
            spdlog::error("Diligent: {}", message);
            break;
        case Diligent::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
            spdlog::critical("Diligent: {}", message);
            break;
        default:
            KARMA_TRACE("render.diligent.internal", "{}", message);
            break;
    }
}

} // namespace

class DiligentBackend final : public Backend {
 public:
    explicit DiligentBackend(karma::platform::Window& window) {
        auto* factory = Diligent::GetEngineFactoryVk();
        if (!factory) {
            spdlog::error("Failed to get Diligent Vulkan factory");
            return;
        }
        factory->SetMessageCallback(DiligentMessageCallback);

        Diligent::EngineVkCreateInfo engine_ci{};
        engine_ci.EnableValidation = false;

        factory->CreateDeviceAndContextsVk(engine_ci, &device_, &context_);
        if (!device_ || !context_) {
            spdlog::error("Failed to create Diligent device/context");
            return;
        }

        int fb_w = 0;
        int fb_h = 0;
        window.getFramebufferSize(fb_w, fb_h);

        Diligent::SwapChainDesc sc_desc{};
        sc_desc.Width = static_cast<Diligent::Uint32>(std::max(1, fb_w));
        sc_desc.Height = static_cast<Diligent::Uint32>(std::max(1, fb_h));
        sc_desc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
        sc_desc.DepthBufferFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        KARMA_TRACE("render.diligent", "Diligent: swapchain init size={}x{}", sc_desc.Width, sc_desc.Height);

        Diligent::NativeWindow native{};
        auto handle = window.nativeHandle();
        KARMA_TRACE("render.diligent", "Diligent: native handle wayland={} x11={} display={} window={} wl_surface={}",
                    handle.is_wayland, handle.is_x11, handle.display, handle.window, handle.wayland_surface);
        if (handle.is_wayland && handle.wayland_surface && handle.display) {
            Diligent::LinuxNativeWindow linuxWindow{};
            linuxWindow.pDisplay = handle.display;
            linuxWindow.pWaylandSurface = handle.wayland_surface;
            linuxWindow.WindowId = 0;
            native = Diligent::NativeWindow{linuxWindow};
        } else if (handle.is_x11 && handle.window && handle.display) {
            Diligent::LinuxNativeWindow linuxWindow{};
#if defined(KARMA_HAS_X11_XCB)
            // TODO(bz3-rewrite): X11 + Diligent swapchain init still fails (VK_ERROR_INITIALIZATION_FAILED).
            // Keep this path visible for now; needs further parity investigation.
            auto* xcb = XGetXCBConnection(reinterpret_cast<Display*>(handle.display));
            if (!xcb) {
                spdlog::error("Diligent: X11 display missing XCB connection");
                return;
            }
            linuxWindow.pXCBConnection = xcb;
            linuxWindow.WindowId = reinterpret_cast<uintptr_t>(handle.window);
            linuxWindow.pDisplay = nullptr;
            linuxWindow.pWaylandSurface = nullptr;
            native = Diligent::NativeWindow{linuxWindow};
            KARMA_TRACE("render.diligent", "Diligent: using XCB connection for X11 swapchain (xcb={})", static_cast<void*>(xcb));
#else
            spdlog::error("Diligent: X11 requires XCB support (libX11-xcb). Use Wayland or install XCB deps.");
            return;
#endif
        } else {
            spdlog::error("Diligent: failed to resolve native window for swapchain");
            return;
        }

        factory->CreateSwapChainVk(device_, context_, sc_desc, native, &swapchain_);
        if (!swapchain_) {
            spdlog::error("Failed to create Diligent swapchain");
            return;
        }

        createPipelineVariants();
        createShadowDepthPipeline();
        createLinePipeline();
        white_srv_ = createWhiteTexture();
        if (!hasReadyPipelines() || !hasReadyLinePipeline() || !swapchain_) {
            return;
        }
        std::array<detail::SamplerVariableAvailability, detail::kDiligentMaterialVariantCount>
            material_sampler_availability{};
        for (std::size_t i = 0; i < material_sampler_availability.size(); ++i) {
            Diligent::IShaderResourceBinding* srb = pipeline_variants_[i].srb;
            detail::SamplerVariableAvailability& availability = material_sampler_availability[i];
            availability.srb_ready = (srb != nullptr);
            if (availability.srb_ready) {
                availability.has_s_tex = hasSamplerVariable(srb, "s_tex");
                availability.has_s_normal = hasSamplerVariable(srb, "s_normal");
                availability.has_s_occlusion = hasSamplerVariable(srb, "s_occlusion");
            }
        }
        detail::SamplerVariableAvailability line_sampler_availability{};
        line_sampler_availability.srb_ready = (line_pipeline_.srb != nullptr);
        if (line_sampler_availability.srb_ready) {
            line_sampler_availability.has_s_tex = hasSamplerVariable(line_pipeline_.srb, "s_tex");
            line_sampler_availability.has_s_normal = hasSamplerVariable(line_pipeline_.srb, "s_normal");
            line_sampler_availability.has_s_occlusion = hasSamplerVariable(line_pipeline_.srb, "s_occlusion");
        }
        const DiligentDirectSamplerContractState direct_sampler_state =
            EvaluateDiligentDirectSamplerContractState(
                material_sampler_availability,
                line_sampler_availability);
        direct_sampler_contract_report_ = direct_sampler_state.contract_report;
        supports_direct_multi_sampler_inputs_ = direct_sampler_state.supports_direct_multi_sampler_inputs;
        direct_sampler_disable_reason_ = direct_sampler_state.disable_reason;
        initialized_ = true;
    }

    ~DiligentBackend() override = default;

    void beginFrame(int width, int height, float dt) override {
        (void)dt;
        if (!initialized_) {
            return;
        }
        draw_items_.clear();
        debug_lines_.clear();
        frame_cleared_ = false;
        if (width == width_ && height == height_) {
            return;
        }
        width_ = width;
        height_ = height;
        if (swapchain_) {
            swapchain_->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
        DiligentShadowState shadow_state{};
        shadow_state.shadow_tex = std::addressof(shadow_tex_);
        shadow_state.shadow_srv = std::addressof(shadow_srv_);
        shadow_state.shadow_rtv = std::addressof(shadow_rtv_);
        shadow_state.shadow_dsv = std::addressof(shadow_dsv_);
        shadow_state.point_shadow_tex = std::addressof(point_shadow_tex_);
        shadow_state.point_shadow_srv = std::addressof(point_shadow_srv_);
        shadow_state.shadow_tex_size = &shadow_tex_size_;
        shadow_state.point_shadow_tex_width = &point_shadow_tex_width_;
        shadow_state.point_shadow_tex_height = &point_shadow_tex_height_;
        shadow_state.shadow_tex_is_rt = &shadow_tex_is_rt_;
        shadow_state.shadow_tex_rt_uses_depth = &shadow_tex_rt_uses_depth_;
        shadow_state.shadow_tex_ready = &shadow_tex_ready_;
        shadow_state.point_shadow_tex_ready = &point_shadow_tex_ready_;
        shadow_state.cached_point_shadow_map = &cached_point_shadow_map_;
        shadow_state.point_shadow_frames_until_update = &point_shadow_frames_until_update_;
        shadow_state.point_shadow_slot_source_index = &point_shadow_slot_source_index_;
        shadow_state.point_shadow_slot_position = &point_shadow_slot_position_;
        shadow_state.point_shadow_slot_range = &point_shadow_slot_range_;
        shadow_state.point_shadow_face_dirty = &point_shadow_face_dirty_;
        shadow_state.point_shadow_face_cursor = &point_shadow_face_cursor_;
        shadow_state.point_shadow_cache_initialized = &point_shadow_cache_initialized_;
        ResetDiligentShadowResources(shadow_state);
        shadow_frames_until_update_ = 0;
        KARMA_TRACE_CHANGED(
            "render.diligent",
            std::to_string(width_) + "x" + std::to_string(height_),
            "shadow resources reset reason=swapchain_resize size={}x{}",
            width_,
            height_);
    }

    void endFrame() override {
        if (!initialized_) {
            return;
        }
        if (swapchain_) {
            swapchain_->Present();
        }
    }

    renderer::MeshId createMesh(const renderer::MeshData& mesh) override {
        if (!initialized_) {
            return renderer::kInvalidMesh;
        }
        return CreateDiligentMeshAsset(device_, white_srv_, mesh, next_mesh_id_, meshes_);
    }

    void destroyMesh(renderer::MeshId mesh) override {
        DestroyDiligentMeshAsset(mesh, meshes_);
    }

    renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) override {
        if (!initialized_) {
            return renderer::kInvalidMaterial;
        }
        return CreateDiligentMaterialAsset(
            device_,
            material,
            supports_direct_multi_sampler_inputs_,
            next_material_id_,
            materials_);
    }

    void destroyMaterial(renderer::MaterialId material) override {
        DestroyDiligentMaterialAsset(material, materials_);
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
        if (!initialized_ || !context_ || !swapchain_ || !hasReadyPipelines() || !hasReadyLinePipeline()) {
            return;
        }

        auto* rtv = swapchain_->GetCurrentBackBufferRTV();
        auto* dsv = swapchain_->GetDepthBufferDSV();
        context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        SetDiligentViewport(
            context_,
            static_cast<uint32_t>(std::max(1, width_)),
            static_cast<uint32_t>(std::max(1, height_)));
        if (!frame_cleared_) {
            const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
                detail::ResolveEnvironmentLightingSemantics(environment_);
            const glm::vec4 clear_color = detail::ComputeEnvironmentClearColor(environment_semantics);
            const float clear[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
            context_->ClearRenderTarget(rtv, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            if (dsv) {
                context_->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
            frame_cleared_ = true;
        }

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
            auto mat_it = materials_.find(item.material);
            if (mesh_it == meshes_.end() || mat_it == materials_.end()) {
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
        DiligentShadowState shadow_state{};
        shadow_state.device = device_;
        shadow_state.context = context_;
        shadow_state.shadow_depth_pso = shadow_depth_pso_;
        shadow_state.shadow_depth_srb = shadow_depth_srb_;
        shadow_state.constant_buffer = constant_buffer_;
        shadow_state.shadow_tex = std::addressof(shadow_tex_);
        shadow_state.shadow_srv = std::addressof(shadow_srv_);
        shadow_state.shadow_rtv = std::addressof(shadow_rtv_);
        shadow_state.shadow_dsv = std::addressof(shadow_dsv_);
        shadow_state.point_shadow_tex = std::addressof(point_shadow_tex_);
        shadow_state.point_shadow_srv = std::addressof(point_shadow_srv_);
        shadow_state.shadow_tex_size = &shadow_tex_size_;
        shadow_state.point_shadow_tex_width = &point_shadow_tex_width_;
        shadow_state.point_shadow_tex_height = &point_shadow_tex_height_;
        shadow_state.shadow_tex_is_rt = &shadow_tex_is_rt_;
        shadow_state.shadow_tex_rt_uses_depth = &shadow_tex_rt_uses_depth_;
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

        const DiligentDirectionalShadowUpdateResult directional_shadow_update =
            UpdateDiligentDirectionalShadowCache(
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

        context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        SetDiligentViewport(
            context_,
            static_cast<uint32_t>(std::max(1, width_)),
            static_cast<uint32_t>(std::max(1, height_)));
        const detail::DirectionalShadowCascadeSet active_shadow_cascades =
            cached_shadow_cascades_.ready
                ? cached_shadow_cascades_
                : detail::BuildDirectionalShadowCascadeSetFromSingleMap(cached_shadow_map_);
        const detail::DirectionalShadowMap& shadow_map = cached_shadow_map_;
        const detail::DirectionalShadowCascade& primary_cascade =
            active_shadow_cascades.cascades[0];
        const detail::ShadowClipDepthTransform clip_depth =
            detail::ResolveShadowClipDepthTransform(false);
        const float inv_depth_range = primary_cascade.depth_range > 1e-6f
            ? (1.0f / primary_cascade.depth_range)
            : 0.0f;
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
        const glm::vec4 shadow_params0{
            shadow_tex_ready_ && active_shadow_cascades.ready ? 1.0f : 0.0f,
            active_shadow_cascades.strength,
            active_shadow_cascades.bias,
            active_shadow_cascades.map_size > 0
                ? (1.0f / static_cast<float>(active_shadow_cascades.map_size))
                : 0.0f,
        };
        const glm::vec4 shadow_params1{
            primary_cascade.extent,
            primary_cascade.center_x,
            primary_cascade.center_y,
            primary_cascade.min_depth,
        };
        const glm::vec4 shadow_params2{
            inv_depth_range,
            static_cast<float>(active_shadow_cascades.pcf_radius),
            clip_depth.scale,
            clip_depth.bias,
        };
        const glm::vec4 shadow_bias_params{
            shadow_semantics.receiver_bias_scale,
            shadow_semantics.normal_bias_scale,
            shadow_semantics.raster_depth_bias,
            shadow_semantics.raster_slope_bias,
        };
        const glm::vec4 shadow_axis_right{
            active_shadow_cascades.axis_right.x,
            active_shadow_cascades.axis_right.y,
            active_shadow_cascades.axis_right.z,
            0.0f,
        };
        const glm::vec4 shadow_axis_up{
            active_shadow_cascades.axis_up.x,
            active_shadow_cascades.axis_up.y,
            active_shadow_cascades.axis_up.z,
            0.0f,
        };
        const glm::vec4 shadow_axis_forward{
            active_shadow_cascades.axis_forward.x,
            active_shadow_cascades.axis_forward.y,
            active_shadow_cascades.axis_forward.z,
            0.0f,
        };
        const glm::vec4 shadow_cascade_splits_uniform{
            shadow_cascade_splits[0],
            shadow_cascade_splits[1],
            shadow_cascade_splits[2],
            shadow_cascade_splits[3],
        };
        const glm::vec4 shadow_cascade_world_texel_uniform{
            shadow_cascade_world_texel[0],
            shadow_cascade_world_texel[1],
            shadow_cascade_world_texel[2],
            shadow_cascade_world_texel[3],
        };
        const glm::vec4 shadow_cascade_params_uniform{
            active_shadow_cascades.transition_fraction,
            static_cast<float>(std::max(active_shadow_cascades.cascade_count, 1)),
            active_shadow_atlas_inv_size,
            0.0f,
        };
        const glm::vec4 shadow_camera_position{
            camera_.position.x,
            camera_.position.y,
            camera_.position.z,
            0.0f,
        };
        const glm::vec4 shadow_camera_forward{
            current_camera_forward.x,
            current_camera_forward.y,
            current_camera_forward.z,
            0.0f,
        };
        std::array<glm::vec4, kMaxLocalLights> local_light_pos_range{};
        std::array<glm::vec4, kMaxLocalLights> local_light_color_intensity{};
        std::array<glm::vec4, kMaxLocalLights> local_light_shadow_slot{};
        std::array<int, kMaxLocalLights> local_light_source_indices{};
        local_light_source_indices.fill(-1);
        for (glm::vec4& slot : local_light_shadow_slot) {
            slot = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
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
            local_light_pos_range[static_cast<std::size_t>(local_light_count)] =
                glm::vec4(light.position, clamped_range);
            local_light_color_intensity[static_cast<std::size_t>(local_light_count)] =
                glm::vec4(light.color.r, light.color.g, light.color.b, clamped_intensity);
            local_light_source_indices[static_cast<std::size_t>(local_light_count)] = static_cast<int>(light_idx);
            ++local_light_count;
        }
        const std::vector<int> point_shadow_selected_indices =
            detail::SelectPointShadowLightIndices(shadow_semantics, lights_);
        const DiligentPointShadowUpdateResult point_shadow_update =
            UpdateDiligentPointShadowCache(
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
        std::array<glm::mat4, kPointShadowMatrixCount> point_shadow_uv_proj{};
        for (glm::mat4& matrix : point_shadow_uv_proj) {
            matrix = glm::mat4(1.0f);
        }
        if (point_shadow_tex_ready_ && cached_point_shadow_map_.ready) {
            point_shadow_active = std::max(0, cached_point_shadow_map_.light_count);
            const std::size_t matrix_count =
                std::min(kPointShadowMatrixCount, cached_point_shadow_map_.uv_proj.size());
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
                local_light_shadow_slot[static_cast<std::size_t>(local_slot)] =
                    glm::vec4(static_cast<float>(shadow_slot), 0.0f, 0.0f, 0.0f);
            }
        }
        const glm::vec4 local_light_count_uniform{
            static_cast<float>(local_light_count),
            0.0f,
            0.0f,
            0.0f,
        };
        const glm::vec4 local_light_params_uniform{
            shadow_semantics.local_light_distance_damping,
            shadow_semantics.local_light_range_falloff_exponent,
            shadow_semantics.ao_affects_local_lights ? 1.0f : 0.0f,
            shadow_semantics.local_light_directional_shadow_lift_strength,
        };
        const bool point_shadow_enabled_uniform = world_layer && point_shadow_active > 0;
        const glm::vec4 point_shadow_params_uniform{
            point_shadow_enabled_uniform ? 1.0f : 0.0f,
            point_shadow_active > 0 ? cached_point_shadow_map_.texel_size : 0.0f,
            static_cast<float>(std::clamp(shadow_semantics.pcf_radius, 0, 1)),
            point_shadow_enabled_uniform ? static_cast<float>(point_shadow_active) : 0.0f,
        };
        const glm::vec4 point_shadow_atlas_texel_uniform{
            (point_shadow_enabled_uniform && cached_point_shadow_map_.atlas_width > 0)
                ? (1.0f / static_cast<float>(cached_point_shadow_map_.atlas_width))
                : 0.0f,
            (point_shadow_enabled_uniform && cached_point_shadow_map_.atlas_height > 0)
                ? (1.0f / static_cast<float>(cached_point_shadow_map_.atlas_height))
                : 0.0f,
            0.0f,
            0.0f,
        };
        const glm::vec4 point_shadow_tuning_uniform{
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
            "render.diligent",
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
            "render.diligent",
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
        DiligentRenderSubmissionInput render_input{};
        render_input.layer = layer;
        render_input.camera = &camera_;
        render_input.width = width_;
        render_input.height = height_;
        render_input.light = &light_;
        render_input.environment_semantics = &environment_semantics;
        render_input.renderables = &renderables;
        render_input.debug_lines = &debug_lines_;
        render_input.shadow_map = &shadow_map;
        render_input.supports_direct_multi_sampler_inputs = supports_direct_multi_sampler_inputs_;
        render_input.direct_sampler_disable_reason = &direct_sampler_disable_reason_;
        render_input.context = context_;
        render_input.pipeline_variants = &pipeline_variants_;
        render_input.line_pipeline = &line_pipeline_;
        render_input.constant_buffer = constant_buffer_;
        render_input.line_vertex_buffer = line_vertex_buffer_;
        render_input.white_srv = std::addressof(white_srv_);
        render_input.shadow_srv = std::addressof(shadow_srv_);
        render_input.shadow_tex_ready = shadow_tex_ready_;
        render_input.point_shadow_srv = std::addressof(point_shadow_srv_);
        render_input.point_shadow_tex_ready = point_shadow_tex_ready_;
        render_input.shadow_params0 = &shadow_params0;
        render_input.shadow_params1 = &shadow_params1;
        render_input.shadow_params2 = &shadow_params2;
        render_input.shadow_bias_params = &shadow_bias_params;
        render_input.shadow_axis_right = &shadow_axis_right;
        render_input.shadow_axis_up = &shadow_axis_up;
        render_input.shadow_axis_forward = &shadow_axis_forward;
        render_input.shadow_cascade_splits_uniform = &shadow_cascade_splits_uniform;
        render_input.shadow_cascade_world_texel_uniform = &shadow_cascade_world_texel_uniform;
        render_input.shadow_cascade_params_uniform = &shadow_cascade_params_uniform;
        render_input.shadow_camera_position = &shadow_camera_position;
        render_input.shadow_camera_forward = &shadow_camera_forward;
        render_input.shadow_cascade_uv_proj = &shadow_cascade_uv_proj;
        render_input.local_light_count_uniform = &local_light_count_uniform;
        render_input.local_light_params_uniform = &local_light_params_uniform;
        render_input.local_light_pos_range = &local_light_pos_range;
        render_input.local_light_color_intensity = &local_light_color_intensity;
        render_input.local_light_shadow_slot = &local_light_shadow_slot;
        render_input.point_shadow_params_uniform = &point_shadow_params_uniform;
        render_input.point_shadow_atlas_texel_uniform = &point_shadow_atlas_texel_uniform;
        render_input.point_shadow_tuning_uniform = &point_shadow_tuning_uniform;
        render_input.point_shadow_uv_proj = &point_shadow_uv_proj;
        SubmitDiligentRenderLayerDraws(render_input);

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
        return initialized_ && device_ && context_ && swapchain_;
    }

 private:
    static constexpr std::size_t kPipelineVariantCount = 4;
    static_assert(kPipelineVariantCount == detail::kDiligentMaterialVariantCount,
                  "pipeline variant count must match direct sampler observability contract");

    static std::size_t PipelineVariantIndex(bool alpha_blend, bool double_sided) {
        return (alpha_blend ? 2u : 0u) + (double_sided ? 1u : 0u);
    }

    bool hasReadyPipelines() const {
        for (const auto& variant : pipeline_variants_) {
            if (!variant.pso || !variant.srb) {
                return false;
            }
        }
        return true;
    }

    bool hasReadyLinePipeline() const {
        return line_pipeline_.pso && line_pipeline_.srb && line_vertex_buffer_;
    }

    bool hasSamplerVariable(Diligent::IShaderResourceBinding* srb, const char* name) const {
        return srb &&
               name &&
               srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, name);
    }

    void createPipelineVariants() {
        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        Diligent::RefCntAutoPtr<Diligent::IShader> ps;
        if (!CreateDiligentMainPipelineShadersAndConstants(
                device_.RawPtr(),
                static_cast<uint32_t>(sizeof(DiligentRenderConstants)),
                constant_buffer_,
                vs,
                ps)) {
            return;
        }

        createPipelineVariant("simple_pso_opaque_cull", vs, ps, false, false,
                              pipeline_variants_[PipelineVariantIndex(false, false)]);
        createPipelineVariant("simple_pso_opaque_double_sided", vs, ps, false, true,
                              pipeline_variants_[PipelineVariantIndex(false, true)]);
        createPipelineVariant("simple_pso_blend_cull", vs, ps, true, false,
                              pipeline_variants_[PipelineVariantIndex(true, false)]);
        createPipelineVariant("simple_pso_blend_double_sided", vs, ps, true, true,
                              pipeline_variants_[PipelineVariantIndex(true, true)]);
    }

    void createShadowDepthPipeline() {
        CreateDiligentShadowDepthPipeline(
            device_.RawPtr(),
            constant_buffer_.RawPtr(),
            shadow_depth_pso_,
            shadow_depth_srb_);
    }

    void createLinePipeline() {
        CreateDiligentLinePipeline(
            device_.RawPtr(),
            swapchain_.RawPtr(),
            constant_buffer_.RawPtr(),
            static_cast<uint32_t>(sizeof(DiligentLineVertex)),
            line_pipeline_.pso,
            line_pipeline_.srb,
            line_vertex_buffer_);
    }

    void createPipelineVariant(const char* name,
                               Diligent::IShader* vs,
                               Diligent::IShader* ps,
                               bool alpha_blend,
                               bool double_sided,
                               DiligentPipelineVariant& out_variant) {
        CreateDiligentPipelineVariant(
            device_.RawPtr(),
            swapchain_.RawPtr(),
            constant_buffer_.RawPtr(),
            name,
            vs,
            ps,
            alpha_blend,
            double_sided,
            out_variant.pso,
            out_variant.srb);
    }

    Diligent::RefCntAutoPtr<Diligent::ITextureView> createWhiteTexture() {
        return CreateDiligentWhiteTexture(device_);
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain_;
    std::array<DiligentPipelineVariant, kPipelineVariantCount> pipeline_variants_{};
    DiligentLinePipeline line_pipeline_{};
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> shadow_depth_pso_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shadow_depth_srb_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constant_buffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> line_vertex_buffer_;
    detail::DiligentDirectSamplerContractReport direct_sampler_contract_report_{};
    std::string direct_sampler_disable_reason_ = "not_initialized";
    bool supports_direct_multi_sampler_inputs_ = false;

    std::unordered_map<renderer::MeshId, Mesh> meshes_;
    std::unordered_map<renderer::MaterialId, Material> materials_;
    std::vector<renderer::DrawItem> draw_items_;
    std::vector<detail::ResolvedDebugLineSemantics> debug_lines_;
    bool frame_cleared_ = false;

    renderer::CameraData camera_{};
    renderer::DirectionalLightData light_{};
    std::vector<renderer::LightData> lights_{};
    renderer::EnvironmentLightingData environment_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> white_srv_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> shadow_tex_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> shadow_srv_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> shadow_rtv_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> shadow_dsv_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> point_shadow_tex_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> point_shadow_srv_;
    uint32_t shadow_tex_size_ = 0u;
    uint32_t point_shadow_tex_width_ = 0u;
    uint32_t point_shadow_tex_height_ = 0u;
    bool shadow_tex_is_rt_ = false;
    bool shadow_tex_rt_uses_depth_ = false;
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
    std::array<uint8_t, kPointShadowMatrixCount> point_shadow_face_dirty_{};
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
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateDiligentBackend(karma::platform::Window& window) {
    return std::make_unique<DiligentBackend>(window);
}

} // namespace karma::renderer_backend

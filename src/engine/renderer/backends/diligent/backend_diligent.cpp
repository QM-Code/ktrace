#include "karma/renderer/backend.hpp"

#include "../backend_factory_internal.hpp"
#include "../direct_sampler_observability_internal.hpp"
#include "../debug_line_internal.hpp"
#include "../directional_shadow_internal.hpp"
#include "../environment_lighting_internal.hpp"
#include "../material_lighting_internal.hpp"
#include "../material_semantics_internal.hpp"

#include "karma/common/logging.hpp"
#include "karma/platform/window.hpp"

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
#include <DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <DiligentCore/Platforms/interface/NativeWindow.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(KARMA_HAS_X11_XCB)
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#endif

namespace karma::renderer_backend {
namespace {

struct Mesh {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> ib;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    uint32_t num_indices = 0;
    std::vector<glm::vec3> shadow_positions{};
    std::vector<uint32_t> shadow_indices{};
    glm::vec3 shadow_center{0.0f, 0.0f, 0.0f};
};

struct Material {
    detail::ResolvedMaterialSemantics semantics{};
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> normal_srv;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> occlusion_srv;
    detail::MaterialShaderTextureInputPath shader_input_path =
        detail::MaterialShaderTextureInputPath::Disabled;
    bool shader_uses_normal_input = false;
    bool shader_uses_occlusion_input = false;
};

struct Constants {
    glm::mat4 u_modelViewProj;
    glm::vec4 u_color;
    glm::vec4 u_lightDir;
    glm::vec4 u_lightColor;
    glm::vec4 u_ambientColor;
    glm::vec4 u_unlit;
    glm::vec4 u_textureMode;
};

struct LineVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

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
            // Keep this path visible for now; needs investigation and parity with m-dev.
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
        direct_sampler_contract_report_ = detail::EvaluateDiligentDirectSamplerContract(
            material_sampler_availability,
            line_sampler_availability);
        supports_direct_multi_sampler_inputs_ = direct_sampler_contract_report_.ready_for_direct_path;
        direct_sampler_disable_reason_ = supports_direct_multi_sampler_inputs_
            ? std::string("ok")
            : direct_sampler_contract_report_.reason;
        KARMA_TRACE("render.diligent",
                    "direct sampler readiness enabled={} materialContract={} lineContract={} reason={}",
                    supports_direct_multi_sampler_inputs_ ? 1 : 0,
                    direct_sampler_contract_report_.material_pipeline_contract_ready ? 1 : 0,
                    direct_sampler_contract_report_.line_pipeline_contract_ready ? 1 : 0,
                    direct_sampler_disable_reason_);
        if (!supports_direct_multi_sampler_inputs_) {
            spdlog::warn("Diligent: direct sampler path disabled (reason={})",
                         direct_sampler_disable_reason_);
        } else if (!direct_sampler_contract_report_.line_pipeline_contract_ready) {
            KARMA_TRACE("render.diligent",
                        "line sampler contract is partial; direct material path remains enabled");
        }
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
        Mesh out;

        Diligent::BufferDesc vb_desc{};
        vb_desc.Usage = Diligent::USAGE_IMMUTABLE;
        vb_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        vb_desc.Name = "mesh_vb";

        std::vector<float> interleaved;
        interleaved.reserve(mesh.positions.size() * 8);
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            const auto& p = mesh.positions[i];
            const auto n = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            const auto uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2(0.0f, 0.0f);
            interleaved.push_back(p.x);
            interleaved.push_back(p.y);
            interleaved.push_back(p.z);
            interleaved.push_back(n.x);
            interleaved.push_back(n.y);
            interleaved.push_back(n.z);
            interleaved.push_back(uv.x);
            interleaved.push_back(uv.y);
        }
        vb_desc.Size = static_cast<uint32_t>(interleaved.size() * sizeof(float));

        Diligent::BufferData vb_data{};
        vb_data.pData = interleaved.data();
        vb_data.DataSize = static_cast<uint32_t>(interleaved.size() * sizeof(float));
        device_->CreateBuffer(vb_desc, &vb_data, &out.vb);

        Diligent::BufferDesc ib_desc{};
        ib_desc.Usage = Diligent::USAGE_IMMUTABLE;
        ib_desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
        ib_desc.Size = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t));
        ib_desc.Name = "mesh_ib";

        Diligent::BufferData ib_data{};
        ib_data.pData = mesh.indices.data();
        ib_data.DataSize = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t));
        device_->CreateBuffer(ib_desc, &ib_data, &out.ib);

        out.num_indices = static_cast<uint32_t>(mesh.indices.size());
        out.shadow_positions = mesh.positions;
        out.shadow_indices = mesh.indices;
        if (!out.shadow_positions.empty()) {
            glm::vec3 min_pos = out.shadow_positions.front();
            glm::vec3 max_pos = out.shadow_positions.front();
            for (const glm::vec3& p : out.shadow_positions) {
                min_pos = glm::min(min_pos, p);
                max_pos = glm::max(max_pos, p);
            }
            out.shadow_center = 0.5f * (min_pos + max_pos);
        }
        if (mesh.albedo && !mesh.albedo->pixels.empty()) {
            out.srv = createTextureView(*mesh.albedo);
            KARMA_TRACE("render.diligent", "createMesh texture={} {}x{}",
                        out.srv ? 1 : 0, mesh.albedo->width, mesh.albedo->height);
        }
        if (!out.srv) {
            out.srv = white_srv_;
        }
        KARMA_TRACE("render.diligent", "createMesh vertices={} indices={}",
                    mesh.positions.size(), mesh.indices.size());

        renderer::MeshId id = next_mesh_id_++;
        meshes_[id] = out;
        return id;
    }

    void destroyMesh(renderer::MeshId mesh) override {
        meshes_.erase(mesh);
    }

    renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) override {
        if (!initialized_) {
            return renderer::kInvalidMaterial;
        }
        const detail::MaterialTextureSetLifecycleIngestion texture_ingestion =
            detail::IngestMaterialTextureSetForLifecycle(material);
        const detail::MaterialShaderInputContract shader_input_contract =
            detail::ResolveMaterialShaderInputContract(
                material, texture_ingestion, supports_direct_multi_sampler_inputs_);
        const detail::ResolvedMaterialSemantics semantics = detail::ResolveMaterialSemantics(material);
        if (!detail::ValidateResolvedMaterialSemantics(semantics)) {
            spdlog::error("Diligent: material semantics validation failed");
            return renderer::kInvalidMaterial;
        }
        Material out;
        out.semantics = semantics;
        out.shader_input_path = shader_input_contract.path;
        out.shader_uses_normal_input = shader_input_contract.used_normal_lifecycle_texture;
        out.shader_uses_occlusion_input = shader_input_contract.used_occlusion_lifecycle_texture;
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_albedo_texture) {
            out.srv = createTextureView(*shader_input_contract.direct_albedo_texture);
        } else if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                   shader_input_contract.fallback_composite_texture) {
            out.srv = createTextureView(*shader_input_contract.fallback_composite_texture);
        } else if (material.albedo && !material.albedo->pixels.empty()) {
            out.srv = createTextureView(*material.albedo);
            KARMA_TRACE("render.diligent", "createMaterial texture={} {}x{}",
                        out.srv ? 1 : 0, material.albedo->width, material.albedo->height);
        }
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_normal_texture) {
            out.normal_srv = createTextureView(*shader_input_contract.direct_normal_texture);
        } else if (texture_ingestion.normal.texture) {
            out.normal_srv = createTextureView(*texture_ingestion.normal.texture);
        }
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_occlusion_texture) {
            out.occlusion_srv = createTextureView(*shader_input_contract.direct_occlusion_texture);
        } else if (texture_ingestion.occlusion.texture) {
            out.occlusion_srv = createTextureView(*texture_ingestion.occlusion.texture);
        }
        KARMA_TRACE("render.diligent",
                    "createMaterial semantics metallic={:.3f} roughness={:.3f} normalVar={:.3f} occlusion={:.3f} occlusionEdge={:.3f} emissive=({:.3f},{:.3f},{:.3f}) alphaMode={} alpha={} cutoff={:.3f} draw={} blend={} doubleSided={} mrTex={} emissiveTex={} normalTex={} occlusionTex={} normalLifecycleTex={} occlusionLifecycleTex={} normalBounded={} occlusionBounded={} shaderPath={} shaderDirect={} shaderFallbackComposite={} shaderConsumesNormal={} shaderConsumesOcclusion={} shaderUsesAlbedo={} shaderTextureBounded={}",
                    out.semantics.metallic, out.semantics.roughness, out.semantics.normal_variation, out.semantics.occlusion,
                    out.semantics.occlusion_edge,
                    out.semantics.emissive.r, out.semantics.emissive.g, out.semantics.emissive.b,
                    static_cast<int>(out.semantics.alpha_mode), out.semantics.base_color.a, out.semantics.alpha_cutoff,
                    out.semantics.draw ? 1 : 0, out.semantics.alpha_blend ? 1 : 0, out.semantics.double_sided ? 1 : 0,
                    out.semantics.used_metallic_roughness_texture ? 1 : 0,
                    out.semantics.used_emissive_texture ? 1 : 0,
                    out.semantics.used_normal_texture ? 1 : 0,
                    out.semantics.used_occlusion_texture ? 1 : 0,
                    out.normal_srv ? 1 : 0,
                    out.occlusion_srv ? 1 : 0,
                    texture_ingestion.normal.bounded ? 1 : 0,
                    texture_ingestion.occlusion.bounded ? 1 : 0,
                    static_cast<int>(shader_input_contract.path),
                    shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler ? 1 : 0,
                    (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                     shader_input_contract.fallback_composite_texture && out.srv)
                        ? 1
                        : 0,
                    shader_input_contract.used_normal_lifecycle_texture ? 1 : 0,
                    shader_input_contract.used_occlusion_lifecycle_texture ? 1 : 0,
                    shader_input_contract.used_albedo_texture ? 1 : 0,
                    shader_input_contract.bounded ? 1 : 0);
        renderer::MaterialId id = next_material_id_++;
        materials_[id] = out;
        return id;
    }

    void destroyMaterial(renderer::MaterialId material) override {
        materials_.erase(material);
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

        struct RenderableDraw {
            const renderer::DrawItem* item = nullptr;
            Mesh* mesh = nullptr;
            Material* material = nullptr;
        };
        std::vector<RenderableDraw> renderables{};
        renderables.reserve(draw_items_.size());
        std::vector<detail::DirectionalShadowCaster> shadow_casters{};
        shadow_casters.reserve(draw_items_.size());
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
            if (!mesh_it->second.shadow_positions.empty() &&
                !mesh_it->second.shadow_indices.empty()) {
                detail::DirectionalShadowCaster caster{};
                caster.transform = item.transform;
                caster.positions = &mesh_it->second.shadow_positions;
                caster.indices = &mesh_it->second.shadow_indices;
                caster.sample_center = mesh_it->second.shadow_center;
                caster.casts_shadow = !mat_it->second.semantics.alpha_blend;
                shadow_casters.push_back(caster);
            }
        }

        const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
            detail::ResolveDirectionalShadowSemantics(light_);
        const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
            detail::ResolveEnvironmentLightingSemantics(environment_);
        const detail::DirectionalShadowMap shadow_map =
            detail::BuildDirectionalShadowMap(shadow_semantics, light_.direction, shadow_casters);
        std::size_t direct_sampler_draws = 0u;
        std::size_t fallback_sampler_draws = 0u;
        std::size_t direct_contract_draws = 0u;
        std::size_t forced_fallback_draws = 0u;
        std::size_t unexpected_direct_draws = 0u;

        for (const RenderableDraw& renderable : renderables) {
            const auto& item = *renderable.item;
            const auto& mesh = *renderable.mesh;
            const auto& mat = *renderable.material;
            const auto& semantics = mat.semantics;

            auto* pipeline = pipelineForMaterial(semantics);
            if (!pipeline || !pipeline->pso || !pipeline->srb) {
                continue;
            }
            context_->SetPipelineState(pipeline->pso);

            float aspect = (height_ > 0) ? float(width_) / float(height_) : 1.0f;
            glm::mat4 view = glm::lookAt(camera_.position, camera_.target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(camera_.fov_y_degrees), aspect, camera_.near_clip, camera_.far_clip);
            Constants constants{};
            constants.u_modelViewProj = proj * view * item.transform;

            float shadow_factor = 1.0f;
            if (!semantics.alpha_blend && shadow_map.ready) {
                const glm::vec3 sample_world = detail::TransformPoint(item.transform, mesh.shadow_center);
                const float visibility = detail::SampleDirectionalShadowVisibility(shadow_map, sample_world);
                shadow_factor = detail::ComputeDirectionalShadowFactor(shadow_map, visibility);
            }
            const detail::ResolvedMaterialLighting lighting =
                detail::ResolveMaterialLighting(semantics, light_, environment_semantics, shadow_factor);
            if (!detail::ValidateResolvedMaterialLighting(lighting)) {
                continue;
            }
            constants.u_color = lighting.color;
            constants.u_lightDir = glm::vec4(light_.direction, 0.0f);
            constants.u_lightColor = lighting.light_color;
            constants.u_ambientColor = lighting.ambient_color;
            constants.u_unlit = glm::vec4(light_.unlit, 0.0f, 0.0f, 0.0f);

            const bool use_direct_sampler_path =
                supports_direct_multi_sampler_inputs_ &&
                mat.shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
            const bool contract_requests_direct =
                mat.shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
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
            constants.u_textureMode = glm::vec4(
                (use_direct_sampler_path && mat.shader_uses_normal_input) ? 1.0f : 0.0f,
                (use_direct_sampler_path && mat.shader_uses_occlusion_input) ? 1.0f : 0.0f,
                0.0f,
                0.0f);

            Diligent::MapHelper<Constants> cb_data(context_, constant_buffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
            *cb_data = constants;

            if (pipeline->srb) {
                if (auto* texVar = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_tex")) {
                    auto texture_view = use_direct_sampler_path
                        ? (mat.srv ? mat.srv : white_srv_)
                        : (mat.srv ? mat.srv : mesh.srv);
                    if (!texture_view) {
                        texture_view = white_srv_;
                    }
                    texVar->Set(texture_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
                if (auto* normalVar = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_normal")) {
                    auto normal_view = (use_direct_sampler_path && mat.normal_srv) ? mat.normal_srv : white_srv_;
                    if (!normal_view) {
                        normal_view = white_srv_;
                    }
                    normalVar->Set(normal_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
                if (auto* occlusionVar = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_occlusion")) {
                    auto occlusion_view = (use_direct_sampler_path && mat.occlusion_srv) ? mat.occlusion_srv : white_srv_;
                    if (!occlusion_view) {
                        occlusion_view = white_srv_;
                    }
                    occlusionVar->Set(occlusion_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
            }
            context_->CommitShaderResources(pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const uint64_t offsets[] = {0};
            Diligent::IBuffer* vbs[] = {mesh.vb};
            context_->SetVertexBuffers(0, 1, vbs, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            context_->SetIndexBuffer(mesh.ib, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawIndexedAttribs draw{};
            draw.IndexType = Diligent::VT_UINT32;
            draw.NumIndices = mesh.num_indices;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            context_->DrawIndexed(draw);
        }
        KARMA_TRACE_CHANGED(
            "render.diligent",
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
                "Diligent: direct sampler assertion failed (enabled={}, directContract={}, directDraws={}, fallbackDraws={}, forcedFallback={}, unexpectedDirect={}, reason={}, invariant={})",
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
            "render.diligent",
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

        if (line_pipeline_.pso && line_pipeline_.srb && line_vertex_buffer_) {
            float aspect = (height_ > 0) ? float(width_) / float(height_) : 1.0f;
            glm::mat4 view = glm::lookAt(camera_.position, camera_.target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(camera_.fov_y_degrees), aspect, camera_.near_clip, camera_.far_clip);

            for (const auto& line : debug_lines_) {
                if (line.layer != layer) {
                    continue;
                }

                context_->SetPipelineState(line_pipeline_.pso);
                {
                    Diligent::MapHelper<LineVertex> line_data(context_, line_vertex_buffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                    line_data[0] = LineVertex{line.start.x, line.start.y, line.start.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
                    line_data[1] = LineVertex{line.end.x, line.end.y, line.end.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
                }

                Constants line_constants{};
                line_constants.u_modelViewProj = proj * view;
                line_constants.u_color = line.color;
                line_constants.u_lightDir = glm::vec4(0.0f);
                line_constants.u_lightColor = glm::vec4(0.0f);
                line_constants.u_ambientColor = glm::vec4(0.0f);
                line_constants.u_unlit = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                line_constants.u_textureMode = glm::vec4(0.0f);
                Diligent::MapHelper<Constants> cb_data(context_, constant_buffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                *cb_data = line_constants;

                if (auto* tex_var = line_pipeline_.srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_tex")) {
                    tex_var->Set(white_srv_, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
                if (auto* tex_var = line_pipeline_.srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_normal")) {
                    tex_var->Set(white_srv_, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
                if (auto* tex_var = line_pipeline_.srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_occlusion")) {
                    tex_var->Set(white_srv_, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
                context_->CommitShaderResources(line_pipeline_.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                const uint64_t offsets[] = {0};
                Diligent::IBuffer* vbs[] = {line_vertex_buffer_};
                context_->SetVertexBuffers(0, 1, vbs, offsets,
                                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                           Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                context_->SetIndexBuffer(nullptr, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                Diligent::DrawAttribs draw{};
                draw.NumVertices = 2;
                draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                context_->Draw(draw);
            }
        }

    }

    void setCamera(const renderer::CameraData& camera) override {
        camera_ = camera;
    }

    void setDirectionalLight(const renderer::DirectionalLightData& light) override {
        light_ = light;
    }

    void setEnvironmentLighting(const renderer::EnvironmentLightingData& environment) override {
        environment_ = environment;
    }

    bool isValid() const override {
        return initialized_ && device_ && context_ && swapchain_;
    }

 private:
    struct PipelineVariant {
        Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    };

    struct LinePipeline {
        Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    };

    static constexpr std::size_t kPipelineVariantCount = 4;
    static_assert(kPipelineVariantCount == detail::kDiligentMaterialVariantCount,
                  "pipeline variant count must match direct sampler observability contract");

    static std::size_t PipelineVariantIndex(bool alpha_blend, bool double_sided) {
        return (alpha_blend ? 2u : 0u) + (double_sided ? 1u : 0u);
    }

    PipelineVariant* pipelineForMaterial(const detail::ResolvedMaterialSemantics& semantics) {
        return &pipeline_variants_[PipelineVariantIndex(semantics.alpha_blend, semantics.double_sided)];
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
        Diligent::ShaderCreateInfo shader_ci{};
        shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        const char* vs_source = R"(
struct VSInput {
    float3 Pos : ATTRIB0;
    float3 Nor : ATTRIB1;
    float2 UV  : ATTRIB2;
};
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
PSInput main(VSInput input) {
    PSInput outp;
    outp.Pos = mul(u_modelViewProj, float4(input.Pos, 1.0));
    outp.Nor = input.Nor;
    outp.UV = input.UV;
    return outp;
}
)";

        const char* ps_source = R"(
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
Texture2D s_tex;
SamplerState s_tex_sampler;
Texture2D s_normal;
SamplerState s_normal_sampler;
Texture2D s_occlusion;
SamplerState s_occlusion_sampler;
float4 main(PSInput input) : SV_TARGET {
    float3 n = normalize(input.Nor);
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    float normalModulation = 1.0;
    if (u_textureMode.x > 0.5) {
        float2 encodedNormal = s_normal.Sample(s_normal_sampler, input.UV).rg * 2.0 - float2(1.0, 1.0);
        float normalResponse = clamp(length(encodedNormal), 0.0, 1.0);
        normalModulation = clamp(1.0 + (0.16 * max(0.0, normalResponse - 0.18)), 1.0, 1.18);
    }
    float occlusionModulation = 1.0;
    if (u_textureMode.y > 0.5) {
        float ao = clamp(s_occlusion.Sample(s_occlusion_sampler, input.UV).r, 0.0, 1.0);
        occlusionModulation = clamp(0.25 + (0.75 * ao), 0.25, 1.0);
    }
    float combinedModulation = clamp(normalModulation * occlusionModulation, 0.20, 1.20);
    float4 shadedTexColor = float4(clamp(texColor.rgb * combinedModulation, 0.0, 1.0), clamp(texColor.a, 0.0, 1.0));
    float4 baseColor = u_color * shadedTexColor;
    if (u_unlit.x > 0.5) {
        return baseColor;
    }
    float ndotl = max(dot(n, normalize(u_lightDir.xyz)), 0.0);
    float3 lit = baseColor.rgb * (u_ambientColor.rgb + u_lightColor.rgb * ndotl);
    return float4(lit, baseColor.a);
}
)";

        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        shader_ci.Desc.Name = "simple_vs";
        shader_ci.EntryPoint = "main";
        shader_ci.Source = vs_source;
        device_->CreateShader(shader_ci, &vs);

        Diligent::RefCntAutoPtr<Diligent::IShader> ps;
        shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        shader_ci.Desc.Name = "simple_ps";
        shader_ci.EntryPoint = "main";
        shader_ci.Source = ps_source;
        device_->CreateShader(shader_ci, &ps);

        Diligent::BufferDesc cb_desc{};
        cb_desc.Name = "constants";
        cb_desc.Size = sizeof(Constants);
        cb_desc.Usage = Diligent::USAGE_DYNAMIC;
        cb_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cb_desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cb_desc, nullptr, &constant_buffer_);
        if (!constant_buffer_ || !vs || !ps) {
            spdlog::error("Diligent: failed to initialize renderer pipeline prerequisites");
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

    void createLinePipeline() {
        if (!device_ || !swapchain_ || !constant_buffer_) {
            return;
        }

        Diligent::ShaderCreateInfo shader_ci{};
        shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        const char* vs_source = R"(
struct VSInput {
    float3 Pos : ATTRIB0;
    float3 Nor : ATTRIB1;
    float2 UV  : ATTRIB2;
};
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
PSInput main(VSInput input) {
    PSInput outp;
    outp.Pos = mul(u_modelViewProj, float4(input.Pos, 1.0));
    outp.Nor = input.Nor;
    outp.UV = input.UV;
    return outp;
}
)";

        const char* ps_source = R"(
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
Texture2D s_tex;
SamplerState s_tex_sampler;
Texture2D s_normal;
SamplerState s_normal_sampler;
Texture2D s_occlusion;
SamplerState s_occlusion_sampler;
float4 main(PSInput input) : SV_TARGET {
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    return u_color * texColor;
}
)";

        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        shader_ci.Desc.Name = "line_vs";
        shader_ci.EntryPoint = "main";
        shader_ci.Source = vs_source;
        device_->CreateShader(shader_ci, &vs);

        Diligent::RefCntAutoPtr<Diligent::IShader> ps;
        shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        shader_ci.Desc.Name = "line_ps";
        shader_ci.EntryPoint = "main";
        shader_ci.Source = ps_source;
        device_->CreateShader(shader_ci, &ps);
        if (!vs || !ps) {
            return;
        }

        Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
        pso_ci.PSODesc.Name = "line_pso";
        pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        pso_ci.GraphicsPipeline.NumRenderTargets = 1;
        pso_ci.GraphicsPipeline.RTVFormats[0] = swapchain_->GetDesc().ColorBufferFormat;
        pso_ci.GraphicsPipeline.DSVFormat = swapchain_->GetDesc().DepthBufferFormat;
        pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
        pso_ci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;

        auto& rt0 = pso_ci.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = true;
        rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;

        Diligent::LayoutElement layout[] = {
            {0, 0, 3, Diligent::VT_FLOAT32, false},
            {1, 0, 3, Diligent::VT_FLOAT32, false},
            {2, 0, 2, Diligent::VT_FLOAT32, false}
        };
        pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
        pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;

        pso_ci.pVS = vs;
        pso_ci.pPS = ps;

        Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_PIXEL, "s_normal", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_PIXEL, "s_occlusion", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        Diligent::SamplerDesc sampler_desc{};
        sampler_desc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_normal_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_occlusion_sampler", sampler_desc},
        };
        pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pso_ci.PSODesc.ResourceLayout.Variables = vars;
        pso_ci.PSODesc.ResourceLayout.NumVariables = 3;
        pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 3;

        device_->CreateGraphicsPipelineState(pso_ci, &line_pipeline_.pso);
        if (!line_pipeline_.pso) {
            return;
        }
        if (auto* vs_var = line_pipeline_.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
            vs_var->Set(constant_buffer_);
        }
        if (auto* ps_var = line_pipeline_.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
            ps_var->Set(constant_buffer_);
        }
        line_pipeline_.pso->CreateShaderResourceBinding(&line_pipeline_.srb, true);

        Diligent::BufferDesc line_vb_desc{};
        line_vb_desc.Name = "debug_line_vb";
        line_vb_desc.Size = sizeof(LineVertex) * 2u;
        line_vb_desc.Usage = Diligent::USAGE_DYNAMIC;
        line_vb_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        line_vb_desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(line_vb_desc, nullptr, &line_vertex_buffer_);
    }

    void createPipelineVariant(const char* name,
                               Diligent::IShader* vs,
                               Diligent::IShader* ps,
                               bool alpha_blend,
                               bool double_sided,
                               PipelineVariant& out_variant) {
        if (!device_ || !swapchain_ || !vs || !ps || !constant_buffer_) {
            return;
        }

        Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
        pso_ci.PSODesc.Name = name;
        pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        pso_ci.GraphicsPipeline.NumRenderTargets = 1;
        pso_ci.GraphicsPipeline.RTVFormats[0] = swapchain_->GetDesc().ColorBufferFormat;
        pso_ci.GraphicsPipeline.DSVFormat = swapchain_->GetDesc().DepthBufferFormat;
        pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        auto& rt0 = pso_ci.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = alpha_blend;
        rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
        pso_ci.GraphicsPipeline.RasterizerDesc.CullMode =
            double_sided ? Diligent::CULL_MODE_NONE : Diligent::CULL_MODE_BACK;
        pso_ci.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;

        Diligent::LayoutElement layout[] = {
            {0, 0, 3, Diligent::VT_FLOAT32, false},
            {1, 0, 3, Diligent::VT_FLOAT32, false},
            {2, 0, 2, Diligent::VT_FLOAT32, false}
        };
        pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
        pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;

        pso_ci.pVS = vs;
        pso_ci.pPS = ps;

        Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_PIXEL, "s_normal", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_PIXEL, "s_occlusion", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        Diligent::SamplerDesc sampler_desc{};
        sampler_desc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
        sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_WRAP;
        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_normal_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_occlusion_sampler", sampler_desc},
        };
        pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pso_ci.PSODesc.ResourceLayout.Variables = vars;
        pso_ci.PSODesc.ResourceLayout.NumVariables = 3;
        pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 3;

        device_->CreateGraphicsPipelineState(pso_ci, &out_variant.pso);
        if (!out_variant.pso) {
            spdlog::error("Diligent: failed to create pipeline '{}'", name ? name : "<unnamed>");
            return;
        }

        if (auto* vs_var = out_variant.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
            vs_var->Set(constant_buffer_);
        } else {
            KARMA_TRACE("render.diligent", "Diligent: missing VS static var 'Constants' for {}", name ? name : "<unnamed>");
        }
        if (auto* ps_var = out_variant.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
            ps_var->Set(constant_buffer_);
        } else {
            KARMA_TRACE("render.diligent", "Diligent: missing PS static var 'Constants' for {}", name ? name : "<unnamed>");
        }

        out_variant.pso->CreateShaderResourceBinding(&out_variant.srb, true);
        if (!out_variant.srb) {
            spdlog::error("Diligent: failed to create shader resource binding for '{}'", name ? name : "<unnamed>");
        }
    }

    Diligent::RefCntAutoPtr<Diligent::ITextureView> createWhiteTexture() {
        Diligent::RefCntAutoPtr<Diligent::ITextureView> view;
        const uint32_t white = 0xffffffffu;
        Diligent::TextureDesc desc{};
        desc.Name = "white_tex";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        Diligent::TextureData texData{};
        Diligent::TextureSubResData subres{};
        subres.pData = &white;
        subres.Stride = sizeof(uint32_t);
        texData.pSubResources = &subres;
        texData.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> tex;
        device_->CreateTexture(desc, &texData, &tex);
        if (tex) {
            view = tex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        }
        return view;
    }

    Diligent::RefCntAutoPtr<Diligent::ITextureView> createTextureView(const renderer::MeshData::TextureData& tex) {
        Diligent::RefCntAutoPtr<Diligent::ITextureView> view;
        if (tex.width <= 0 || tex.height <= 0) {
            return view;
        }
        const std::vector<uint8_t> rgba_pixels = detail::ExpandTextureToRgba8(tex);
        if (rgba_pixels.empty()) {
            return view;
        }
        Diligent::TextureDesc desc{};
        desc.Name = "albedo_tex";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<uint32_t>(tex.width);
        desc.Height = static_cast<uint32_t>(tex.height);
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        Diligent::TextureData texData{};
        Diligent::TextureSubResData subres{};
        subres.pData = rgba_pixels.data();
        subres.Stride = static_cast<uint32_t>(tex.width * 4);
        texData.pSubResources = &subres;
        texData.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> outTex;
        device_->CreateTexture(desc, &texData, &outTex);
        if (outTex) {
            view = outTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        }
        return view;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain_;
    std::array<PipelineVariant, kPipelineVariantCount> pipeline_variants_{};
    LinePipeline line_pipeline_{};
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
    renderer::EnvironmentLightingData environment_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> white_srv_;
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateDiligentBackend(karma::platform::Window& window) {
    return std::make_unique<DiligentBackend>(window);
}

} // namespace karma::renderer_backend

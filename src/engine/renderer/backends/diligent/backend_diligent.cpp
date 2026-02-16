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

constexpr uint32_t kDiligentMaterialMaxAnisotropy = 8u;
constexpr std::size_t kMaxLocalLights = 4u;

struct Mesh {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> ib;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    uint32_t num_indices = 0;
    std::vector<glm::vec3> shadow_positions{};
    std::vector<uint32_t> shadow_indices{};
    glm::vec3 shadow_center{0.0f, 0.0f, 0.0f};
    float shadow_radius = 0.0f;
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
    glm::mat4 u_model;
    glm::vec4 u_color;
    glm::vec4 u_lightDir;
    glm::vec4 u_lightColor;
    glm::vec4 u_ambientColor;
    glm::vec4 u_unlit;
    glm::vec4 u_textureMode;
    glm::vec4 u_shadowParams0;
    glm::vec4 u_shadowParams1;
    glm::vec4 u_shadowParams2;
    glm::vec4 u_shadowBiasParams;
    glm::vec4 u_shadowAxisRight;
    glm::vec4 u_shadowAxisUp;
    glm::vec4 u_shadowAxisForward;
    glm::vec4 u_localLightCount;
    glm::vec4 u_localLightParams;
    std::array<glm::vec4, kMaxLocalLights> u_localLightPosRange;
    std::array<glm::vec4, kMaxLocalLights> u_localLightColorIntensity;
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

struct RenderableDraw {
    const renderer::DrawItem* item = nullptr;
    Mesh* mesh = nullptr;
    Material* material = nullptr;
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

bool intersectsDirectionalShadowMap(const detail::DirectionalShadowMap& map,
                                    const glm::vec3& center,
                                    float radius) {
    if (!map.ready) {
        return true;
    }
    const float lx = glm::dot(center, map.axis_right);
    const float ly = glm::dot(center, map.axis_up);
    const float lz = glm::dot(center, map.axis_forward);
    const float padded_extent = map.extent + std::max(radius, 0.0f);
    if (std::fabs(lx - map.center_x) > padded_extent ||
        std::fabs(ly - map.center_y) > padded_extent) {
        return false;
    }
    if (map.depth_range > 1e-6f) {
        const float min_depth = map.min_depth - std::max(radius, 0.0f);
        const float max_depth = map.min_depth + map.depth_range + std::max(radius, 0.0f);
        if (lz < min_depth || lz > max_depth) {
            return false;
        }
    }
    return true;
}

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
        resetShadowResources();
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
            float radius_sq = 0.0f;
            for (const glm::vec3& p : out.shadow_positions) {
                const glm::vec3 delta = p - out.shadow_center;
                radius_sq = std::max(radius_sq, glm::dot(delta, delta));
            }
            out.shadow_radius = std::sqrt(std::max(radius_sq, 0.0f));
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
        setViewport(static_cast<uint32_t>(std::max(1, width_)), static_cast<uint32_t>(std::max(1, height_)));
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
        const int requested_update_every_frames = std::max(1, light_.shadow.update_every_frames);
        if (shadow_update_every_frames_ != requested_update_every_frames) {
            shadow_update_every_frames_ = requested_update_every_frames;
            shadow_frames_until_update_ = 0;
            KARMA_TRACE_CHANGED(
                "render.diligent",
                std::to_string(shadow_update_every_frames_),
                "shadow update cadence everyFrames={}",
                shadow_update_every_frames_);
        }
        const glm::vec3 current_camera_forward = resolveCameraForward(camera_);
        const glm::vec3 current_light_direction =
            normalizeDirectionOrFallback(light_.direction, cached_shadow_light_direction_);
        bool shadow_inputs_changed = false;
        if (world_layer) {
            if (!shadow_cache_inputs_valid_) {
                shadow_inputs_changed = true;
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
            } else {
                for (std::size_t i = 0; i < current_shadow_items.size(); ++i) {
                    const renderer::DrawItem& curr = current_shadow_items[i];
                    const renderer::DrawItem& prev = previous_shadow_items_[i];
                    if (curr.mesh != prev.mesh ||
                        curr.material != prev.material ||
                        curr.layer != prev.layer ||
                        curr.casts_shadow != prev.casts_shadow ||
                        matrixChangedBeyondEpsilon(curr.transform, prev.transform)) {
                        shadow_inputs_changed = true;
                        break;
                    }
                }
            }
        }
        const bool gpu_shadow_capable =
            shadow_depth_pso_ && world_layer && !shadow_casters.empty();
        const bool use_gpu_shadow_path =
            gpu_shadow_requested && shadow_semantics.enabled && gpu_shadow_capable;
        if (gpu_shadow_requested && shadow_semantics.enabled && world_layer && !use_gpu_shadow_path) {
            const char* reason = "gpu_shadow_pass_not_implemented";
            if (!shadow_depth_pso_) {
                reason = "gpu_shadow_pipeline_unavailable";
            } else if (shadow_casters.empty()) {
                reason = "gpu_shadow_no_casters";
            }
            KARMA_TRACE_CHANGED(
                "render.diligent",
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
            shadow_tex_ready_ = false;
            shadow_cache_inputs_valid_ = false;
            previous_shadow_items_.clear();
        } else if (world_layer && shadow_casters.empty()) {
            shadow_frames_until_update_ = 0;
            cached_shadow_map_ = detail::DirectionalShadowMap{};
            shadow_tex_ready_ = false;
            shadow_cache_inputs_valid_ = false;
            previous_shadow_items_.clear();
        } else if (world_layer && !shadow_casters.empty()) {
            if (shadow_frames_until_update_ <= 0 ||
                shadow_inputs_changed ||
                !cached_shadow_map_.ready ||
                cached_shadow_map_.size != shadow_semantics.map_size) {
                if (use_gpu_shadow_path) {
                    cached_shadow_map_ = detail::BuildDirectionalShadowProjection(
                        shadow_semantics,
                        light_.direction,
                        shadow_casters,
                        &shadow_view);
                    shadow_tex_ready_ = renderGpuShadowMap(cached_shadow_map_, renderables);
                    if (!shadow_tex_ready_) {
                        KARMA_TRACE_CHANGED(
                            "render.diligent",
                            std::to_string(layer) + ":gpu_shadow_render_failed",
                            "shadow execution mode requested={} active={} reason={}",
                            renderer::DirectionalLightData::ShadowExecutionModeToken(light_.shadow.execution_mode),
                            renderer::DirectionalLightData::ShadowExecutionModeToken(
                                renderer::DirectionalLightData::ShadowExecutionMode::CpuReference),
                            "gpu_shadow_render_failed");
                        cached_shadow_map_ = detail::BuildDirectionalShadowMap(
                            shadow_semantics,
                            light_.direction,
                            shadow_casters,
                            &shadow_view);
                        updateShadowTexture(cached_shadow_map_);
                    }
                } else {
                    cached_shadow_map_ = detail::BuildDirectionalShadowMap(
                        shadow_semantics,
                        light_.direction,
                        shadow_casters,
                        &shadow_view);
                    updateShadowTexture(cached_shadow_map_);
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
        context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        setViewport(static_cast<uint32_t>(std::max(1, width_)), static_cast<uint32_t>(std::max(1, height_)));
        const detail::DirectionalShadowMap& shadow_map = cached_shadow_map_;
        const detail::ShadowClipDepthTransform clip_depth =
            detail::ResolveShadowClipDepthTransform(false);
        const float inv_depth_range = shadow_map.depth_range > 1e-6f
            ? (1.0f / shadow_map.depth_range)
            : 0.0f;
        const glm::vec4 shadow_params0{
            shadow_tex_ready_ ? 1.0f : 0.0f,
            shadow_map.strength,
            shadow_map.bias,
            shadow_map.size > 0 ? (1.0f / static_cast<float>(shadow_map.size)) : 0.0f,
        };
        const glm::vec4 shadow_params1{
            shadow_map.extent,
            shadow_map.center_x,
            shadow_map.center_y,
            shadow_map.min_depth,
        };
        const glm::vec4 shadow_params2{
            inv_depth_range,
            static_cast<float>(shadow_map.pcf_radius),
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
            shadow_map.axis_right.x,
            shadow_map.axis_right.y,
            shadow_map.axis_right.z,
            0.0f,
        };
        const glm::vec4 shadow_axis_up{
            shadow_map.axis_up.x,
            shadow_map.axis_up.y,
            shadow_map.axis_up.z,
            0.0f,
        };
        const glm::vec4 shadow_axis_forward{
            shadow_map.axis_forward.x,
            shadow_map.axis_forward.y,
            shadow_map.axis_forward.z,
            0.0f,
        };
        std::array<glm::vec4, kMaxLocalLights> local_light_pos_range{};
        std::array<glm::vec4, kMaxLocalLights> local_light_color_intensity{};
        int local_light_count = 0;
        int point_shadow_requested = 0;
        for (const renderer::LightData& light : lights_) {
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
            ++local_light_count;
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
        const int point_shadow_selected =
            std::min(point_shadow_requested, std::max(0, shadow_semantics.point_max_shadow_lights));
        const char* point_shadow_reason = "point_shadow_pass_unimplemented";
        if (shadow_semantics.point_max_shadow_lights <= 0) {
            point_shadow_reason = "point_shadows_disabled_by_cap";
        } else if (point_shadow_selected <= 0) {
            point_shadow_reason = "point_shadow_no_shadow_lights";
        }
        KARMA_TRACE_CHANGED(
            "render.diligent",
            std::to_string(layer) + ":" +
                std::to_string(point_shadow_requested) + ":" +
                std::to_string(point_shadow_selected) + ":" +
                point_shadow_reason,
            "point shadow status layer={} requested={} selected={} active={} reason={}",
            layer,
            point_shadow_requested,
            point_shadow_selected,
            0,
            point_shadow_reason);
        int shadow_covered = -1;
        if (shadow_map.ready && !shadow_map.depth.empty()) {
            int covered = 0;
            for (float value : shadow_map.depth) {
                if (std::isfinite(value) && value < 0.999f) {
                    ++covered;
                }
            }
            shadow_covered = covered;
        }
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
            constants.u_model = item.transform;
            const detail::ResolvedMaterialLighting lighting =
                detail::ResolveMaterialLighting(semantics, light_, environment_semantics, 1.0f);
            if (!detail::ValidateResolvedMaterialLighting(lighting)) {
                continue;
            }
            constants.u_color = lighting.color;
            const glm::vec3 lighting_direction = -light_.direction;
            constants.u_lightDir = glm::vec4(lighting_direction, 0.0f);
            constants.u_lightColor = lighting.light_color;
            constants.u_ambientColor = lighting.ambient_color;
            constants.u_unlit = glm::vec4(light_.unlit, 0.0f, 0.0f, 0.0f);
            constants.u_shadowParams0 = shadow_params0;
            constants.u_shadowParams1 = shadow_params1;
            constants.u_shadowParams2 = shadow_params2;
            constants.u_shadowBiasParams = shadow_bias_params;
            constants.u_shadowAxisRight = shadow_axis_right;
            constants.u_shadowAxisUp = shadow_axis_up;
            constants.u_shadowAxisForward = shadow_axis_forward;
            constants.u_localLightCount = local_light_count_uniform;
            constants.u_localLightParams = local_light_params_uniform;
            constants.u_localLightPosRange = local_light_pos_range;
            constants.u_localLightColorIntensity = local_light_color_intensity;
            if (semantics.alpha_blend) {
                constants.u_shadowParams0.x = 0.0f;
            }

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
                if (auto* shadowVar = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_shadow")) {
                    auto shadow_view = shadow_tex_ready_ ? shadow_srv_ : white_srv_;
                    if (!shadow_view) {
                        shadow_view = white_srv_;
                    }
                    shadowVar->Set(shadow_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
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
        if (shadow_map.ready) {
            const char* shadow_source = shadow_map.depth.empty() ? "gpu_projection" : "cpu_raster";
            KARMA_TRACE_CHANGED(
                "render.diligent",
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

    void setViewport(uint32_t width, uint32_t height) {
        if (!context_) {
            return;
        }
        const uint32_t clamped_width = std::max(1u, width);
        const uint32_t clamped_height = std::max(1u, height);
        Diligent::Viewport viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(clamped_width);
        viewport.Height = static_cast<float>(clamped_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->SetViewports(1, &viewport, clamped_width, clamped_height);
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
    float3 WorldPos : TEXCOORD1;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2; // inv_depth_range, pcf_radius, clip_depth_scale, clip_depth_bias
    float4 u_shadowBiasParams; // receiver_scale, normal_scale, raster_depth_bias, raster_slope_bias
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
    float4 u_localLightCount;
    float4 u_localLightParams;
    float4 u_localLightPosRange[4];
    float4 u_localLightColorIntensity[4];
};
PSInput main(VSInput input) {
    PSInput outp;
    outp.Pos = mul(u_modelViewProj, float4(input.Pos, 1.0));
    float4 worldPos = mul(u_model, float4(input.Pos, 1.0));
    outp.WorldPos = worldPos.xyz;
    outp.Nor = mul((float3x3)u_model, input.Nor);
    outp.UV = input.UV;
    return outp;
}
)";

        const char* ps_source = R"(
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2;
    float4 u_shadowBiasParams;
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
    float4 u_localLightCount;
    float4 u_localLightParams;
    float4 u_localLightPosRange[4];
    float4 u_localLightColorIntensity[4];
};
Texture2D s_tex;
SamplerState s_tex_sampler;
Texture2D s_normal;
SamplerState s_normal_sampler;
Texture2D s_occlusion;
SamplerState s_occlusion_sampler;
Texture2D s_shadow;
SamplerState s_shadow_sampler;

float sampleShadowVisibility(float3 worldPos, float ndotl) {
    if (u_shadowParams0.x < 0.5) {
        return 1.0;
    }
    if (ndotl <= 0.0001) {
        return 1.0;
    }

    float extent = max(u_shadowParams1.x, 0.001);
    float centerX = u_shadowParams1.y;
    float centerY = u_shadowParams1.z;
    float minDepth = u_shadowParams1.w;
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float radius = clamp(u_shadowParams2.y, 0.0, 4.0);
    float invMapSize = max(u_shadowParams0.w, 0.0);
    float slope = clamp(1.0 - ndotl, 0.0, 1.0);
    float bias = clamp(
        u_shadowParams0.z +
            (u_shadowParams0.w * (u_shadowBiasParams.x + (u_shadowBiasParams.y * slope))),
        0.0,
        0.02);

    float lx = dot(worldPos, u_shadowAxisRight.xyz);
    float ly = dot(worldPos, u_shadowAxisUp.xyz);
    float lz = dot(worldPos, u_shadowAxisForward.xyz);
    float u = 0.5 + ((lx - centerX) / (2.0 * extent));
    float v = 0.5 + ((ly - centerY) / (2.0 * extent));
    float depth = (lz - minDepth) * invDepthRange;

    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 || depth < 0.0 || depth > 1.0) {
        return 1.0;
    }

    int iradius = int(clamp(floor(radius + 0.5), 0.0, 4.0));
    if (iradius <= 0) {
        float mapDepth = s_shadow.Sample(s_shadow_sampler, float2(u, v)).r;
        return step(depth - bias, mapDepth);
    }

    float lit = 0.0;
    float count = 0.0;
    if (iradius == 1) {
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                float2 uv = float2(u, v) + float2(float(ox), float(oy)) * invMapSize;
                float mapDepth = s_shadow.Sample(s_shadow_sampler, uv).r;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += step(depth - bias, mapDepth) * w;
                count += w;
            }
        }
    } else if (iradius == 2) {
        for (int oy = -2; oy <= 2; ++oy) {
            for (int ox = -2; ox <= 2; ++ox) {
                float2 uv = float2(u, v) + float2(float(ox), float(oy)) * invMapSize;
                float mapDepth = s_shadow.Sample(s_shadow_sampler, uv).r;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += step(depth - bias, mapDepth) * w;
                count += w;
            }
        }
    } else if (iradius == 3) {
        for (int oy = -3; oy <= 3; ++oy) {
            for (int ox = -3; ox <= 3; ++ox) {
                float2 uv = float2(u, v) + float2(float(ox), float(oy)) * invMapSize;
                float mapDepth = s_shadow.Sample(s_shadow_sampler, uv).r;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += step(depth - bias, mapDepth) * w;
                count += w;
            }
        }
    } else {
        for (int oy = -4; oy <= 4; ++oy) {
            for (int ox = -4; ox <= 4; ++ox) {
                float2 uv = float2(u, v) + float2(float(ox), float(oy)) * invMapSize;
                float mapDepth = s_shadow.Sample(s_shadow_sampler, uv).r;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += step(depth - bias, mapDepth) * w;
                count += w;
            }
        }
    }
    if (count < 0.5) {
        return 1.0;
    }
    return lit / count;
}

float4 main(PSInput input) : SV_TARGET {
    float3 n = normalize(input.Nor);
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    float normalModulation = 1.0;
    if (u_textureMode.x > 0.5) {
        float2 encodedNormal = s_normal.Sample(s_normal_sampler, input.UV).rg * 2.0 - float2(1.0, 1.0);
        float normalResponse = clamp(length(encodedNormal), 0.0, 1.0);
        normalModulation = clamp(1.0 + (0.16 * max(0.0, normalResponse - 0.18)), 1.0, 1.18);
    }
    float ao = 1.0;
    float occlusionModulation = 1.0;
    if (u_textureMode.y > 0.5) {
        ao = clamp(s_occlusion.Sample(s_occlusion_sampler, input.UV).r, 0.0, 1.0);
        occlusionModulation = clamp(0.25 + (0.75 * ao), 0.25, 1.0);
    }
    float combinedModulation = clamp(normalModulation * occlusionModulation, 0.20, 1.20);
    float4 shadedTexColor = float4(clamp(texColor.rgb * combinedModulation, 0.0, 1.0), clamp(texColor.a, 0.0, 1.0));
    float4 baseColor = u_color * shadedTexColor;
    if (u_unlit.x > 0.5) {
        return baseColor;
    }

    float ndotl = max(dot(n, normalize(u_lightDir.xyz)), 0.0);
    float shadowVis = ndotl > 0.0001 ? sampleShadowVisibility(input.WorldPos, ndotl) : 1.0;
    float shadowFactor = clamp(1.0 - (u_shadowParams0.y * (1.0 - shadowVis)), 0.0, 1.0);
    int localCount = int(clamp(floor(u_localLightCount.x + 0.5), 0.0, 4.0));
    float localDamping = max(u_localLightParams.x, 0.0);
    float localRangeExponent = max(u_localLightParams.y, 0.1);
    bool aoAffectsLocal = u_localLightParams.z > 0.5;
    float localShadowLiftStrength = max(u_localLightParams.w, 0.0);
    float3 localLightAccum = float3(0.0, 0.0, 0.0);
    float localShadowLiftEnergy = 0.0;
    for (int i = 0; i < 4; ++i) {
        if (i >= localCount) {
            continue;
        }
        float3 toLight = u_localLightPosRange[i].xyz - input.WorldPos;
        float lightDistance = length(toLight);
        float lightRange = max(u_localLightPosRange[i].w, 0.001);
        if (lightDistance >= lightRange || lightDistance <= 1e-5) {
            continue;
        }
        float3 localDir = toLight / lightDistance;
        float localNdotL = max(dot(n, localDir), 0.0);
        if (localNdotL <= 0.0) {
            continue;
        }
        float rangeAttenuation = pow(saturate(1.0 - (lightDistance / lightRange)), localRangeExponent);
        float distanceAttenuation = 1.0 / (1.0 + (localDamping * lightDistance * lightDistance));
        float attenuation = rangeAttenuation * distanceAttenuation;
        float aoMod = aoAffectsLocal ? ao : 1.0;
        float localScalar = u_localLightColorIntensity[i].a * attenuation * localNdotL * aoMod;
        localLightAccum += baseColor.rgb * u_localLightColorIntensity[i].rgb * localScalar;
        float localLuminance = dot(u_localLightColorIntensity[i].rgb, float3(0.2126, 0.7152, 0.0722));
        localShadowLiftEnergy += localLuminance * localScalar;
    }

    float shadowLift = 1.0 - exp(-localShadowLiftEnergy * localShadowLiftStrength);
    float liftedShadowFactor = lerp(shadowFactor, 1.0, saturate(shadowLift));
    float3 lit = baseColor.rgb * (u_ambientColor.rgb + u_lightColor.rgb * ndotl * liftedShadowFactor);
    lit += localLightAccum;
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

    void createShadowDepthPipeline() {
        if (!device_ || !constant_buffer_) {
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
struct VSOutput {
    float4 Pos : SV_POSITION;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2;
    float4 u_shadowBiasParams;
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
};
VSOutput main(VSInput input) {
    VSOutput outp;
    float4 worldPos = mul(u_model, float4(input.Pos, 1.0));
    float3 worldNormal = normalize(mul((float3x3)u_model, input.Nor));
    float extent = max(u_shadowParams1.x, 0.001);
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float invMapSize = max(u_shadowParams0.w, 0.0);
    float lx = dot(worldPos.xyz, u_shadowAxisRight.xyz);
    float ly = dot(worldPos.xyz, u_shadowAxisUp.xyz);
    float lz = dot(worldPos.xyz, u_shadowAxisForward.xyz);
    float depth = clamp((lz - u_shadowParams1.w) * invDepthRange, 0.0, 1.0);
    float slope = clamp(1.0 - abs(dot(worldNormal, -u_shadowAxisForward.xyz)), 0.0, 1.0);
    depth = clamp(
        depth + u_shadowBiasParams.z + (invMapSize * u_shadowBiasParams.w * slope),
        0.0,
        1.0);
    float ndcX = (lx - u_shadowParams1.y) / extent;
    float ndcY = ((ly - u_shadowParams1.z) / extent) * u_shadowAxisUp.w;
    outp.Pos = float4(ndcX, ndcY, depth * u_shadowParams2.z + u_shadowParams2.w, 1.0);
    return outp;
}
)";

        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        shader_ci.Desc.Name = "shadow_depth_vs";
        shader_ci.EntryPoint = "main";
        shader_ci.Source = vs_source;
        device_->CreateShader(shader_ci, &vs);

        if (!vs) {
            return;
        }

        Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
        pso_ci.PSODesc.Name = "shadow_depth_pso";
        pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        pso_ci.GraphicsPipeline.NumRenderTargets = 0;
        pso_ci.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pso_ci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        pso_ci.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS;

        Diligent::LayoutElement layout[] = {
            {0, 0, 3, Diligent::VT_FLOAT32, false},
            {1, 0, 3, Diligent::VT_FLOAT32, false},
            {2, 0, 2, Diligent::VT_FLOAT32, false}
        };
        pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
        pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;
        pso_ci.pVS = vs;
        pso_ci.pPS = nullptr;

        Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_VERTEX, "Constants", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        };
        pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pso_ci.PSODesc.ResourceLayout.Variables = vars;
        pso_ci.PSODesc.ResourceLayout.NumVariables = 1;

        device_->CreateGraphicsPipelineState(pso_ci, &shadow_depth_pso_);
        if (!shadow_depth_pso_) {
            spdlog::error("Diligent: failed to create shadow depth pipeline");
            return;
        }
        if (auto* vs_var = shadow_depth_pso_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
            vs_var->Set(constant_buffer_);
        }
        shadow_depth_pso_->CreateShaderResourceBinding(&shadow_depth_srb_, true);
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
            {Diligent::SHADER_TYPE_PIXEL, "s_shadow", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        Diligent::SamplerDesc sampler_desc{};
        sampler_desc.MinFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
        sampler_desc.MagFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
        sampler_desc.MipFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
        sampler_desc.MaxAnisotropy = kDiligentMaterialMaxAnisotropy;
        sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_WRAP;
        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_normal_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_occlusion_sampler", sampler_desc},
            {Diligent::SHADER_TYPE_PIXEL, "s_shadow_sampler", {
                 Diligent::FILTER_TYPE_LINEAR,
                 Diligent::FILTER_TYPE_LINEAR,
                 Diligent::FILTER_TYPE_LINEAR,
                 Diligent::TEXTURE_ADDRESS_CLAMP,
                 Diligent::TEXTURE_ADDRESS_CLAMP,
                 Diligent::TEXTURE_ADDRESS_CLAMP}},
        };
        pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pso_ci.PSODesc.ResourceLayout.Variables = vars;
        pso_ci.PSODesc.ResourceLayout.NumVariables = 4;
        pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 4;

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

    bool ensureShadowTexture(uint32_t size, bool render_target) {
        if (!device_ || size == 0u) {
            return false;
        }
        const bool needs_recreate =
            !shadow_tex_ || shadow_tex_size_ != size || shadow_tex_is_rt_ != render_target;
        if (needs_recreate) {
            shadow_tex_ = nullptr;
            shadow_srv_ = nullptr;
            shadow_rtv_ = nullptr;
            shadow_dsv_ = nullptr;
            shadow_tex_rt_uses_depth_ = false;

            const auto create_shadow_texture =
                [&](Diligent::TEXTURE_FORMAT format,
                    Diligent::BIND_FLAGS bind_flags,
                    bool rt_uses_depth) -> bool {
                Diligent::TextureDesc desc{};
                desc.Name = "shadow_depth_tex";
                desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                desc.Width = size;
                desc.Height = size;
                desc.MipLevels = 1;
                desc.Format = format;
                desc.BindFlags = bind_flags;
                desc.Usage = Diligent::USAGE_DEFAULT;
                device_->CreateTexture(desc, nullptr, &shadow_tex_);
                if (!shadow_tex_) {
                    return false;
                }
                shadow_srv_ = shadow_tex_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                if (render_target) {
                    if (rt_uses_depth) {
                        shadow_dsv_ = shadow_tex_->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
                    } else {
                        shadow_rtv_ = shadow_tex_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
                    }
                }
                const bool views_ready =
                    shadow_srv_ &&
                    (!render_target ||
                     (rt_uses_depth ? static_cast<bool>(shadow_dsv_) : static_cast<bool>(shadow_rtv_)));
                if (!views_ready) {
                    shadow_tex_ = nullptr;
                    shadow_srv_ = nullptr;
                    shadow_rtv_ = nullptr;
                    shadow_dsv_ = nullptr;
                    return false;
                }
                shadow_tex_size_ = size;
                shadow_tex_is_rt_ = render_target;
                shadow_tex_rt_uses_depth_ = render_target && rt_uses_depth;
                return true;
            };

            bool created = false;
            if (render_target) {
                created = create_shadow_texture(
                    Diligent::TEX_FORMAT_D32_FLOAT,
                    Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_DEPTH_STENCIL,
                    true);
                if (!created) {
                    KARMA_TRACE("render.diligent", "shadow depth attachment create failed; using color-min fallback");
                }
            }
            if (!created) {
                created = create_shadow_texture(
                    Diligent::TEX_FORMAT_R32_FLOAT,
                    Diligent::BIND_SHADER_RESOURCE |
                        (render_target ? Diligent::BIND_RENDER_TARGET : Diligent::BIND_NONE),
                    false);
            }
            if (!created) {
                shadow_tex_size_ = 0u;
                shadow_tex_is_rt_ = false;
                shadow_tex_rt_uses_depth_ = false;
            }
        }

        if (!shadow_tex_ || !shadow_srv_) {
            return false;
        }
        if (render_target && !shadow_rtv_ && !shadow_dsv_) {
            return false;
        }
        return true;
    }

    bool renderGpuShadowMap(const detail::DirectionalShadowMap& map, const std::vector<RenderableDraw>& renderables) {
        if (!context_ || !map.ready || map.size <= 0 || !shadow_depth_pso_) {
            return false;
        }
        const uint32_t size = static_cast<uint32_t>(map.size);
        if (!ensureShadowTexture(size, true)) {
            return false;
        }

        const bool depth_attachment = shadow_tex_rt_uses_depth_;
        if (depth_attachment) {
            auto* shadow_dsv = shadow_dsv_.RawPtr();
            context_->SetRenderTargets(0, nullptr, shadow_dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            context_->ClearDepthStencil(
                shadow_dsv,
                Diligent::CLEAR_DEPTH_FLAG,
                1.0f,
                0,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        } else {
            auto* shadow_rtv = shadow_rtv_.RawPtr();
            context_->SetRenderTargets(1, &shadow_rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            const float clear[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            context_->ClearRenderTarget(shadow_rtv, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        setViewport(size, size);
        context_->SetPipelineState(shadow_depth_pso_);
        if (shadow_depth_srb_) {
            context_->CommitShaderResources(shadow_depth_srb_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        const detail::ShadowClipDepthTransform clip_depth =
            detail::ResolveShadowClipDepthTransform(false);
        const float clip_y_sign = detail::ResolveShadowClipYSign(false);
        const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
            detail::ResolveDirectionalShadowSemantics(light_);
        const float inv_depth_range = map.depth_range > 1e-6f ? (1.0f / map.depth_range) : 1.0f;
        const glm::vec4 shadow_params0{
            1.0f,
            map.strength,
            map.bias,
            map.size > 0 ? (1.0f / static_cast<float>(map.size)) : 0.0f,
        };
        const glm::vec4 shadow_params1{
            map.extent,
            map.center_x,
            map.center_y,
            map.min_depth,
        };
        const glm::vec4 shadow_params2{
            inv_depth_range,
            static_cast<float>(map.pcf_radius),
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
            map.axis_right.x,
            map.axis_right.y,
            map.axis_right.z,
            0.0f,
        };
        const glm::vec4 shadow_axis_up{
            map.axis_up.x,
            map.axis_up.y,
            map.axis_up.z,
            clip_y_sign,
        };
        const glm::vec4 shadow_axis_forward{
            map.axis_forward.x,
            map.axis_forward.y,
            map.axis_forward.z,
            0.0f,
        };

        std::size_t submitted = 0u;
        std::size_t culled = 0u;
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
                if (!intersectsDirectionalShadowMap(map, world_center, world_radius)) {
                    ++culled;
                    continue;
                }
            }

            Constants constants{};
            constants.u_model = item.transform;
            constants.u_shadowParams0 = shadow_params0;
            constants.u_shadowParams1 = shadow_params1;
            constants.u_shadowParams2 = shadow_params2;
            constants.u_shadowBiasParams = shadow_bias_params;
            constants.u_shadowAxisRight = shadow_axis_right;
            constants.u_shadowAxisUp = shadow_axis_up;
            constants.u_shadowAxisForward = shadow_axis_forward;
            Diligent::MapHelper<Constants> cb_data(
                context_,
                constant_buffer_,
                Diligent::MAP_WRITE,
                Diligent::MAP_FLAG_DISCARD);
            *cb_data = constants;

            const uint64_t offsets[] = {0};
            Diligent::IBuffer* vbs[] = {renderable.mesh->vb};
            context_->SetVertexBuffers(
                0,
                1,
                vbs,
                offsets,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            context_->SetIndexBuffer(renderable.mesh->ib, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawIndexedAttribs draw{};
            draw.IndexType = Diligent::VT_UINT32;
            draw.NumIndices = renderable.mesh->num_indices;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            context_->DrawIndexed(draw);
            ++submitted;
        }

        if (shadow_tex_) {
            Diligent::StateTransitionDesc barrier{};
            barrier.pResource = shadow_tex_;
            barrier.OldState = depth_attachment
                ? Diligent::RESOURCE_STATE_DEPTH_WRITE
                : Diligent::RESOURCE_STATE_RENDER_TARGET;
            barrier.NewState = Diligent::RESOURCE_STATE_SHADER_RESOURCE;
            barrier.Flags = Diligent::STATE_TRANSITION_FLAG_UPDATE_STATE;
            context_->TransitionResourceStates(1, &barrier);
        }

        KARMA_TRACE_CHANGED(
            "render.diligent",
            std::to_string(map.size) + ":" + std::to_string(submitted) + ":" +
                std::to_string(culled) + ":" +
                (depth_attachment ? "depth" : "color_min"),
            "gpu shadow pass size={} draws={} culled={} attachment={}",
            map.size,
            submitted,
            culled,
            depth_attachment ? "depth" : "color_min");
        return submitted > 0u;
    }

    void updateShadowTexture(const detail::DirectionalShadowMap& map) {
        shadow_tex_ready_ = false;
        if (!context_ || !map.ready || map.size <= 0 || map.depth.empty()) {
            return;
        }
        const uint32_t size = static_cast<uint32_t>(map.size);
        if (!ensureShadowTexture(size, false)) {
            return;
        }
        const std::size_t expected = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
        std::vector<float> upload_data(expected, 1.0f);
        for (std::size_t i = 0; i < std::min(expected, map.depth.size()); ++i) {
            const float value = map.depth[i];
            upload_data[i] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
        }

        Diligent::TextureSubResData subres{};
        subres.pData = upload_data.data();
        subres.Stride = static_cast<Diligent::Uint64>(size * sizeof(float));
        Diligent::Box box{};
        box.MinX = 0;
        box.MinY = 0;
        box.MinZ = 0;
        box.MaxX = size;
        box.MaxY = size;
        box.MaxZ = 1;
        context_->UpdateTexture(
            shadow_tex_,
            0,
            0,
            box,
            subres,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        shadow_tex_ready_ = true;
    }

    void resetShadowResources() {
        shadow_tex_ = nullptr;
        shadow_srv_ = nullptr;
        shadow_rtv_ = nullptr;
        shadow_dsv_ = nullptr;
        shadow_tex_size_ = 0u;
        shadow_tex_is_rt_ = false;
        shadow_tex_rt_uses_depth_ = false;
        shadow_tex_ready_ = false;
    }

    Diligent::RefCntAutoPtr<Diligent::ITextureView> createTextureView(const renderer::MeshData::TextureData& tex) {
        Diligent::RefCntAutoPtr<Diligent::ITextureView> view;
        if (tex.width <= 0 || tex.height <= 0) {
            return view;
        }
        const std::vector<detail::Rgba8MipLevel> mip_chain = detail::BuildRgba8MipChain(tex);
        if (mip_chain.empty()) {
            return view;
        }
        const detail::Rgba8MipLevel& base_level = mip_chain.front();
        if (base_level.width <= 0 || base_level.height <= 0 || base_level.pixels.empty()) {
            return view;
        }
        Diligent::TextureDesc desc{};
        desc.Name = "albedo_tex";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<uint32_t>(base_level.width);
        desc.Height = static_cast<uint32_t>(base_level.height);
        desc.MipLevels = static_cast<uint32_t>(mip_chain.size());
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        Diligent::TextureData texData{};
        std::vector<Diligent::TextureSubResData> subresources(mip_chain.size());
        for (std::size_t mip = 0; mip < mip_chain.size(); ++mip) {
            const detail::Rgba8MipLevel& level = mip_chain[mip];
            if (level.width <= 0 || level.height <= 0 || level.pixels.empty()) {
                return view;
            }
            Diligent::TextureSubResData& subres = subresources[mip];
            subres.pData = level.pixels.data();
            subres.Stride = static_cast<uint32_t>(level.width * 4);
        }
        texData.pSubResources = subresources.data();
        texData.NumSubresources = static_cast<uint32_t>(subresources.size());
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
    uint32_t shadow_tex_size_ = 0u;
    bool shadow_tex_is_rt_ = false;
    bool shadow_tex_rt_uses_depth_ = false;
    bool shadow_tex_ready_ = false;
    detail::DirectionalShadowMap cached_shadow_map_{};
    int shadow_update_every_frames_ = 1;
    int shadow_frames_until_update_ = 0;
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

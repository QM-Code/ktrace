#include "karma/renderer/backend.hpp"

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
};

struct Material {
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct Constants {
    glm::mat4 u_modelViewProj;
    glm::vec4 u_color;
    glm::vec4 u_lightDir;
    glm::vec4 u_lightColor;
    glm::vec4 u_ambientColor;
    glm::vec4 u_unlit;
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

        createPipeline();
        white_srv_ = createWhiteTexture();
        if (!pso_ || !swapchain_) {
            return;
        }
        initialized_ = true;
    }

    ~DiligentBackend() override = default;

    void beginFrame(int width, int height, float dt) override {
        (void)dt;
        if (!initialized_) {
            return;
        }
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
        Material out;
        out.color = material.base_color;
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

    void renderFrame() override {
        renderLayer(0);
    }

    void renderLayer(renderer::LayerId layer) override {
        if (!initialized_ || !context_ || !swapchain_ || !pso_) {
            return;
        }

        auto* rtv = swapchain_->GetCurrentBackBufferRTV();
        auto* dsv = swapchain_->GetDepthBufferDSV();
        const float clear[4] = {0.18f, 0.18f, 0.18f, 1.0f};
        context_->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->ClearRenderTarget(rtv, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (dsv) {
            context_->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        context_->SetPipelineState(pso_);

        for (const auto& item : draw_items_) {
            if (item.layer != layer) {
                continue;
            }
            auto mesh_it = meshes_.find(item.mesh);
            auto mat_it = materials_.find(item.material);
            if (mesh_it == meshes_.end() || mat_it == materials_.end()) {
                continue;
            }

            const auto& mesh = mesh_it->second;
            const auto& mat = mat_it->second;

            float aspect = (height_ > 0) ? float(width_) / float(height_) : 1.0f;
            glm::mat4 view = glm::lookAt(camera_.position, camera_.target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(camera_.fov_y_degrees), aspect, camera_.near_clip, camera_.far_clip);
            proj[1][1] *= -1.0f; // Vulkan clip space
            Constants constants{};
            constants.u_modelViewProj = proj * view * item.transform;
            constants.u_color = mat.color;
            constants.u_lightDir = glm::vec4(light_.direction, 0.0f);
            constants.u_lightColor = light_.color;
            constants.u_ambientColor = light_.ambient;
            constants.u_unlit = glm::vec4(light_.unlit, 0.0f, 0.0f, 0.0f);

            Diligent::MapHelper<Constants> cb_data(context_, constant_buffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
            *cb_data = constants;

            if (srb_) {
                if (auto* texVar = srb_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_tex")) {
                    texVar->Set(mesh.srv, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                }
            }
            context_->CommitShaderResources(srb_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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

        // TODO(bz3-rewrite): keep per-layer queues so multiple layers can render in a frame.
        draw_items_.clear();
    }

    void setCamera(const renderer::CameraData& camera) override {
        camera_ = camera;
    }

    void setDirectionalLight(const renderer::DirectionalLightData& light) override {
        light_ = light;
    }

    bool isValid() const override {
        return initialized_ && device_ && context_ && swapchain_;
    }

 private:
    void createPipeline() {
        Diligent::ShaderCreateInfo shader_ci{};
        shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        // No combined sampler flag needed for this minimal shader.

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
};
Texture2D s_tex;
SamplerState s_tex_sampler;
float4 main(PSInput input) : SV_TARGET {
    float3 n = normalize(input.Nor);
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    float4 baseColor = u_color * texColor;
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

        Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
        pso_ci.PSODesc.Name = "simple_pso";
        pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        pso_ci.GraphicsPipeline.NumRenderTargets = 1;
        pso_ci.GraphicsPipeline.RTVFormats[0] = swapchain_->GetDesc().ColorBufferFormat;
        pso_ci.GraphicsPipeline.DSVFormat = swapchain_->GetDesc().DepthBufferFormat;
        pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        pso_ci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_BACK;

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
            {Diligent::SHADER_TYPE_PIXEL, "s_tex", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", Diligent::SamplerDesc{}}
        };
        pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pso_ci.PSODesc.ResourceLayout.Variables = vars;
        pso_ci.PSODesc.ResourceLayout.NumVariables = 1;
        pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

        device_->CreateGraphicsPipelineState(pso_ci, &pso_);

        Diligent::BufferDesc cb_desc{};
        cb_desc.Name = "constants";
        cb_desc.Size = sizeof(Constants);
        cb_desc.Usage = Diligent::USAGE_DYNAMIC;
        cb_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cb_desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cb_desc, nullptr, &constant_buffer_);
        if (pso_ && constant_buffer_) {
            if (auto* vs_var = pso_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
                vs_var->Set(constant_buffer_);
            } else {
                KARMA_TRACE("render.diligent", "Diligent: missing VS static var 'Constants'");
            }
            if (auto* ps_var = pso_->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
                ps_var->Set(constant_buffer_);
            } else {
                KARMA_TRACE("render.diligent", "Diligent: missing PS static var 'Constants'");
            }
        }
        if (pso_) {
            pso_->CreateShaderResourceBinding(&srb_, true);
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
        if (tex.pixels.empty() || tex.width <= 0 || tex.height <= 0) {
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
        subres.pData = tex.pixels.data();
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
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constant_buffer_;

    std::unordered_map<renderer::MeshId, Mesh> meshes_;
    std::unordered_map<renderer::MaterialId, Material> materials_;
    std::vector<renderer::DrawItem> draw_items_;

    renderer::CameraData camera_{};
    renderer::DirectionalLightData light_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> white_srv_;
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateBackend(karma::platform::Window& window) {
#if defined(KARMA_RENDER_BACKEND_DILIGENT)
    return std::make_unique<DiligentBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace karma::renderer_backend

/*
 * RmlUi Diligent renderer stub for KARMA.
 * TODO: Implement actual Diligent rendering.
 */
#include "karma_extras/ui/platform/rmlui/renderer_diligent.hpp"

#include "karma/graphics/backends/diligent/ui_bridge.hpp"

#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineResourceSignature.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

struct UiVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
};

struct UiConstants {
    Diligent::float4x4 transform;
    Diligent::float4 translate;
};

template <typename ColorT>
uint32_t packColor(const ColorT& color) {
    return static_cast<uint32_t>(color.red)
        | (static_cast<uint32_t>(color.green) << 8)
        | (static_cast<uint32_t>(color.blue) << 16)
        | (static_cast<uint32_t>(color.alpha) << 24);
}
} // namespace

void RenderInterface_Diligent::SetViewport(int width, int height, int offset_x, int offset_y) {
    viewport_width = width > 0 ? width : 1;
    viewport_height = height > 0 ? height : 1;
    viewport_offset_x = offset_x;
    viewport_offset_y = offset_y;
    projection = Rml::Matrix4f::ProjectOrtho(0, static_cast<float>(viewport_width),
                                             static_cast<float>(viewport_height), 0,
                                             -10000, 10000);
    transform = projection;
    ensureRenderTarget(viewport_width, viewport_height);
}

void RenderInterface_Diligent::BeginFrame() {
    ensurePipeline();
    ensureRenderTarget(viewport_width, viewport_height);
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (ctx.context && uiTargetRtv_) {
        ctx.context->SetRenderTargets(1, &uiTargetRtv_, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        ctx.context->ClearRenderTarget(uiTargetRtv_, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    debug_draw_calls = 0;
    debug_triangles = 0;
    debug_frame++;
}

void RenderInterface_Diligent::EndFrame() {
    if (debug_frame % 120 == 0) {
        spdlog::info("RmlUi(Diligent): frame {} draw_calls={} tris={}",
                     debug_frame, debug_draw_calls, debug_triangles);
    }
}

Rml::CompiledGeometryHandle RenderInterface_Diligent::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                      Rml::Span<const int> indices) {
    if (vertices.empty() || indices.empty()) {
        return {};
    }

    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device) {
        return {};
    }

    std::vector<UiVertex> packed_vertices;
    packed_vertices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        packed_vertices[i].x = vertices[i].position.x;
        packed_vertices[i].y = vertices[i].position.y;
        packed_vertices[i].u = vertices[i].tex_coord.x;
        packed_vertices[i].v = vertices[i].tex_coord.y;
        packed_vertices[i].color = packColor(vertices[i].colour);
    }

    std::vector<uint32_t> packed_indices;
    packed_indices.resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        packed_indices[i] = static_cast<uint32_t>(indices[i]);
    }

    auto* geometry = new GeometryData();
    geometry->indexCount = static_cast<uint32_t>(packed_indices.size());

    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "RmlUi Diligent VB";
    vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vbDesc.Size = static_cast<Diligent::Uint32>(packed_vertices.size() * sizeof(UiVertex));
    Diligent::BufferData vbData;
    vbData.pData = packed_vertices.data();
    vbData.DataSize = vbDesc.Size;
    ctx.device->CreateBuffer(vbDesc, &vbData, &geometry->vertexBuffer);

    Diligent::BufferDesc ibDesc;
    ibDesc.Name = "RmlUi Diligent IB";
    ibDesc.Usage = Diligent::USAGE_IMMUTABLE;
    ibDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    ibDesc.Size = static_cast<Diligent::Uint32>(packed_indices.size() * sizeof(uint32_t));
    Diligent::BufferData ibData;
    ibData.pData = packed_indices.data();
    ibData.DataSize = ibDesc.Size;
    ctx.device->CreateBuffer(ibDesc, &ibData, &geometry->indexBuffer);

    if (!geometry->vertexBuffer || !geometry->indexBuffer) {
        delete geometry;
        return {};
    }

    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void RenderInterface_Diligent::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                              Rml::Vector2f translation,
                                              Rml::TextureHandle texture) {
    if (!handle) {
        return;
    }

    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || !ctx.context || !pipeline_) {
        return;
    }
    if (!uiTargetRtv_) {
        return;
    }

    if (texture == TexturePostprocess) {
        return;
    }

    ensureWhiteTexture();

    GeometryData* geometry = reinterpret_cast<GeometryData*>(handle);
    if (!geometry->vertexBuffer || !geometry->indexBuffer || geometry->indexCount == 0) {
        return;
    }

    const Diligent::Uint64 vbOffset = 0;
    Diligent::IBuffer* vbs[] = {geometry->vertexBuffer};
    ctx.context->SetVertexBuffers(0, 1, vbs, &vbOffset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx.context->SetIndexBuffer(geometry->indexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::ITextureView* rtv = uiTargetRtv_;
    ctx.context->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::Viewport vp{};
    vp.TopLeftX = static_cast<float>(viewport_offset_x);
    vp.TopLeftY = static_cast<float>(viewport_offset_y);
    vp.Width = static_cast<float>(viewport_width);
    vp.Height = static_cast<float>(viewport_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx.context->SetViewports(1, &vp, 0, 0);

    if (scissor_enabled && scissor_region.Valid()) {
        const int x = std::max(0, scissor_region.p0.x);
        const int y = std::max(0, scissor_region.p0.y);
        const int w = std::max(0, scissor_region.Width());
        const int h = std::max(0, scissor_region.Height());
        Diligent::Rect rect{ x, y, x + w, y + h };
        ctx.context->SetScissorRects(1, &rect, 0, 0);
    } else {
        Diligent::Rect rect{0, 0, viewport_width, viewport_height};
        ctx.context->SetScissorRects(1, &rect, 0, 0);
    }

    UiConstants constants{};
    std::memcpy(constants.transform.m, transform.data(), sizeof(constants.transform.m));
    constants.translate = Diligent::float4{translation.x, translation.y, 0.0f, 0.0f};
    {
        Diligent::MapHelper<UiConstants> cb(ctx.context, constantBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        *cb = constants;
    }

    bool use_texture = (texture != 0);
    const TextureData* tex_data = nullptr;
    if (use_texture && texture != RenderInterface_Diligent::TextureEnableWithoutBinding) {
        tex_data = lookupTexture(texture);
        if (!tex_data || !tex_data->srv) {
            use_texture = false;
        } else {
            last_texture = texture;
        }
    } else if (use_texture && texture == RenderInterface_Diligent::TextureEnableWithoutBinding) {
        tex_data = lookupTexture(last_texture);
        if (!tex_data || !tex_data->srv) {
            use_texture = false;
        }
    }

    Diligent::ITextureView* srv = whiteTextureView_;
    if (use_texture && tex_data && tex_data->srv) {
        srv = tex_data->srv;
    }
    if (!srv) {
        return;
    }

    if (auto* var = shaderBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")) {
        var->Set(srv);
    }

    ctx.context->SetPipelineState(pipeline_);
    ctx.context->CommitShaderResources(shaderBinding_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::DrawIndexedAttribs drawAttrs{};
    drawAttrs.IndexType = Diligent::VT_UINT32;
    drawAttrs.NumIndices = geometry->indexCount;
    drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
    ctx.context->DrawIndexed(drawAttrs);
    debug_draw_calls++;
    debug_triangles += geometry->indexCount / 3;
}

void RenderInterface_Diligent::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    if (!handle) {
        return;
    }
    auto* geometry = reinterpret_cast<GeometryData*>(handle);
    delete geometry;
}

Rml::TextureHandle RenderInterface_Diligent::LoadTexture(Rml::Vector2i& texture_dimensions,
                                                         const Rml::String& source) {
    const Rml::String tex_prefix = "texid:";
    if (source.rfind(tex_prefix, 0) == 0) {
        const char* id_str = source.c_str() + tex_prefix.size();
        char* end_ptr = nullptr;
        const unsigned long token = std::strtoul(id_str, &end_ptr, 10);
        if (token == 0) {
            return {};
        }
        int width = 1;
        int height = 1;
        if (end_ptr && *end_ptr == ':') {
            int parsed_w = 0;
            int parsed_h = 0;
            if (std::sscanf(end_ptr + 1, "%dx%d", &parsed_w, &parsed_h) == 2) {
                if (parsed_w > 0 && parsed_h > 0) {
                    width = parsed_w;
                    height = parsed_h;
                }
            }
        }
        TextureData entry;
        entry.external = true;
        entry.width = width;
        entry.height = height;
        entry.srv = graphics_backend::diligent_ui::ResolveExternalTexture(token);
        if (!entry.srv) {
            return {};
        }
        const Rml::TextureHandle handle = next_texture_id++;
        textures.emplace(handle, entry);
        texture_dimensions.x = entry.width;
        texture_dimensions.y = entry.height;
        return handle;
    }

    Rml::FileInterface* file_interface = Rml::GetFileInterface();
    Rml::FileHandle file_handle = file_interface->Open(source);
    if (!file_handle) {
        return {};
    }

    file_interface->Seek(file_handle, 0, SEEK_END);
    size_t buffer_size = file_interface->Tell(file_handle);
    file_interface->Seek(file_handle, 0, SEEK_SET);
    if (buffer_size == 0) {
        file_interface->Close(file_handle);
        return {};
    }

    std::vector<uint8_t> buffer;
    buffer.resize(buffer_size);
    file_interface->Read(buffer.data(), buffer_size, file_handle);
    file_interface->Close(file_handle);

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()),
                                            &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return {};
    }

    Rml::TextureHandle handle = GenerateTexture(
        Rml::Span<const Rml::byte>(reinterpret_cast<const Rml::byte*>(pixels),
                                   static_cast<size_t>(width * height * 4)),
        Rml::Vector2i(width, height));
    stbi_image_free(pixels);
    texture_dimensions.x = width;
    texture_dimensions.y = height;
    return handle;
}

Rml::TextureHandle RenderInterface_Diligent::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                                             Rml::Vector2i source_dimensions) {
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || source_data.empty() || source_dimensions.x <= 0 || source_dimensions.y <= 0) {
        return {};
    }

    TextureData data;
    data.width = source_dimensions.x;
    data.height = source_dimensions.y;
    data.external = false;

    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(data.width);
    desc.Height = static_cast<Diligent::Uint32>(data.height);
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.Name = "RmlUi Diligent Texture";

    Diligent::TextureData initData;
    Diligent::TextureSubResData sub{};
    sub.pData = source_data.data();
    sub.Stride = static_cast<Diligent::Uint32>(data.width * 4);
    initData.pSubResources = &sub;
    initData.NumSubresources = 1;
    ctx.device->CreateTexture(desc, &initData, &data.texture);
    if (!data.texture) {
        return {};
    }
    data.srv = data.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!data.srv) {
        return {};
    }

    const Rml::TextureHandle handle = next_texture_id++;
    textures.emplace(handle, data);
    return handle;
}

void RenderInterface_Diligent::ReleaseTexture(Rml::TextureHandle texture_handle) {
    textures.erase(texture_handle);
}

void RenderInterface_Diligent::SetTransform(const Rml::Matrix4f* new_transform) {
    transform = (new_transform ? (projection * (*new_transform)) : projection);
}

void RenderInterface_Diligent::ensurePipeline() {
    if (pipeline_) {
        return;
    }
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || !ctx.swapChain) {
        return;
    }

    const char* vsSource = R"(
cbuffer UiConstants
{
    float4x4 g_Transform;
    float4 g_Translate;
};
struct VSInput
{
    float2 Pos : ATTRIB0;
    float2 UV : ATTRIB1;
    float4 Color : ATTRIB2;
};
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    float2 pos = In.Pos + g_Translate.xy;
    Out.Pos = mul(g_Transform, float4(pos, 0.0, 1.0));
    Out.UV = In.UV;
    Out.Color = In.Color;
    return Out;
}
)";

    const char* psSource = R"(
Texture2D g_Texture;
SamplerState g_Texture_sampler;
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};
float4 main(PSInput In) : SV_Target
{
    float4 tex = g_Texture.Sample(g_Texture_sampler, In.UV);
    return tex * In.Color;
}
)";

    Diligent::ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "RmlUi Diligent VS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = vsSource;
    ctx.device->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "RmlUi Diligent PS";
    shaderCI.EntryPoint = "main";
    shaderCI.Source = psSource;
    ctx.device->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("RmlUi(Diligent): failed to create shaders");
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "RmlUi Diligent PSO";
    psoCI.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    psoCI.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    psoCI.GraphicsPipeline.RasterizerDesc.ScissorEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    const auto& scDesc = ctx.swapChain->GetDesc();
    psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;

    auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = true;
    rt0.SrcBlend = Diligent::BLEND_FACTOR_ONE;
    rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
    rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;

    Diligent::LayoutElement layout[] = {
        {0, 0, 2, Diligent::VT_FLOAT32, false},
        {1, 0, 2, Diligent::VT_FLOAT32, false},
        {2, 0, 4, Diligent::VT_UINT8, true}
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(std::size(layout));

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(std::size(vars));

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;

    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture_sampler", samplerDesc}
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(std::size(samplers));

    ctx.device->CreatePipelineState(psoCI, &pipeline_);
    if (!pipeline_) {
        spdlog::error("RmlUi(Diligent): failed to create pipeline state");
        return;
    }

    Diligent::BufferDesc cbDesc;
    cbDesc.Name = "RmlUi Diligent CB";
    cbDesc.Size = sizeof(UiConstants);
    cbDesc.Usage = Diligent::USAGE_DYNAMIC;
    cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    ctx.device->CreateBuffer(cbDesc, nullptr, &constantBuffer_);
    if (constantBuffer_) {
        if (auto* var = pipeline_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "UiConstants")) {
            var->Set(constantBuffer_);
        }
    }

    pipeline_->CreateShaderResourceBinding(&shaderBinding_, true);
}

void RenderInterface_Diligent::ensureRenderTarget(int width, int height) {
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || !ctx.swapChain) {
        return;
    }
    if (width <= 0 || height <= 0) {
        if (uiToken_ != 0) {
            graphics_backend::diligent_ui::UnregisterExternalTexture(uiToken_);
            uiToken_ = 0;
        }
        uiTargetSrv_ = nullptr;
        uiTargetRtv_ = nullptr;
        uiTargetTexture_ = nullptr;
        uiWidth_ = 0;
        uiHeight_ = 0;
        return;
    }
    if (width == uiWidth_ && height == uiHeight_ && uiTargetTexture_) {
        return;
    }

    if (uiToken_ != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(uiToken_);
        uiToken_ = 0;
    }
    uiTargetSrv_ = nullptr;
    uiTargetRtv_ = nullptr;
    uiTargetTexture_ = nullptr;

    const auto& scDesc = ctx.swapChain->GetDesc();
    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format = scDesc.ColorBufferFormat;
    desc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    desc.Name = "RmlUi Diligent UI RT";
    ctx.device->CreateTexture(desc, nullptr, &uiTargetTexture_);
    if (!uiTargetTexture_) {
        return;
    }

    uiTargetRtv_ = uiTargetTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    uiTargetSrv_ = uiTargetTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!uiTargetRtv_ || !uiTargetSrv_) {
        uiTargetTexture_ = nullptr;
        uiTargetRtv_ = nullptr;
        uiTargetSrv_ = nullptr;
        return;
    }

    uiToken_ = graphics_backend::diligent_ui::RegisterExternalTexture(uiTargetSrv_);
    uiWidth_ = width;
    uiHeight_ = height;
}

void RenderInterface_Diligent::ensureWhiteTexture() {
    if (whiteTexture_) {
        return;
    }
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device) {
        return;
    }
    const uint32_t white = 0xffffffffu;
    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.Name = "RmlUi White Texture";

    Diligent::TextureData initData;
    Diligent::TextureSubResData sub{};
    sub.pData = &white;
    sub.Stride = sizeof(white);
    initData.pSubResources = &sub;
    initData.NumSubresources = 1;
    ctx.device->CreateTexture(desc, &initData, &whiteTexture_);
    if (whiteTexture_) {
        whiteTextureView_ = whiteTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

const RenderInterface_Diligent::TextureData* RenderInterface_Diligent::lookupTexture(Rml::TextureHandle handle) const {
    auto it = textures.find(handle);
    if (it == textures.end()) {
        return nullptr;
    }
    return &it->second;
}

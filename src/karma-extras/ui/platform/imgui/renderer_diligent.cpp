#include "karma_extras/ui/platform/imgui/renderer_diligent.hpp"

#include <imgui.h>

#include "karma/graphics/backends/diligent/ui_bridge.hpp"
#include "spdlog/spdlog.h"
#include "karma_extras/ui/imgui/texture_utils.hpp"

#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace graphics_backend {
namespace {

struct ImGuiVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
};

struct ImGuiConstants {
    float scaleBias[4];
};

} // namespace

DiligentRenderer::DiligentRenderer() = default;

DiligentRenderer::~DiligentRenderer() {
    if (fontToken_ != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(fontToken_);
        fontToken_ = 0;
    }
    if (uiToken_ != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(uiToken_);
        uiToken_ = 0;
    }
    fontSrv_ = nullptr;
    uiTargetSrv_ = nullptr;
    uiTargetRtv_ = nullptr;
    uiTargetTexture_ = nullptr;
    pipeline_ = nullptr;
    shaderBinding_ = nullptr;
    vertexBuffer_ = nullptr;
    indexBuffer_ = nullptr;
    constantBuffer_ = nullptr;
    ready_ = false;
}

void* DiligentRenderer::toImGuiTextureId(const graphics::TextureHandle& texture) const {
    if (!texture.valid()) {
        return nullptr;
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(texture.id));
}

bool DiligentRenderer::isImGuiReady() const {
    return ready_ && fontSrv_;
}

void DiligentRenderer::ensureImGuiRenderTarget(int width, int height) {
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
    desc.Name = "ImGui Diligent UI RT";
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

graphics::TextureHandle DiligentRenderer::getImGuiRenderTarget() const {
    graphics::TextureHandle handle{};
    if (uiToken_ == 0) {
        return handle;
    }
    handle.id = uiToken_;
    handle.width = static_cast<uint32_t>(uiWidth_);
    handle.height = static_cast<uint32_t>(uiHeight_);
    handle.format = graphics::TextureFormat::RGBA8_UNORM;
    return handle;
}

void DiligentRenderer::ensurePipeline() {
    if (pipeline_) {
        return;
    }
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || !ctx.swapChain) {
        return;
    }

    const char* vsSource = R"(
cbuffer ImGuiConstants {
    float4 g_ScaleBias;
};
struct VSInput {
    float2 Pos : ATTRIB0;
    float2 UV : ATTRIB1;
    float4 Color : ATTRIB2;
};
struct PSInput {
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};
PSInput main(VSInput In)
{
    PSInput Out;
    float2 pos = In.Pos * g_ScaleBias.xy + g_ScaleBias.zw;
    Out.Pos = float4(pos, 0.0, 1.0);
    Out.UV = In.UV;
    Out.Color = In.Color;
    return Out;
}
)";

    const char* psSource = R"(
Texture2D g_Texture;
SamplerState g_Texture_sampler;
struct PSInput {
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
    shaderCI.EntryPoint = "main";

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shaderCI.Desc.Name = "ImGui Diligent VS";
    shaderCI.Source = vsSource;
    ctx.device->CreateShader(shaderCI, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shaderCI.Desc.Name = "ImGui Diligent PS";
    shaderCI.Source = psSource;
    ctx.device->CreateShader(shaderCI, &ps);

    if (!vs || !ps) {
        spdlog::error("ImGui(Diligent): failed to create shaders");
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "ImGui Diligent PSO";
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
    rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
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
    psoCI.GraphicsPipeline.InputLayout.NumElements = static_cast<Diligent::Uint32>(sizeof(layout) / sizeof(layout[0]));

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(sizeof(vars) / sizeof(vars[0]));

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
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(sizeof(samplers) / sizeof(samplers[0]));

    ctx.device->CreatePipelineState(psoCI, &pipeline_);
    if (!pipeline_) {
        spdlog::error("ImGui(Diligent): failed to create pipeline state");
        return;
    }

    Diligent::BufferDesc cbDesc;
    cbDesc.Name = "ImGui Diligent CB";
    cbDesc.Size = sizeof(ImGuiConstants);
    cbDesc.Usage = Diligent::USAGE_DYNAMIC;
    cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    ctx.device->CreateBuffer(cbDesc, nullptr, &constantBuffer_);
    if (constantBuffer_) {
        if (auto* var = pipeline_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "ImGuiConstants")) {
            var->Set(constantBuffer_);
        }
    }

    pipeline_->CreateShaderResourceBinding(&shaderBinding_, true);
    ready_ = pipeline_ && shaderBinding_ && constantBuffer_;
}

void DiligentRenderer::ensureBuffers(std::size_t vertexBytes, std::size_t indexBytes) {
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device) {
        return;
    }

    if (!vertexBuffer_ || vertexBufferSize_ < vertexBytes) {
        Diligent::BufferDesc vbDesc;
        vbDesc.Name = "ImGui Diligent VB";
        vbDesc.Usage = Diligent::USAGE_DYNAMIC;
        vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        vbDesc.Size = static_cast<Diligent::Uint32>(vertexBytes);
        ctx.device->CreateBuffer(vbDesc, nullptr, &vertexBuffer_);
        vertexBufferSize_ = vertexBytes;
    }

    if (!indexBuffer_ || indexBufferSize_ < indexBytes) {
        Diligent::BufferDesc ibDesc;
        ibDesc.Name = "ImGui Diligent IB";
        ibDesc.Usage = Diligent::USAGE_DYNAMIC;
        ibDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        ibDesc.Size = static_cast<Diligent::Uint32>(indexBytes);
        ctx.device->CreateBuffer(ibDesc, nullptr, &indexBuffer_);
        indexBufferSize_ = indexBytes;
    }
}

void DiligentRenderer::rebuildImGuiFonts(ImFontAtlas* atlas) {
    if (!atlas) {
        return;
    }
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device) {
        return;
    }

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        spdlog::error("ImGui(Diligent): font texture build failed");
        return;
    }

    if (fontToken_ != 0) {
        graphics_backend::diligent_ui::UnregisterExternalTexture(fontToken_);
        fontToken_ = 0;
    }
    fontSrv_ = nullptr;

    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.Name = "ImGui Diligent Font";

    Diligent::TextureSubResData sub{};
    sub.pData = pixels;
    sub.Stride = static_cast<Diligent::Uint32>(width * 4);
    Diligent::TextureData init{};
    init.pSubResources = &sub;
    init.NumSubresources = 1;

    Diligent::RefCntAutoPtr<Diligent::ITexture> tex;
    ctx.device->CreateTexture(desc, &init, &tex);
    if (!tex) {
        spdlog::error("ImGui(Diligent): failed to create font texture");
        return;
    }

    fontSrv_ = tex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!fontSrv_) {
        spdlog::error("ImGui(Diligent): failed to create font SRV");
        return;
    }

    fontToken_ = graphics_backend::diligent_ui::RegisterExternalTexture(fontSrv_);
    atlas->SetTexID(ui::ToImGuiTextureId(fontToken_));
}

void DiligentRenderer::renderImGuiToTarget(ImDrawData* drawData) {
    if (!drawData) {
        return;
    }
    auto ctx = graphics_backend::diligent_ui::GetContext();
    if (!ctx.device || !ctx.context || !ctx.swapChain) {
        return;
    }
    if (!uiTargetRtv_) {
        return;
    }

    ensurePipeline();
    if (!ready_) {
        return;
    }

    if (drawData->TotalVtxCount == 0) {
        return;
    }

    const size_t vtxBufferBytes = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImGuiVertex);
    const size_t idxSize = sizeof(ImDrawIdx);
    const size_t idxBufferBytes = static_cast<size_t>(drawData->TotalIdxCount) * idxSize;
    ensureBuffers(vtxBufferBytes, idxBufferBytes);
    if (!vertexBuffer_ || !indexBuffer_) {
        return;
    }

    {
        Diligent::MapHelper<ImGuiVertex> vtxData(ctx.context, vertexBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        Diligent::MapHelper<ImDrawIdx> idxData(ctx.context, indexBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        ImGuiVertex* vtxDst = vtxData;
        ImDrawIdx* idxDst = idxData;
        for (int n = 0; n < drawData->CmdListsCount; n++) {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            for (int i = 0; i < cmdList->VtxBuffer.Size; ++i) {
                const ImDrawVert& v = cmdList->VtxBuffer[i];
                vtxDst->x = v.pos.x;
                vtxDst->y = v.pos.y;
                vtxDst->u = v.uv.x;
                vtxDst->v = v.uv.y;
                vtxDst->color = v.col;
                ++vtxDst;
            }
            std::memcpy(idxDst, cmdList->IdxBuffer.Data, static_cast<size_t>(cmdList->IdxBuffer.Size) * idxSize);
            idxDst += cmdList->IdxBuffer.Size;
        }
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float fbWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    const float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
    if (fbWidth <= 0.0f || fbHeight <= 0.0f) {
        return;
    }

    ImGuiConstants constants{};
    const float sx = 2.0f / drawData->DisplaySize.x;
    const float sy = -2.0f / drawData->DisplaySize.y;
    const float tx = -1.0f - drawData->DisplayPos.x * sx;
    const float ty = 1.0f + drawData->DisplayPos.y * sy;
    constants.scaleBias[0] = sx;
    constants.scaleBias[1] = sy;
    constants.scaleBias[2] = tx;
    constants.scaleBias[3] = ty;
    {
        Diligent::MapHelper<ImGuiConstants> cb(ctx.context, constantBuffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        *cb = constants;
    }

    Diligent::ITextureView* rtv = uiTargetRtv_;
    ctx.context->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx.context->ClearRenderTarget(rtv, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::Viewport vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = fbWidth;
    vp.Height = fbHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx.context->SetViewports(1, &vp, 0, 0);

    const Diligent::Uint64 vbOffset = 0;
    Diligent::IBuffer* vbs[] = {vertexBuffer_};
    ctx.context->SetVertexBuffers(0, 1, vbs, &vbOffset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx.context->SetIndexBuffer(indexBuffer_, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    ctx.context->SetPipelineState(pipeline_);

    int globalVtxOffset = 0;
    int globalIdxOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
                continue;
            }

            const ImVec4 clip = pcmd->ClipRect;
            const int clipX = static_cast<int>(clip.x);
            const int clipY = static_cast<int>(clip.y);
            const int clipW = static_cast<int>(clip.z - clip.x);
            const int clipH = static_cast<int>(clip.w - clip.y);
            if (clipW <= 0 || clipH <= 0) {
                continue;
            }
            Diligent::Rect scissor{clipX, clipY, clipX + clipW, clipY + clipH};
            ctx.context->SetScissorRects(1, &scissor, 0, 0);

            uint64_t token = ui::FromImGuiTextureId(pcmd->TextureId);
            if (token == 0) {
                token = fontToken_;
            }
            auto* srv = graphics_backend::diligent_ui::ResolveExternalTexture(token);
            if (!srv) {
                continue;
            }
            if (auto* var = shaderBinding_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")) {
                var->Set(srv);
            }
            ctx.context->CommitShaderResources(shaderBinding_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawIndexedAttribs drawAttrs{};
            drawAttrs.IndexType = (sizeof(ImDrawIdx) == 2) ? Diligent::VT_UINT16 : Diligent::VT_UINT32;
            drawAttrs.NumIndices = pcmd->ElemCount;
            drawAttrs.FirstIndexLocation = static_cast<Diligent::Uint32>(globalIdxOffset + pcmd->IdxOffset);
            drawAttrs.BaseVertex = globalVtxOffset + pcmd->VtxOffset;
            drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            ctx.context->DrawIndexed(drawAttrs);
        }
        globalIdxOffset += cmdList->IdxBuffer.Size;
        globalVtxOffset += cmdList->VtxBuffer.Size;
    }
}

} // namespace graphics_backend

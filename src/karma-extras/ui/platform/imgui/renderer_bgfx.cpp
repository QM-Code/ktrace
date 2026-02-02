#include "karma_extras/ui/platform/imgui/renderer_bgfx.hpp"

#include <imgui.h>

#include "common/data_path_resolver.hpp"
#include "common/file_utils.hpp"
#include "karma/graphics/backends/bgfx/texture_utils.hpp"
#include "spdlog/spdlog.h"
#include "karma_extras/ui/imgui/texture_utils.hpp"

#include <algorithm>
#include <cstring>

namespace graphics_backend {
namespace {
constexpr bgfx::ViewId kImGuiView = 245;

}

BgfxRenderer::BgfxRenderer() {
    if (!bgfx::getCaps()) {
        return;
    }

    sampler_ = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    scaleBias_ = bgfx::createUniform("u_scaleBias", bgfx::UniformType::Vec4);

    const std::filesystem::path shaderDir = karma::data::Resolve("bgfx/shaders/bin/vk/imgui");

    const auto vsPath = shaderDir / "vs_imgui.bin";
    const auto fsPath = shaderDir / "fs_imgui.bin";

    auto vsBytes = karma::file::ReadFileBytes(vsPath);
    auto fsBytes = karma::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("UiSystem: missing ImGui bgfx shaders '{}', '{}'", vsPath.string(), fsPath.string());
        destroyResources();
        return;
    }

    const bgfx::Memory* vsMem = bgfx::copy(vsBytes.data(), static_cast<uint32_t>(vsBytes.size()));
    const bgfx::Memory* fsMem = bgfx::copy(fsBytes.data(), static_cast<uint32_t>(fsBytes.size()));
    bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
    program_ = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(program_)) {
        spdlog::error("UiSystem: failed to create ImGui bgfx shader program");
        destroyResources();
        return;
    }

    layout_.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    ready_ = true;
}

BgfxRenderer::~BgfxRenderer() {
    destroyResources();
}

void* BgfxRenderer::toImGuiTextureId(const graphics::TextureHandle& texture) const {
    if (!texture.valid()) {
        return nullptr;
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(texture.id));
}

bool BgfxRenderer::isImGuiReady() const {
    return ready_ && fontsReady_;
}

void BgfxRenderer::ensureImGuiRenderTarget(int width, int height) {
    if (!ready_) {
        return;
    }
    if (width <= 0 || height <= 0) {
        if (bgfx::isValid(uiTargetFrameBuffer_)) {
            bgfx::destroy(uiTargetFrameBuffer_);
            uiTargetFrameBuffer_ = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(uiTargetTexture_)) {
            bgfx::destroy(uiTargetTexture_);
            uiTargetTexture_ = BGFX_INVALID_HANDLE;
        }
        uiWidth_ = 0;
        uiHeight_ = 0;
        return;
    }
    if (width == uiWidth_ && height == uiHeight_
        && bgfx::isValid(uiTargetFrameBuffer_) && bgfx::isValid(uiTargetTexture_)) {
        return;
    }
    if (bgfx::isValid(uiTargetFrameBuffer_)) {
        bgfx::destroy(uiTargetFrameBuffer_);
        uiTargetFrameBuffer_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uiTargetTexture_)) {
        bgfx::destroy(uiTargetTexture_);
        uiTargetTexture_ = BGFX_INVALID_HANDLE;
    }

    const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    uiTargetTexture_ = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        colorFlags);
    if (bgfx::isValid(uiTargetTexture_)) {
        bgfx::Attachment attachment;
        attachment.init(uiTargetTexture_);
        uiTargetFrameBuffer_ = bgfx::createFrameBuffer(1, &attachment, false);
    }

    uiWidth_ = width;
    uiHeight_ = height;
}

graphics::TextureHandle BgfxRenderer::getImGuiRenderTarget() const {
    graphics::TextureHandle handle{};
    if (!bgfx::isValid(uiTargetTexture_)) {
        return handle;
    }
    handle.id = static_cast<uint64_t>(uiTargetTexture_.idx + 1);
    handle.width = static_cast<uint32_t>(uiWidth_);
    handle.height = static_cast<uint32_t>(uiHeight_);
    handle.format = graphics::TextureFormat::RGBA8_UNORM;
    return handle;
}

void BgfxRenderer::rebuildImGuiFonts(ImFontAtlas* atlas) {
    if (!ready_ || !atlas) {
        return;
    }
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        spdlog::error("UiSystem: ImGui font texture build failed");
        return;
    }

    if (bgfx::isValid(fontTexture_)) {
        bgfx::destroy(fontTexture_);
        fontTexture_ = BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* mem = bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4));
    fontTexture_ = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        0,
        mem);

    if (!bgfx::isValid(fontTexture_)) {
        spdlog::error("UiSystem: failed to create ImGui font texture");
        return;
    }

    graphics::TextureHandle handle{};
    handle.id = static_cast<uint64_t>(fontTexture_.idx + 1);
    handle.width = static_cast<uint32_t>(width);
    handle.height = static_cast<uint32_t>(height);
    handle.format = graphics::TextureFormat::RGBA8_UNORM;
    atlas->SetTexID(ui::ToImGuiTextureId(handle));
    fontsReady_ = true;
}

void BgfxRenderer::renderImGuiToTarget(ImDrawData* drawData) {
    if (!drawData || !ready_ || !bgfx::isValid(program_) || !bgfx::isValid(fontTexture_)) {
        return;
    }
    if (!bgfx::isValid(uiTargetFrameBuffer_)) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    const int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    const float scaleBias[4] = { 2.0f / io.DisplaySize.x, -2.0f / io.DisplaySize.y, -1.0f, 1.0f };
    bgfx::setViewTransform(kImGuiView, nullptr, nullptr);
    bgfx::setViewFrameBuffer(kImGuiView, uiTargetFrameBuffer_);
    bgfx::setViewRect(kImGuiView, 0, 0, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));
    bgfx::setViewClear(kImGuiView, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    bgfx::touch(kImGuiView);
    bgfx::setUniform(scaleBias_, scaleBias);

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        const uint32_t availVb = bgfx::getAvailTransientVertexBuffer(cmdList->VtxBuffer.Size, layout_);
        const uint32_t availIb = bgfx::getAvailTransientIndexBuffer(cmdList->IdxBuffer.Size, sizeof(ImDrawIdx) == 4);
        if (availVb < static_cast<uint32_t>(cmdList->VtxBuffer.Size)
            || availIb < static_cast<uint32_t>(cmdList->IdxBuffer.Size)) {
            continue;
        }
        bgfx::allocTransientVertexBuffer(&tvb, cmdList->VtxBuffer.Size, layout_);
        bgfx::allocTransientIndexBuffer(&tib, cmdList->IdxBuffer.Size, sizeof(ImDrawIdx) == 4);

        auto* verts = reinterpret_cast<ImGuiVertex*>(tvb.data);
        for (int i = 0; i < cmdList->VtxBuffer.Size; ++i) {
            verts[i].x = vtxBuffer[i].pos.x;
            verts[i].y = vtxBuffer[i].pos.y;
            verts[i].u = vtxBuffer[i].uv.x;
            verts[i].v = vtxBuffer[i].uv.y;
            verts[i].abgr = vtxBuffer[i].col;
        }
        std::memcpy(tib.data, idxBuffer, static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));

        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
            } else {
                const ImVec4 clip = pcmd->ClipRect;
                const uint16_t cx = static_cast<uint16_t>(std::max(0.0f, clip.x));
                const uint16_t cy = static_cast<uint16_t>(std::max(0.0f, clip.y));
                const uint16_t cw = static_cast<uint16_t>(std::min(clip.z, io.DisplaySize.x) - clip.x);
                const uint16_t ch = static_cast<uint16_t>(std::min(clip.w, io.DisplaySize.y) - clip.y);
                if (cw == 0 || ch == 0) {
                    idxOffset += pcmd->ElemCount;
                    continue;
                }

                bgfx::setScissor(cx, cy, cw, ch);
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));

                bgfx::TextureHandle textureHandle = fontTexture_;
                if (pcmd->TextureId) {
                    const uint16_t idx = bgfx_utils::ToBgfxTextureHandle(ui::FromImGuiTextureId(pcmd->TextureId));
                    textureHandle.idx = idx;
                }
                bgfx::setTexture(0, sampler_, textureHandle);

                bgfx::setVertexBuffer(0, &tvb, vtxOffset, cmdList->VtxBuffer.Size);
                bgfx::setIndexBuffer(&tib, idxOffset, pcmd->ElemCount);
                bgfx::submit(kImGuiView, program_);
            }
            idxOffset += pcmd->ElemCount;
        }
    }
}

void BgfxRenderer::destroyResources() {
    if (!bgfx::getCaps()) {
        ready_ = false;
        fontsReady_ = false;
        return;
    }
    if (bgfx::isValid(uiTargetFrameBuffer_)) {
        bgfx::destroy(uiTargetFrameBuffer_);
        uiTargetFrameBuffer_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uiTargetTexture_)) {
        bgfx::destroy(uiTargetTexture_);
        uiTargetTexture_ = BGFX_INVALID_HANDLE;
    }
    uiWidth_ = 0;
    uiHeight_ = 0;
    if (bgfx::isValid(fontTexture_)) {
        bgfx::destroy(fontTexture_);
        fontTexture_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
        program_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(sampler_)) {
        bgfx::destroy(sampler_);
        sampler_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(scaleBias_)) {
        bgfx::destroy(scaleBias_);
        scaleBias_ = BGFX_INVALID_HANDLE;
    }
    ready_ = false;
    fontsReady_ = false;
}

} // namespace graphics_backend

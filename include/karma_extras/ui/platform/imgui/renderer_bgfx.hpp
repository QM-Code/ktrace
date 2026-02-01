#pragma once

#include "karma/graphics/ui_render_target_bridge.hpp"

#include <bgfx/bgfx.h>
#include <cstdint>


namespace graphics_backend {

class BgfxRenderer final : public UiRenderTargetBridge {
public:
    BgfxRenderer();
    ~BgfxRenderer() override;

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override;
    void rebuildImGuiFonts(ImFontAtlas* atlas) override;
    void renderImGuiToTarget(ImDrawData* drawData) override;
    bool isImGuiReady() const override;
    void ensureImGuiRenderTarget(int width, int height) override;
    graphics::TextureHandle getImGuiRenderTarget() const override;

private:
    struct ImGuiVertex {
        float x;
        float y;
        float u;
        float v;
        uint32_t abgr;
    };

    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scaleBias_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle fontTexture_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout_{};
    bgfx::TextureHandle uiTargetTexture_ = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle uiTargetFrameBuffer_ = BGFX_INVALID_HANDLE;
    int uiWidth_ = 0;
    int uiHeight_ = 0;
    bool ready_ = false;
    bool fontsReady_ = false;

    void destroyResources();
};

} // namespace graphics_backend

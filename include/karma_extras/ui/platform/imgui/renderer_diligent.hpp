#pragma once

#include "karma/graphics/ui_render_target_bridge.hpp"

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#include <cstdint>
#include <memory>

namespace Diligent {
class IBuffer;
class IShaderResourceBinding;
class IPipelineState;
class ITexture;
class ITextureView;
}

namespace graphics_backend {

class DiligentRenderer final : public UiRenderTargetBridge {
public:
    DiligentRenderer();
    ~DiligentRenderer() override;

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override;
    void rebuildImGuiFonts(ImFontAtlas* atlas) override;
    void renderImGuiToTarget(ImDrawData* drawData) override;
    bool isImGuiReady() const override;
    void ensureImGuiRenderTarget(int width, int height) override;
    graphics::TextureHandle getImGuiRenderTarget() const override;

private:
    void ensurePipeline();
    void ensureBuffers(std::size_t vertexBytes, std::size_t indexBytes);

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constantBuffer_;
    std::size_t vertexBufferSize_ = 0;
    std::size_t indexBufferSize_ = 0;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> fontSrv_;
    uint64_t fontToken_ = 0;
    Diligent::RefCntAutoPtr<Diligent::ITexture> uiTargetTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> uiTargetRtv_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> uiTargetSrv_;
    uint64_t uiToken_ = 0;
    int uiWidth_ = 0;
    int uiHeight_ = 0;
    bool ready_ = false;
};

} // namespace graphics_backend

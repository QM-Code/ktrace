/*
 * RmlUi Diligent renderer stub for KARMA.
 * TODO: Implement actual Diligent rendering.
 */
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class RenderInterface_Diligent : public Rml::RenderInterface {
public:
    RenderInterface_Diligent() = default;
    ~RenderInterface_Diligent() override = default;

    // Returns true if the renderer was successfully constructed.
    explicit operator bool() const { return ready; }

    void SetViewport(int width, int height, int offset_x = 0, int offset_y = 0);
    void BeginFrame();
    void EndFrame();

    void Clear() {}
    void SetPresentToBackbuffer(bool) {}
    unsigned int GetOutputTextureId() const { return static_cast<unsigned int>(uiToken_); }
    int GetOutputWidth() const { return uiWidth_ > 0 ? uiWidth_ : viewport_width; }
    int GetOutputHeight() const { return uiHeight_ > 0 ? uiHeight_ : viewport_height; }

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle handle,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    void EnableScissorRegion(bool enable) override { scissor_enabled = enable; }
    void SetScissorRegion(Rml::Rectanglei region) override { scissor_region = region; }

    void EnableClipMask(bool) override {}
    void RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) override {}

    void SetTransform(const Rml::Matrix4f* transform) override;

    Rml::LayerHandle PushLayer() override { return {}; }
    void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode,
                         Rml::Span<const Rml::CompiledFilterHandle>) override {}
    void PopLayer() override {}

    Rml::TextureHandle SaveLayerAsTexture() override { return {}; }
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override { return {}; }
    Rml::CompiledFilterHandle CompileFilter(const Rml::String&, const Rml::Dictionary&) override { return {}; }
    void ReleaseFilter(Rml::CompiledFilterHandle) override {}

    Rml::CompiledShaderHandle CompileShader(const Rml::String&, const Rml::Dictionary&) override { return {}; }
    void RenderShader(Rml::CompiledShaderHandle, Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseShader(Rml::CompiledShaderHandle) override {}

    static constexpr Rml::TextureHandle TextureEnableWithoutBinding = Rml::TextureHandle(-1);
    static constexpr Rml::TextureHandle TexturePostprocess = Rml::TextureHandle(-2);

private:
    struct GeometryData {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
        uint32_t indexCount = 0;
    };

    struct TextureData {
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::ITextureView* srv = nullptr;
        int width = 0;
        int height = 0;
        bool external = false;
    };

    bool ready = true;
    bool warned = false;
    int viewport_width = 0;
    int viewport_height = 0;
    int viewport_offset_x = 0;
    int viewport_offset_y = 0;
    bool scissor_enabled = false;
    Rml::Rectanglei scissor_region{};
    Rml::Matrix4f transform;
    Rml::Matrix4f projection;

    Rml::TextureHandle next_texture_id = 1;
    std::unordered_map<Rml::TextureHandle, TextureData> textures;
    uint32_t debug_draw_calls = 0;
    uint32_t debug_triangles = 0;
    uint32_t debug_frame = 0;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constantBuffer_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> whiteTexture_;
    Diligent::ITextureView* whiteTextureView_ = nullptr;
    Rml::TextureHandle last_texture = 0;
    uint64_t uiToken_ = 0;
    Diligent::RefCntAutoPtr<Diligent::ITexture> uiTargetTexture_;
    Diligent::ITextureView* uiTargetRtv_ = nullptr;
    Diligent::ITextureView* uiTargetSrv_ = nullptr;
    int uiWidth_ = 0;
    int uiHeight_ = 0;

    void ensurePipeline();
    void ensureWhiteTexture();
    void ensureRenderTarget(int width, int height);
    const TextureData* lookupTexture(Rml::TextureHandle handle) const;
};

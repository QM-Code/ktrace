/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef RMLUI_BACKENDS_RENDERER_BGFX_H
#define RMLUI_BACKENDS_RENDERER_BGFX_H

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

#include <bgfx/bgfx.h>

#include <cstdint>
#include <unordered_map>

class RenderInterface_BGFX : public Rml::RenderInterface {
public:
    RenderInterface_BGFX();
    ~RenderInterface_BGFX();

    // Returns true if the renderer was successfully constructed.
    explicit operator bool() const { return ready; }

    void SetViewport(int viewport_width, int viewport_height, int viewport_offset_x = 0, int viewport_offset_y = 0);
    void BeginFrame();
    void EndFrame();

    void Clear() {}
    void SetPresentToBackbuffer(bool) {}
    unsigned int GetOutputTextureId() const { return outputTextureId; }
    int GetOutputWidth() const { return viewport_width; }
    int GetOutputHeight() const { return viewport_height; }

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    void EnableClipMask(bool enable) override { (void)enable; }
    void RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) override {}

    void SetTransform(const Rml::Matrix4f* transform) override;

    Rml::LayerHandle PushLayer() override { return {}; }
    void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode, Rml::Span<const Rml::CompiledFilterHandle>) override {}
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
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
        uint32_t index_count = 0;
    };

    struct TextureData {
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        int width = 0;
        int height = 0;
        bool external = false;
    };

    void destroyPrograms();
    void applyTransformUniform();
    const TextureData* lookupTexture(Rml::TextureHandle handle) const;
    void ensureRenderTarget(int width, int height);

    bool ready = false;
    bgfx::ProgramHandle program_texture = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle program_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uniform_transform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uniform_translate = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uniform_sampler = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;

    int viewport_width = 0;
    int viewport_height = 0;
    int viewport_offset_x = 0;
    int viewport_offset_y = 0;

    Rml::Matrix4f projection;
    Rml::Matrix4f transform;
    bool transform_dirty = true;

    bool scissor_enabled = false;
    Rml::Rectanglei scissor_region = Rml::Rectanglei::MakeInvalid();

    Rml::TextureHandle last_texture = {};
    std::unordered_map<Rml::TextureHandle, TextureData> textures;
    uintptr_t next_texture_id = 1;
    bgfx::TextureHandle uiTargetTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle uiTargetFrameBuffer = BGFX_INVALID_HANDLE;
    int uiWidth = 0;
    int uiHeight = 0;
    unsigned int outputTextureId = 0;

};

#endif

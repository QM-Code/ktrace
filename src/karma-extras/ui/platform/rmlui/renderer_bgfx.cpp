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

#include "karma_extras/ui/platform/rmlui/renderer_bgfx.hpp"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi/Core/Platform.h>

#include <stb_image.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "common/data_path_resolver.hpp"
#include "common/file_utils.hpp"
#include "spdlog/spdlog.h"

namespace {

constexpr bgfx::ViewId kRmlUiView = 254;

struct RmlUiVertex {
    float x;
    float y;
    uint32_t abgr;
    float u;
    float v;
};

static uint32_t toAbgr(const Rml::ColourbPremultiplied& color) {
    return (static_cast<uint32_t>(color.alpha) << 24)
        | (static_cast<uint32_t>(color.blue) << 16)
        | (static_cast<uint32_t>(color.green) << 8)
        | (static_cast<uint32_t>(color.red));
}

} // namespace

RenderInterface_BGFX::RenderInterface_BGFX() {
    if (!bgfx::getCaps()) {
        return;
    }

    uniform_transform = bgfx::createUniform("u_transform", bgfx::UniformType::Mat4);
    uniform_translate = bgfx::createUniform("u_translate", bgfx::UniformType::Vec4);
    uniform_sampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    const std::filesystem::path shader_dir = karma::data::Resolve("bgfx/shaders/bin/vk/rmlui");
    const auto vs_path = shader_dir / "vs_rmlui.bin";
    const auto fs_tex_path = shader_dir / "fs_rmlui_texture.bin";
    const auto fs_color_path = shader_dir / "fs_rmlui_color.bin";

    const auto vs_bytes = karma::file::ReadFileBytes(vs_path);
    const auto fs_tex_bytes = karma::file::ReadFileBytes(fs_tex_path);
    const auto fs_color_bytes = karma::file::ReadFileBytes(fs_color_path);
    if (vs_bytes.empty() || fs_tex_bytes.empty() || fs_color_bytes.empty()) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "RmlUi(BGFX): missing shader binaries.");
        destroyPrograms();
        return;
    }

    bgfx::ShaderHandle vsh = bgfx::createShader(bgfx::copy(vs_bytes.data(), static_cast<uint32_t>(vs_bytes.size())));
    bgfx::ShaderHandle fsh_tex = bgfx::createShader(bgfx::copy(fs_tex_bytes.data(), static_cast<uint32_t>(fs_tex_bytes.size())));
    bgfx::ShaderHandle fsh_color = bgfx::createShader(bgfx::copy(fs_color_bytes.data(), static_cast<uint32_t>(fs_color_bytes.size())));

    program_texture = bgfx::createProgram(vsh, fsh_tex, true);
    program_color = bgfx::createProgram(bgfx::createShader(bgfx::copy(vs_bytes.data(), static_cast<uint32_t>(vs_bytes.size()))),
                                        fsh_color,
                                        true);

    if (!bgfx::isValid(program_texture) || !bgfx::isValid(program_color)) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "RmlUi(BGFX): failed to create shader programs.");
        destroyPrograms();
        return;
    }

    layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    ready = true;
}

RenderInterface_BGFX::~RenderInterface_BGFX() {
    if (!bgfx::getCaps()) {
        return;
    }

    for (auto& entry : textures) {
        if (!entry.second.external && bgfx::isValid(entry.second.handle)) {
            bgfx::destroy(entry.second.handle);
        }
    }
    textures.clear();

    if (bgfx::isValid(uiTargetFrameBuffer)) {
        bgfx::destroy(uiTargetFrameBuffer);
        uiTargetFrameBuffer = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uiTargetTexture)) {
        bgfx::destroy(uiTargetTexture);
        uiTargetTexture = BGFX_INVALID_HANDLE;
    }
    uiWidth = 0;
    uiHeight = 0;
    outputTextureId = 0;

    destroyPrograms();
}

void RenderInterface_BGFX::destroyPrograms() {
    if (!bgfx::getCaps()) {
        return;
    }
    if (bgfx::isValid(program_texture)) {
        bgfx::destroy(program_texture);
        program_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(program_color)) {
        bgfx::destroy(program_color);
        program_color = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uniform_transform)) {
        bgfx::destroy(uniform_transform);
        uniform_transform = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uniform_translate)) {
        bgfx::destroy(uniform_translate);
        uniform_translate = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uniform_sampler)) {
        bgfx::destroy(uniform_sampler);
        uniform_sampler = BGFX_INVALID_HANDLE;
    }
}

void RenderInterface_BGFX::SetViewport(int width, int height, int offset_x, int offset_y) {
    viewport_width = Rml::Math::Max(width, 1);
    viewport_height = Rml::Math::Max(height, 1);
    viewport_offset_x = offset_x;
    viewport_offset_y = offset_y;
    projection = Rml::Matrix4f::ProjectOrtho(0, static_cast<float>(viewport_width),
                                             static_cast<float>(viewport_height), 0,
                                             -10000, 10000);
    transform = projection;
    transform_dirty = true;
    ensureRenderTarget(viewport_width, viewport_height);
}

void RenderInterface_BGFX::BeginFrame() {
    if (!ready || viewport_width <= 0 || viewport_height <= 0) {
        return;
    }
    ensureRenderTarget(viewport_width, viewport_height);

    bgfx::setViewMode(kRmlUiView, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(kRmlUiView, nullptr, nullptr);
    if (bgfx::isValid(uiTargetFrameBuffer)) {
        bgfx::setViewFrameBuffer(kRmlUiView, uiTargetFrameBuffer);
    }
    bgfx::setViewRect(kRmlUiView,
                      static_cast<uint16_t>(viewport_offset_x),
                      static_cast<uint16_t>(viewport_offset_y),
                      static_cast<uint16_t>(viewport_width),
                      static_cast<uint16_t>(viewport_height));
    bgfx::setViewClear(kRmlUiView, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    bgfx::touch(kRmlUiView);
}

void RenderInterface_BGFX::EndFrame() {
}

void RenderInterface_BGFX::ensureRenderTarget(int width, int height) {
    if (!bgfx::getCaps()) {
        return;
    }
    if (width <= 0 || height <= 0) {
        if (bgfx::isValid(uiTargetFrameBuffer)) {
            bgfx::destroy(uiTargetFrameBuffer);
            uiTargetFrameBuffer = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(uiTargetTexture)) {
            bgfx::destroy(uiTargetTexture);
            uiTargetTexture = BGFX_INVALID_HANDLE;
        }
        uiWidth = 0;
        uiHeight = 0;
        outputTextureId = 0;
        return;
    }
    if (width == uiWidth && height == uiHeight && bgfx::isValid(uiTargetTexture) && bgfx::isValid(uiTargetFrameBuffer)) {
        return;
    }
    if (bgfx::isValid(uiTargetFrameBuffer)) {
        bgfx::destroy(uiTargetFrameBuffer);
        uiTargetFrameBuffer = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uiTargetTexture)) {
        bgfx::destroy(uiTargetTexture);
        uiTargetTexture = BGFX_INVALID_HANDLE;
    }

    const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    uiTargetTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        colorFlags);
    if (bgfx::isValid(uiTargetTexture)) {
        bgfx::Attachment attachment;
        attachment.init(uiTargetTexture);
        uiTargetFrameBuffer = bgfx::createFrameBuffer(1, &attachment, false);
    }
    uiWidth = width;
    uiHeight = height;
    outputTextureId = bgfx::isValid(uiTargetTexture) ? static_cast<unsigned int>(uiTargetTexture.idx + 1) : 0;
}

Rml::CompiledGeometryHandle RenderInterface_BGFX::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                   Rml::Span<const int> indices) {
    if (!ready || vertices.empty() || indices.empty()) {
        return {};
    }

    std::vector<RmlUiVertex> packed_vertices;
    packed_vertices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        packed_vertices[i].x = vertices[i].position.x;
        packed_vertices[i].y = vertices[i].position.y;
        packed_vertices[i].u = vertices[i].tex_coord.x;
        packed_vertices[i].v = vertices[i].tex_coord.y;
        packed_vertices[i].abgr = toAbgr(vertices[i].colour);
    }

    std::vector<uint32_t> packed_indices;
    packed_indices.resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        packed_indices[i] = static_cast<uint32_t>(indices[i]);
    }

    GeometryData* geometry = new GeometryData();
    geometry->index_count = static_cast<uint32_t>(packed_indices.size());

    const bgfx::Memory* vmem = bgfx::copy(packed_vertices.data(),
                                          static_cast<uint32_t>(packed_vertices.size() * sizeof(RmlUiVertex)));
    geometry->vbh = bgfx::createVertexBuffer(vmem, layout);

    const bgfx::Memory* imem = bgfx::copy(packed_indices.data(),
                                          static_cast<uint32_t>(packed_indices.size() * sizeof(uint32_t)));
    geometry->ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);

    if (!bgfx::isValid(geometry->vbh) || !bgfx::isValid(geometry->ibh)) {
        if (bgfx::isValid(geometry->vbh)) {
            bgfx::destroy(geometry->vbh);
        }
        if (bgfx::isValid(geometry->ibh)) {
            bgfx::destroy(geometry->ibh);
        }
        delete geometry;
        return {};
    }

    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void RenderInterface_BGFX::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                          Rml::Vector2f translation,
                                          Rml::TextureHandle texture) {
    if (!ready || !handle) {
        return;
    }

    if (texture == TexturePostprocess) {
        return;
    }

    GeometryData* geometry = reinterpret_cast<GeometryData*>(handle);
    if (!bgfx::isValid(geometry->vbh) || !bgfx::isValid(geometry->ibh)) {
        return;
    }

    if (bgfx::isValid(uniform_transform)) {
        applyTransformUniform();
    }
    if (bgfx::isValid(uniform_translate)) {
        const float translate[4] = {translation.x, translation.y, 0.0f, 0.0f};
        bgfx::setUniform(uniform_translate, translate);
    }

    if (scissor_enabled && scissor_region.Valid()) {
        const int x = std::max(0, scissor_region.p0.x);
        const int y = std::max(0, scissor_region.p0.y);
        const int w = std::max(0, scissor_region.Width());
        const int h = std::max(0, scissor_region.Height());
        if (w > 0 && h > 0) {
            bgfx::setScissor(static_cast<uint16_t>(x),
                             static_cast<uint16_t>(y),
                             static_cast<uint16_t>(w),
                             static_cast<uint16_t>(h));
        }
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                     BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA) |
                     BGFX_STATE_MSAA;
    bgfx::setState(state);

    bool use_texture_program = (texture != 0);
    const TextureData* tex_data = nullptr;
    if (use_texture_program && texture != TextureEnableWithoutBinding) {
        tex_data = lookupTexture(texture);
        if (!tex_data || !bgfx::isValid(tex_data->handle)) {
            use_texture_program = false;
        } else {
            last_texture = texture;
        }
    } else if (use_texture_program && texture == TextureEnableWithoutBinding) {
        tex_data = lookupTexture(last_texture);
        if (!tex_data || !bgfx::isValid(tex_data->handle)) {
            use_texture_program = false;
        }
    }

    if (use_texture_program) {
        if (bgfx::isValid(uniform_sampler) && tex_data) {
            bgfx::setTexture(0, uniform_sampler, tex_data->handle);
        }
        bgfx::setVertexBuffer(0, geometry->vbh);
        bgfx::setIndexBuffer(geometry->ibh, 0, geometry->index_count);
        bgfx::submit(kRmlUiView, program_texture);
    } else {
        bgfx::setVertexBuffer(0, geometry->vbh);
        bgfx::setIndexBuffer(geometry->ibh, 0, geometry->index_count);
        bgfx::submit(kRmlUiView, program_color);
    }

}

void RenderInterface_BGFX::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    if (!handle) {
        return;
    }
    GeometryData* geometry = reinterpret_cast<GeometryData*>(handle);
    if (bgfx::isValid(geometry->vbh)) {
        bgfx::destroy(geometry->vbh);
    }
    if (bgfx::isValid(geometry->ibh)) {
        bgfx::destroy(geometry->ibh);
    }
    delete geometry;
}

Rml::TextureHandle RenderInterface_BGFX::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    const Rml::String tex_prefix = "texid:";
    if (source.rfind(tex_prefix, 0) == 0) {
        const char* id_str = source.c_str() + tex_prefix.size();
        char* end_ptr = nullptr;
        const unsigned long tex_token = std::strtoul(id_str, &end_ptr, 10);
        if (tex_token == 0) {
            return {};
        }
        const unsigned long tex_id = tex_token - 1;
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
        entry.handle.idx = static_cast<uint16_t>(tex_id);
        entry.width = width;
        entry.height = height;
        entry.external = true;
        const Rml::TextureHandle handle = static_cast<Rml::TextureHandle>(next_texture_id++);
        textures.emplace(handle, entry);
        texture_dimensions.x = entry.width;
        texture_dimensions.y = entry.height;
        spdlog::trace("RmlUi(BGFX): external texture texid:{} -> handle={} size={}x{}",
                      tex_token, handle, entry.width, entry.height);
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

    const bgfx::Memory* mem = bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4));
    stbi_image_free(pixels);

    bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);

    if (!bgfx::isValid(tex)) {
        return {};
    }

    TextureData entry;
    entry.handle = tex;
    entry.width = width;
    entry.height = height;
    entry.external = false;

    const Rml::TextureHandle handle = static_cast<Rml::TextureHandle>(next_texture_id++);
    textures.emplace(handle, entry);
    texture_dimensions.x = width;
    texture_dimensions.y = height;
    spdlog::trace("RmlUi(BGFX): LoadTexture handle={} size={}x{}", handle, width, height);
    return handle;
}

Rml::TextureHandle RenderInterface_BGFX::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                                         Rml::Vector2i source_dimensions) {
    if (source_data.empty() || source_dimensions.x <= 0 || source_dimensions.y <= 0) {
        return {};
    }

    spdlog::trace("RmlUi(BGFX): GenerateTexture {}x{}", source_dimensions.x, source_dimensions.y);

    const bgfx::Memory* mem = bgfx::copy(source_data.data(),
                                         static_cast<uint32_t>(source_dimensions.x * source_dimensions.y * 4));
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(source_dimensions.x),
        static_cast<uint16_t>(source_dimensions.y),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);

    if (!bgfx::isValid(tex)) {
        return {};
    }

    TextureData entry;
    entry.handle = tex;
    entry.width = source_dimensions.x;
    entry.height = source_dimensions.y;
    entry.external = false;

    const Rml::TextureHandle handle = static_cast<Rml::TextureHandle>(next_texture_id++);
    textures.emplace(handle, entry);
    spdlog::trace("RmlUi(BGFX): GenerateTexture handle={} size={}x{}", handle, source_dimensions.x, source_dimensions.y);
    return handle;
}

void RenderInterface_BGFX::ReleaseTexture(Rml::TextureHandle texture_handle) {
    auto it = textures.find(texture_handle);
    if (it == textures.end()) {
        return;
    }
    if (!it->second.external && bgfx::isValid(it->second.handle)) {
        bgfx::destroy(it->second.handle);
    }
    textures.erase(it);
}

void RenderInterface_BGFX::EnableScissorRegion(bool enable) {
    scissor_enabled = enable;
    if (!enable) {
        scissor_region = Rml::Rectanglei::MakeInvalid();
    }
}

void RenderInterface_BGFX::SetScissorRegion(Rml::Rectanglei region) {
    scissor_region = region;
}

void RenderInterface_BGFX::SetTransform(const Rml::Matrix4f* new_transform) {
    transform = (new_transform ? (projection * (*new_transform)) : projection);
    transform_dirty = true;
}

void RenderInterface_BGFX::applyTransformUniform() {
    if (!bgfx::isValid(uniform_transform)) {
        return;
    }
    bgfx::setUniform(uniform_transform, transform.data());
    transform_dirty = false;
}

const RenderInterface_BGFX::TextureData* RenderInterface_BGFX::lookupTexture(Rml::TextureHandle handle) const {
    auto it = textures.find(handle);
    if (it == textures.end()) {
        return nullptr;
    }
    return &it->second;
}

#include "ui/backends/rmlui/internal.hpp"

#if defined(KARMA_HAS_RMLUI)

#include "karma/common/logging/logging.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace karma::ui::rmlui {
namespace {

inline int ClampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(hi, value));
}

inline float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

} // namespace

bool CpuRenderInterface::beginFrame(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    frame_width_ = width;
    frame_height_ = height;
    scissor_enabled_ = false;
    scissor_ = Rml::Rectanglei::FromPositionSize({0, 0}, {width, height});
    draw_calls_.clear();
    draw_call_count_ = 0;

    const size_t pixel_count = static_cast<size_t>(frame_width_) * static_cast<size_t>(frame_height_) * 4u;
    if (frame_texture_.pixels.size() != pixel_count) {
        frame_texture_.pixels.assign(pixel_count, 0);
    } else {
        std::fill(frame_texture_.pixels.begin(), frame_texture_.pixels.end(), 0);
    }
    frame_texture_.width = frame_width_;
    frame_texture_.height = frame_height_;
    frame_texture_.channels = 4;
    return true;
}

void CpuRenderInterface::rasterize() {
    for (const auto& call : draw_calls_) {
        const auto geometry_it = geometries_.find(call.geometry);
        if (geometry_it == geometries_.end()) {
            continue;
        }

        const Texture* texture = nullptr;
        if (call.texture != 0) {
            const auto texture_it = textures_.find(call.texture);
            if (texture_it != textures_.end()) {
                texture = &texture_it->second;
            }
        }

        const auto& geometry = geometry_it->second;
        if (geometry.indices.size() < 3) {
            continue;
        }

        int clip_min_x = 0;
        int clip_min_y = 0;
        int clip_max_x = frame_width_;
        int clip_max_y = frame_height_;
        if (call.scissor_enabled) {
            clip_min_x = ClampInt(call.scissor.Left(), 0, frame_width_);
            clip_min_y = ClampInt(call.scissor.Top(), 0, frame_height_);
            clip_max_x = ClampInt(call.scissor.Right(), 0, frame_width_);
            clip_max_y = ClampInt(call.scissor.Bottom(), 0, frame_height_);
            if (clip_min_x >= clip_max_x || clip_min_y >= clip_max_y) {
                continue;
            }
        }

        for (size_t idx = 0; idx + 2 < geometry.indices.size(); idx += 3) {
            const int ia = geometry.indices[idx + 0];
            const int ib = geometry.indices[idx + 1];
            const int ic = geometry.indices[idx + 2];
            if (ia < 0 || ib < 0 || ic < 0 ||
                static_cast<size_t>(ia) >= geometry.vertices.size() ||
                static_cast<size_t>(ib) >= geometry.vertices.size() ||
                static_cast<size_t>(ic) >= geometry.vertices.size()) {
                continue;
            }
            rasterizeTriangle(geometry.vertices[static_cast<size_t>(ia)],
                              geometry.vertices[static_cast<size_t>(ib)],
                              geometry.vertices[static_cast<size_t>(ic)],
                              call.translation,
                              texture,
                              clip_min_x,
                              clip_min_y,
                              clip_max_x,
                              clip_max_y);
        }
    }
}

const renderer::MeshData::TextureData& CpuRenderInterface::frameTexture() const {
    return frame_texture_;
}

size_t CpuRenderInterface::drawCallCount() const {
    return draw_call_count_;
}

Rml::CompiledGeometryHandle CpuRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                Rml::Span<const int> indices) {
    if (vertices.empty() || indices.empty()) {
        return 0;
    }

    const Rml::CompiledGeometryHandle handle = next_geometry_handle_++;
    Geometry geometry{};
    geometry.vertices.assign(vertices.begin(), vertices.end());
    geometry.indices.assign(indices.begin(), indices.end());
    geometries_[handle] = std::move(geometry);
    return handle;
}

void CpuRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                        Rml::Vector2f translation,
                                        Rml::TextureHandle texture) {
    if (geometry == 0) {
        return;
    }
    DrawCall call{};
    call.geometry = geometry;
    call.translation = translation;
    call.texture = texture;
    call.scissor_enabled = scissor_enabled_;
    call.scissor = scissor_;
    draw_calls_.push_back(call);
    ++draw_call_count_;
}

void CpuRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    geometries_.erase(geometry);
}

Rml::TextureHandle CpuRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    texture_dimensions = Rml::Vector2i(0, 0);
    const std::string source_key = source;
    if (!missing_texture_sources_.contains(source_key)) {
        missing_texture_sources_.insert(source_key);
        KARMA_TRACE("ui.system.rmlui",
                    "RmlAdapter[rmlui]: external texture load not implemented '{}'",
                    source_key);
    }
    return 0;
}

Rml::TextureHandle CpuRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                       Rml::Vector2i source_dimensions) {
    if (source_dimensions.x <= 0 || source_dimensions.y <= 0) {
        return 0;
    }

    const size_t required_bytes =
        static_cast<size_t>(source_dimensions.x) * static_cast<size_t>(source_dimensions.y) * 4u;
    if (source.size() < required_bytes) {
        return 0;
    }

    Texture texture{};
    texture.width = source_dimensions.x;
    texture.height = source_dimensions.y;
    texture.pixels.assign(source.begin(), source.begin() + static_cast<ptrdiff_t>(required_bytes));

    const Rml::TextureHandle handle = next_texture_handle_++;
    textures_[handle] = std::move(texture);
    return handle;
}

void CpuRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
    textures_.erase(texture);
}

void CpuRenderInterface::EnableScissorRegion(bool enable) {
    scissor_enabled_ = enable;
}

void CpuRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
    scissor_ = region;
}

void CpuRenderInterface::sampleTexture(const Texture* texture, float u, float v, float& r, float& g, float& b, float& a) {
    if (!texture || texture->width <= 0 || texture->height <= 0 || texture->pixels.empty()) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        a = 1.0f;
        return;
    }

    const float uc = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(texture->width - 1);
    const float vc = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(texture->height - 1);
    const int x0 = ClampInt(static_cast<int>(std::floor(uc)), 0, texture->width - 1);
    const int y0 = ClampInt(static_cast<int>(std::floor(vc)), 0, texture->height - 1);
    const int x1 = ClampInt(x0 + 1, 0, texture->width - 1);
    const int y1 = ClampInt(y0 + 1, 0, texture->height - 1);
    const float tx = uc - static_cast<float>(x0);
    const float ty = vc - static_cast<float>(y0);

    auto read_channel = [&](int x, int y, int channel) -> float {
        const size_t pixel_offset =
            (static_cast<size_t>(y) * static_cast<size_t>(texture->width) + static_cast<size_t>(x)) * 4u;
        return static_cast<float>(texture->pixels[pixel_offset + static_cast<size_t>(channel)]) / 255.0f;
    };

    const float r00 = read_channel(x0, y0, 0);
    const float r10 = read_channel(x1, y0, 0);
    const float r01 = read_channel(x0, y1, 0);
    const float r11 = read_channel(x1, y1, 0);
    const float g00 = read_channel(x0, y0, 1);
    const float g10 = read_channel(x1, y0, 1);
    const float g01 = read_channel(x0, y1, 1);
    const float g11 = read_channel(x1, y1, 1);
    const float b00 = read_channel(x0, y0, 2);
    const float b10 = read_channel(x1, y0, 2);
    const float b01 = read_channel(x0, y1, 2);
    const float b11 = read_channel(x1, y1, 2);
    const float a00 = read_channel(x0, y0, 3);
    const float a10 = read_channel(x1, y0, 3);
    const float a01 = read_channel(x0, y1, 3);
    const float a11 = read_channel(x1, y1, 3);

    const float rx0 = (r00 * (1.0f - tx)) + (r10 * tx);
    const float rx1 = (r01 * (1.0f - tx)) + (r11 * tx);
    const float gx0 = (g00 * (1.0f - tx)) + (g10 * tx);
    const float gx1 = (g01 * (1.0f - tx)) + (g11 * tx);
    const float bx0 = (b00 * (1.0f - tx)) + (b10 * tx);
    const float bx1 = (b01 * (1.0f - tx)) + (b11 * tx);
    const float ax0 = (a00 * (1.0f - tx)) + (a10 * tx);
    const float ax1 = (a01 * (1.0f - tx)) + (a11 * tx);

    r = (rx0 * (1.0f - ty)) + (rx1 * ty);
    g = (gx0 * (1.0f - ty)) + (gx1 * ty);
    b = (bx0 * (1.0f - ty)) + (bx1 * ty);
    a = (ax0 * (1.0f - ty)) + (ax1 * ty);
}

void CpuRenderInterface::rasterizeTriangle(const Rml::Vertex& va,
                                           const Rml::Vertex& vb,
                                           const Rml::Vertex& vc,
                                           const Rml::Vector2f& translation,
                                           const Texture* texture,
                                           int clip_min_x,
                                           int clip_min_y,
                                           int clip_max_x,
                                           int clip_max_y) {
    const float ax = va.position.x + translation.x;
    const float ay = va.position.y + translation.y;
    const float bx = vb.position.x + translation.x;
    const float by = vb.position.y + translation.y;
    const float cx = vc.position.x + translation.x;
    const float cy = vc.position.y + translation.y;

    const float area = Edge(ax, ay, bx, by, cx, cy);
    if (std::abs(area) <= 1e-6f) {
        return;
    }

    const int tri_min_x =
        std::max(clip_min_x, std::max(0, static_cast<int>(std::floor(std::min({ax, bx, cx})))));
    const int tri_min_y =
        std::max(clip_min_y, std::max(0, static_cast<int>(std::floor(std::min({ay, by, cy})))));
    const int tri_max_x =
        std::min(clip_max_x, std::min(frame_width_, static_cast<int>(std::ceil(std::max({ax, bx, cx})))));
    const int tri_max_y =
        std::min(clip_max_y, std::min(frame_height_, static_cast<int>(std::ceil(std::max({ay, by, cy})))));
    if (tri_min_x >= tri_max_x || tri_min_y >= tri_max_y) {
        return;
    }

    const float ar = static_cast<float>(va.colour.red) / 255.0f;
    const float ag = static_cast<float>(va.colour.green) / 255.0f;
    const float ab = static_cast<float>(va.colour.blue) / 255.0f;
    const float aa = static_cast<float>(va.colour.alpha) / 255.0f;
    const float br = static_cast<float>(vb.colour.red) / 255.0f;
    const float bg = static_cast<float>(vb.colour.green) / 255.0f;
    const float bb = static_cast<float>(vb.colour.blue) / 255.0f;
    const float ba = static_cast<float>(vb.colour.alpha) / 255.0f;
    const float cr = static_cast<float>(vc.colour.red) / 255.0f;
    const float cg = static_cast<float>(vc.colour.green) / 255.0f;
    const float cb = static_cast<float>(vc.colour.blue) / 255.0f;
    const float ca = static_cast<float>(vc.colour.alpha) / 255.0f;

    for (int y = tri_min_y; y < tri_max_y; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        for (int x = tri_min_x; x < tri_max_x; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float w0_raw = Edge(bx, by, cx, cy, px, py);
            const float w1_raw = Edge(cx, cy, ax, ay, px, py);
            const float w2_raw = Edge(ax, ay, bx, by, px, py);
            const bool inside =
                ((w0_raw >= 0.0f) && (w1_raw >= 0.0f) && (w2_raw >= 0.0f)) ||
                ((w0_raw <= 0.0f) && (w1_raw <= 0.0f) && (w2_raw <= 0.0f));
            if (!inside) {
                continue;
            }

            const float w0 = w0_raw / area;
            const float w1 = w1_raw / area;
            const float w2 = w2_raw / area;

            const float u = (w0 * va.tex_coord.x) + (w1 * vb.tex_coord.x) + (w2 * vc.tex_coord.x);
            const float v = (w0 * va.tex_coord.y) + (w1 * vb.tex_coord.y) + (w2 * vc.tex_coord.y);
            const float vr = (w0 * ar) + (w1 * br) + (w2 * cr);
            const float vg = (w0 * ag) + (w1 * bg) + (w2 * cg);
            const float vb_col = (w0 * ab) + (w1 * bb) + (w2 * cb);
            const float va_col = (w0 * aa) + (w1 * ba) + (w2 * ca);

            float tr = 1.0f;
            float tg = 1.0f;
            float tb = 1.0f;
            float ta = 1.0f;
            sampleTexture(texture, u, v, tr, tg, tb, ta);

            const float src_r = std::clamp(vr * tr, 0.0f, 1.0f);
            const float src_g = std::clamp(vg * tg, 0.0f, 1.0f);
            const float src_b = std::clamp(vb_col * tb, 0.0f, 1.0f);
            const float src_a = std::clamp(va_col * ta, 0.0f, 1.0f);
            if (src_a <= 0.0f) {
                continue;
            }

            // RmlUi vertex colors and generated textures use premultiplied alpha.
            // Convert to straight-alpha RGB before blending into the software target.
            const float src_r_straight = std::clamp(src_r / src_a, 0.0f, 1.0f);
            const float src_g_straight = std::clamp(src_g / src_a, 0.0f, 1.0f);
            const float src_b_straight = std::clamp(src_b / src_a, 0.0f, 1.0f);

            const size_t dst_idx =
                (static_cast<size_t>(y) * static_cast<size_t>(frame_width_) + static_cast<size_t>(x)) * 4u;
            const float dst_r = static_cast<float>(frame_texture_.pixels[dst_idx + 0]) / 255.0f;
            const float dst_g = static_cast<float>(frame_texture_.pixels[dst_idx + 1]) / 255.0f;
            const float dst_b = static_cast<float>(frame_texture_.pixels[dst_idx + 2]) / 255.0f;
            const float dst_a = static_cast<float>(frame_texture_.pixels[dst_idx + 3]) / 255.0f;

            // Composite in straight-alpha space so backend blend state
            // (src alpha / inv src alpha) produces expected overlay opacity.
            const float out_a = src_a + (dst_a * (1.0f - src_a));
            float out_r = 0.0f;
            float out_g = 0.0f;
            float out_b = 0.0f;
            if (out_a > 1e-6f) {
                out_r = ((src_r_straight * src_a) + (dst_r * dst_a * (1.0f - src_a))) / out_a;
                out_g = ((src_g_straight * src_a) + (dst_g * dst_a * (1.0f - src_a))) / out_a;
                out_b = ((src_b_straight * src_a) + (dst_b * dst_a * (1.0f - src_a))) / out_a;
            }

            frame_texture_.pixels[dst_idx + 0] =
                static_cast<uint8_t>(std::clamp(out_r * 255.0f, 0.0f, 255.0f));
            frame_texture_.pixels[dst_idx + 1] =
                static_cast<uint8_t>(std::clamp(out_g * 255.0f, 0.0f, 255.0f));
            frame_texture_.pixels[dst_idx + 2] =
                static_cast<uint8_t>(std::clamp(out_b * 255.0f, 0.0f, 255.0f));
            frame_texture_.pixels[dst_idx + 3] =
                static_cast<uint8_t>(std::clamp(out_a * 255.0f, 0.0f, 255.0f));
        }
    }
}

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)

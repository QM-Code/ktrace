#include "ui/backends/imgui/internal.hpp"

#if defined(KARMA_HAS_IMGUI)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace karma::ui::imgui {
namespace {

inline float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

inline void UnpackVertexColor(ImU32 packed, float& r, float& g, float& b, float& a) {
    const uint8_t cr = static_cast<uint8_t>((packed >> IM_COL32_R_SHIFT) & 0xFFu);
    const uint8_t cg = static_cast<uint8_t>((packed >> IM_COL32_G_SHIFT) & 0xFFu);
    const uint8_t cb = static_cast<uint8_t>((packed >> IM_COL32_B_SHIFT) & 0xFFu);
    const uint8_t ca = static_cast<uint8_t>((packed >> IM_COL32_A_SHIFT) & 0xFFu);
    r = static_cast<float>(cr) / 255.0f;
    g = static_cast<float>(cg) / 255.0f;
    b = static_cast<float>(cb) / 255.0f;
    a = static_cast<float>(ca) / 255.0f;
}

inline void SampleFontAtlas(const std::vector<uint8_t>& atlas_pixels,
                            int atlas_w,
                            int atlas_h,
                            float u,
                            float v,
                            float& r,
                            float& g,
                            float& b,
                            float& a) {
    if (atlas_w <= 0 || atlas_h <= 0 || atlas_pixels.empty()) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        a = 1.0f;
        return;
    }

    const float uc = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(atlas_w - 1);
    const float vc = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(atlas_h - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(uc)), 0, atlas_w - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(vc)), 0, atlas_h - 1);
    const int x1 = std::clamp(x0 + 1, 0, atlas_w - 1);
    const int y1 = std::clamp(y0 + 1, 0, atlas_h - 1);
    const float tx = uc - static_cast<float>(x0);
    const float ty = vc - static_cast<float>(y0);

    auto sample = [&](int sx, int sy, int channel) -> float {
        const size_t idx =
            (static_cast<size_t>(sy) * static_cast<size_t>(atlas_w) + static_cast<size_t>(sx)) * 4u;
        return static_cast<float>(atlas_pixels[idx + static_cast<size_t>(channel)]) / 255.0f;
    };

    const float r00 = sample(x0, y0, 0);
    const float r10 = sample(x1, y0, 0);
    const float r01 = sample(x0, y1, 0);
    const float r11 = sample(x1, y1, 0);
    const float g00 = sample(x0, y0, 1);
    const float g10 = sample(x1, y0, 1);
    const float g01 = sample(x0, y1, 1);
    const float g11 = sample(x1, y1, 1);
    const float b00 = sample(x0, y0, 2);
    const float b10 = sample(x1, y0, 2);
    const float b01 = sample(x0, y1, 2);
    const float b11 = sample(x1, y1, 2);
    const float a00 = sample(x0, y0, 3);
    const float a10 = sample(x1, y0, 3);
    const float a01 = sample(x0, y1, 3);
    const float a11 = sample(x1, y1, 3);

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

void RasterizeTriangle(const ImDrawVert& va,
                       const ImDrawVert& vb,
                       const ImDrawVert& vc,
                       ImTextureID texture_id,
                       ImTextureID atlas_texture_id,
                       int clip_min_x,
                       int clip_min_y,
                       int clip_max_x,
                       int clip_max_y,
                       int target_w,
                       int target_h,
                       const std::vector<uint8_t>& atlas_pixels,
                       int atlas_w,
                       int atlas_h,
                       std::vector<uint8_t>& target) {
    const float ax = va.pos.x;
    const float ay = va.pos.y;
    const float bx = vb.pos.x;
    const float by = vb.pos.y;
    const float cx = vc.pos.x;
    const float cy = vc.pos.y;

    const float area = Edge(ax, ay, bx, by, cx, cy);
    if (std::abs(area) <= 1e-6f) {
        return;
    }

    const int tri_min_x = std::max(clip_min_x, std::max(0, static_cast<int>(std::floor(std::min({ax, bx, cx})))));
    const int tri_min_y = std::max(clip_min_y, std::max(0, static_cast<int>(std::floor(std::min({ay, by, cy})))));
    const int tri_max_x =
        std::min(clip_max_x, std::min(target_w, static_cast<int>(std::ceil(std::max({ax, bx, cx})))));
    const int tri_max_y =
        std::min(clip_max_y, std::min(target_h, static_cast<int>(std::ceil(std::max({ay, by, cy})))));
    if (tri_min_x >= tri_max_x || tri_min_y >= tri_max_y) {
        return;
    }

    float ar = 0.0f;
    float ag = 0.0f;
    float ab = 0.0f;
    float aa = 0.0f;
    float br = 0.0f;
    float bg = 0.0f;
    float bb = 0.0f;
    float ba = 0.0f;
    float cr = 0.0f;
    float cg = 0.0f;
    float cb = 0.0f;
    float ca = 0.0f;
    UnpackVertexColor(va.col, ar, ag, ab, aa);
    UnpackVertexColor(vb.col, br, bg, bb, ba);
    UnpackVertexColor(vc.col, cr, cg, cb, ca);

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

            const float u = (w0 * va.uv.x) + (w1 * vb.uv.x) + (w2 * vc.uv.x);
            const float v = (w0 * va.uv.y) + (w1 * vb.uv.y) + (w2 * vc.uv.y);
            const float vr = (w0 * ar) + (w1 * br) + (w2 * cr);
            const float vg = (w0 * ag) + (w1 * bg) + (w2 * cg);
            const float vb_col = (w0 * ab) + (w1 * bb) + (w2 * cb);
            const float va_col = (w0 * aa) + (w1 * ba) + (w2 * ca);

            float tr = 1.0f;
            float tg = 1.0f;
            float tb = 1.0f;
            float ta = 1.0f;
            if (texture_id == atlas_texture_id) {
                SampleFontAtlas(atlas_pixels, atlas_w, atlas_h, u, v, tr, tg, tb, ta);
            }

            const float src_r = std::clamp(vr * tr, 0.0f, 1.0f);
            const float src_g = std::clamp(vg * tg, 0.0f, 1.0f);
            const float src_b = std::clamp(vb_col * tb, 0.0f, 1.0f);
            const float src_a = std::clamp(va_col * ta, 0.0f, 1.0f);
            if (src_a <= 0.0f) {
                continue;
            }

            const size_t dst_idx =
                (static_cast<size_t>(y) * static_cast<size_t>(target_w) + static_cast<size_t>(x)) * 4u;
            const float dst_r = static_cast<float>(target[dst_idx + 0]) / 255.0f;
            const float dst_g = static_cast<float>(target[dst_idx + 1]) / 255.0f;
            const float dst_b = static_cast<float>(target[dst_idx + 2]) / 255.0f;
            const float dst_a = static_cast<float>(target[dst_idx + 3]) / 255.0f;

            // Composite in straight-alpha space so backend blend state
            // (src alpha / inv src alpha) produces expected overlay opacity.
            const float out_a = src_a + (dst_a * (1.0f - src_a));
            float out_r = 0.0f;
            float out_g = 0.0f;
            float out_b = 0.0f;
            if (out_a > 1e-6f) {
                out_r = ((src_r * src_a) + (dst_r * dst_a * (1.0f - src_a))) / out_a;
                out_g = ((src_g * src_a) + (dst_g * dst_a * (1.0f - src_a))) / out_a;
                out_b = ((src_b * src_a) + (dst_b * dst_a * (1.0f - src_a))) / out_a;
            }

            target[dst_idx + 0] = static_cast<uint8_t>(std::clamp(out_r * 255.0f, 0.0f, 255.0f));
            target[dst_idx + 1] = static_cast<uint8_t>(std::clamp(out_g * 255.0f, 0.0f, 255.0f));
            target[dst_idx + 2] = static_cast<uint8_t>(std::clamp(out_b * 255.0f, 0.0f, 255.0f));
            target[dst_idx + 3] = static_cast<uint8_t>(std::clamp(out_a * 255.0f, 0.0f, 255.0f));
        }
    }
}

} // namespace

void RasterizeDrawData(const ImDrawData* draw_data,
                       int target_w,
                       int target_h,
                       ImTextureID atlas_texture_id,
                       const std::vector<uint8_t>& atlas_pixels,
                       int atlas_w,
                       int atlas_h,
                       std::vector<uint8_t>& target_pixels,
                       RasterStats& stats) {
    stats = {};
    if (target_w <= 0 || target_h <= 0) {
        target_pixels.clear();
        return;
    }

    target_pixels.assign(static_cast<size_t>(target_w) * static_cast<size_t>(target_h) * 4u, 0);
    if (!draw_data || draw_data->CmdListsCount <= 0) {
        return;
    }

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;
    stats.cmd_lists = static_cast<uint32_t>(draw_data->CmdListsCount);
    stats.vertices = static_cast<uint32_t>(draw_data->TotalVtxCount);
    stats.indices = static_cast<uint32_t>(draw_data->TotalIdxCount);

    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
        const ImDrawList* cmd_list = draw_data->CmdLists[list_index];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
        for (int cmd_index = 0; cmd_index < cmd_list->CmdBuffer.Size; ++cmd_index) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_index];
            if (pcmd->UserCallback != nullptr) {
                continue;
            }
            ++stats.cmds;

            ImVec4 clip_rect;
            clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

            const int clip_min_x = static_cast<int>(std::floor(std::max(clip_rect.x, 0.0f)));
            const int clip_min_y = static_cast<int>(std::floor(std::max(clip_rect.y, 0.0f)));
            const int clip_max_x = static_cast<int>(std::ceil(std::min(clip_rect.z, static_cast<float>(target_w))));
            const int clip_max_y = static_cast<int>(std::ceil(std::min(clip_rect.w, static_cast<float>(target_h))));
            if (clip_min_x >= clip_max_x || clip_min_y >= clip_max_y) {
                continue;
            }

            const ImDrawIdx* idx = idx_buffer + pcmd->IdxOffset;
            for (unsigned int i = 0; i + 2 < pcmd->ElemCount; i += 3) {
                const unsigned int i0 = static_cast<unsigned int>(idx[i + 0]) + pcmd->VtxOffset;
                const unsigned int i1 = static_cast<unsigned int>(idx[i + 1]) + pcmd->VtxOffset;
                const unsigned int i2 = static_cast<unsigned int>(idx[i + 2]) + pcmd->VtxOffset;
                if (i0 >= static_cast<unsigned int>(cmd_list->VtxBuffer.Size) ||
                    i1 >= static_cast<unsigned int>(cmd_list->VtxBuffer.Size) ||
                    i2 >= static_cast<unsigned int>(cmd_list->VtxBuffer.Size)) {
                    continue;
                }
                RasterizeTriangle(vtx_buffer[i0],
                                  vtx_buffer[i1],
                                  vtx_buffer[i2],
                                  pcmd->GetTexID(),
                                  atlas_texture_id,
                                  clip_min_x,
                                  clip_min_y,
                                  clip_max_x,
                                  clip_max_y,
                                  target_w,
                                  target_h,
                                  atlas_pixels,
                                  atlas_w,
                                  atlas_h,
                                  target_pixels);
            }
        }
    }
}

} // namespace karma::ui::imgui

#endif // defined(KARMA_HAS_IMGUI)

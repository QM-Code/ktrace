#include "ui/ui_backend.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(KARMA_HAS_IMGUI)
#include <imgui.h>
#endif

namespace karma::ui {
namespace {

#if defined(KARMA_HAS_IMGUI)
struct RasterStats {
    uint32_t cmd_lists = 0;
    uint32_t cmds = 0;
    uint32_t vertices = 0;
    uint32_t indices = 0;
};

ImGuiKey MapKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return ImGuiKey_A;
        case platform::Key::B: return ImGuiKey_B;
        case platform::Key::C: return ImGuiKey_C;
        case platform::Key::D: return ImGuiKey_D;
        case platform::Key::E: return ImGuiKey_E;
        case platform::Key::F: return ImGuiKey_F;
        case platform::Key::G: return ImGuiKey_G;
        case platform::Key::H: return ImGuiKey_H;
        case platform::Key::I: return ImGuiKey_I;
        case platform::Key::J: return ImGuiKey_J;
        case platform::Key::K: return ImGuiKey_K;
        case platform::Key::L: return ImGuiKey_L;
        case platform::Key::M: return ImGuiKey_M;
        case platform::Key::N: return ImGuiKey_N;
        case platform::Key::O: return ImGuiKey_O;
        case platform::Key::P: return ImGuiKey_P;
        case platform::Key::Q: return ImGuiKey_Q;
        case platform::Key::R: return ImGuiKey_R;
        case platform::Key::S: return ImGuiKey_S;
        case platform::Key::T: return ImGuiKey_T;
        case platform::Key::U: return ImGuiKey_U;
        case platform::Key::V: return ImGuiKey_V;
        case platform::Key::W: return ImGuiKey_W;
        case platform::Key::X: return ImGuiKey_X;
        case platform::Key::Y: return ImGuiKey_Y;
        case platform::Key::Z: return ImGuiKey_Z;
        case platform::Key::Num0: return ImGuiKey_0;
        case platform::Key::Num1: return ImGuiKey_1;
        case platform::Key::Num2: return ImGuiKey_2;
        case platform::Key::Num3: return ImGuiKey_3;
        case platform::Key::Num4: return ImGuiKey_4;
        case platform::Key::Num5: return ImGuiKey_5;
        case platform::Key::Num6: return ImGuiKey_6;
        case platform::Key::Num7: return ImGuiKey_7;
        case platform::Key::Num8: return ImGuiKey_8;
        case platform::Key::Num9: return ImGuiKey_9;
        case platform::Key::Minus: return ImGuiKey_Minus;
        case platform::Key::Equals: return ImGuiKey_Equal;
        case platform::Key::LeftBracket: return ImGuiKey_LeftBracket;
        case platform::Key::RightBracket: return ImGuiKey_RightBracket;
        case platform::Key::Backslash: return ImGuiKey_Backslash;
        case platform::Key::Semicolon: return ImGuiKey_Semicolon;
        case platform::Key::Apostrophe: return ImGuiKey_Apostrophe;
        case platform::Key::Comma: return ImGuiKey_Comma;
        case platform::Key::Slash: return ImGuiKey_Slash;
        case platform::Key::Grave: return ImGuiKey_GraveAccent;
        case platform::Key::LeftControl: return ImGuiKey_LeftCtrl;
        case platform::Key::RightControl: return ImGuiKey_RightCtrl;
        case platform::Key::LeftAlt: return ImGuiKey_LeftAlt;
        case platform::Key::RightAlt: return ImGuiKey_RightAlt;
        case platform::Key::LeftSuper: return ImGuiKey_LeftSuper;
        case platform::Key::RightSuper: return ImGuiKey_RightSuper;
        case platform::Key::Menu: return ImGuiKey_Menu;
        case platform::Key::Home: return ImGuiKey_Home;
        case platform::Key::End: return ImGuiKey_End;
        case platform::Key::PageUp: return ImGuiKey_PageUp;
        case platform::Key::PageDown: return ImGuiKey_PageDown;
        case platform::Key::Insert: return ImGuiKey_Insert;
        case platform::Key::Delete: return ImGuiKey_Delete;
        case platform::Key::CapsLock: return ImGuiKey_CapsLock;
        case platform::Key::NumLock: return ImGuiKey_NumLock;
        case platform::Key::ScrollLock: return ImGuiKey_ScrollLock;
        case platform::Key::Left: return ImGuiKey_LeftArrow;
        case platform::Key::Right: return ImGuiKey_RightArrow;
        case platform::Key::Up: return ImGuiKey_UpArrow;
        case platform::Key::Down: return ImGuiKey_DownArrow;
        case platform::Key::LeftShift: return ImGuiKey_LeftShift;
        case platform::Key::RightShift: return ImGuiKey_RightShift;
        case platform::Key::F1: return ImGuiKey_F1;
        case platform::Key::F2: return ImGuiKey_F2;
        case platform::Key::F3: return ImGuiKey_F3;
        case platform::Key::F4: return ImGuiKey_F4;
        case platform::Key::F5: return ImGuiKey_F5;
        case platform::Key::F6: return ImGuiKey_F6;
        case platform::Key::F7: return ImGuiKey_F7;
        case platform::Key::F8: return ImGuiKey_F8;
        case platform::Key::F9: return ImGuiKey_F9;
        case platform::Key::F10: return ImGuiKey_F10;
        case platform::Key::F11: return ImGuiKey_F11;
        case platform::Key::F12: return ImGuiKey_F12;
        case platform::Key::Enter: return ImGuiKey_Enter;
        case platform::Key::Space: return ImGuiKey_Space;
        case platform::Key::Tab: return ImGuiKey_Tab;
        case platform::Key::Period: return ImGuiKey_Period;
        case platform::Key::Backspace: return ImGuiKey_Backspace;
        case platform::Key::Escape: return ImGuiKey_Escape;
        case platform::Key::Unknown:
        default: return ImGuiKey_None;
    }
}

int MapMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return ImGuiMouseButton_Left;
        case platform::MouseButton::Right: return ImGuiMouseButton_Right;
        case platform::MouseButton::Middle: return ImGuiMouseButton_Middle;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        default: return -1;
    }
}

void PushModifiers(ImGuiIO& io, const platform::Modifiers& mods) {
    io.AddKeyEvent(ImGuiMod_Shift, mods.shift);
    io.AddKeyEvent(ImGuiMod_Ctrl, mods.ctrl);
    io.AddKeyEvent(ImGuiMod_Alt, mods.alt);
    io.AddKeyEvent(ImGuiMod_Super, mods.super);
}

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

class ImGuiBackend final : public UiBackend {
 public:
    const char* name() const override {
        return "imgui";
    }

    bool init() override {
        IMGUI_CHECKVERSION();
        context_ = ImGui::CreateContext();
        if (!context_) {
            KARMA_TRACE("ui.system.imgui", "UiBackend[{}]: failed to create context", name());
            return false;
        }
        ImGui::SetCurrentContext(context_);
        ImGui::StyleColorsDark();

        output_width_ = std::max<uint16_t>(256u, config::ReadUInt16Config({"ui.imgui.SoftwareBridge.Width"}, 1024u));
        output_height_ = std::max<uint16_t>(144u, config::ReadUInt16Config({"ui.imgui.SoftwareBridge.Height"}, 576u));

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(static_cast<float>(output_width_), static_cast<float>(output_height_));
        io.DeltaTime = 1.0f / 60.0f;

        unsigned char* font_pixels = nullptr;
        int font_width = 0;
        int font_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
        if (!font_pixels || font_width <= 0 || font_height <= 0) {
            KARMA_TRACE("ui.system.imgui", "UiBackend[{}]: failed to build font atlas", name());
            return false;
        }
        atlas_width_ = font_width;
        atlas_height_ = font_height;
        atlas_pixels_.assign(font_pixels, font_pixels + static_cast<size_t>(font_width * font_height * 4));
        io.Fonts->SetTexID(atlas_texture_id_);

        texture_.width = output_width_;
        texture_.height = output_height_;
        texture_.channels = 4;
        texture_.pixels.assign(static_cast<size_t>(output_width_) * static_cast<size_t>(output_height_) * 4u, 0);
        texture_revision_ = 1;

        KARMA_TRACE("ui.system.imgui",
                    "UiBackend[{}]: initialized output={}x{} font={}x{}",
                    name(),
                    output_width_,
                    output_height_,
                    atlas_width_,
                    atlas_height_);
        return true;
    }

    void shutdown() override {
        if (context_) {
            ImGui::SetCurrentContext(context_);
            ImGui::DestroyContext(context_);
            context_ = nullptr;
        }
        atlas_pixels_.clear();
        working_pixels_.clear();
    }

    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        if (!context_) {
            return;
        }
        ImGui::SetCurrentContext(context_);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(output_width_), static_cast<float>(output_height_));
        io.DeltaTime = dt > 0.0f ? dt : (1.0f / 60.0f);

        for (const auto& event : events) {
            switch (event.type) {
                case platform::EventType::MouseMove:
                    io.AddMousePosEvent(static_cast<float>(event.mouse_x), static_cast<float>(event.mouse_y));
                    break;
                case platform::EventType::MouseButtonDown:
                case platform::EventType::MouseButtonUp: {
                    PushModifiers(io, event.mods);
                    const int button = MapMouseButton(event.mouse_button);
                    if (button >= 0) {
                        io.AddMouseButtonEvent(button, event.type == platform::EventType::MouseButtonDown);
                    }
                    break;
                }
                case platform::EventType::MouseScroll:
                    io.AddMouseWheelEvent(event.scroll_x, event.scroll_y);
                    break;
                case platform::EventType::KeyDown:
                case platform::EventType::KeyUp: {
                    PushModifiers(io, event.mods);
                    const ImGuiKey key = MapKey(event.key);
                    if (key != ImGuiKey_None) {
                        io.AddKeyEvent(key, event.type == platform::EventType::KeyDown);
                    }
                    break;
                }
                case platform::EventType::TextInput:
                    if (!event.text.empty()) {
                        io.AddInputCharactersUTF8(event.text.c_str());
                    } else if (event.codepoint != 0) {
                        io.AddInputCharacter(event.codepoint);
                    }
                    break;
                case platform::EventType::WindowFocus:
                    io.AddFocusEvent(event.focused);
                    break;
                default:
                    break;
            }
        }
    }

    void build(const std::vector<UiDrawContext::ImGuiDrawCallback>& imgui_draw_callbacks,
               const std::vector<UiDrawContext::RmlUiDrawCallback>& rmlui_draw_callbacks,
               const std::vector<UiDrawContext::TextPanel>& text_panels,
               UiOverlayFrame& out) override {
        if (!context_) {
            return;
        }
        (void)rmlui_draw_callbacks;

        ImGui::SetCurrentContext(context_);
        ImGui::NewFrame();

        bool drew_ui = false;
        for (const auto& panel : text_panels) {
            ImGui::SetNextWindowPos(ImVec2(panel.x, panel.y), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(panel.bg_alpha);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize;
            if (panel.auto_size) {
                flags |= ImGuiWindowFlags_AlwaysAutoResize;
            }
            ImGui::Begin(panel.title.c_str(), nullptr, flags);
            for (const auto& line : panel.lines) {
                ImGui::TextUnformatted(line.c_str());
            }
            ImGui::End();
            drew_ui = true;
        }

        for (const auto& callback : imgui_draw_callbacks) {
            if (!callback) {
                continue;
            }
            callback();
            drew_ui = true;
        }
        if (!drew_ui && logging::ShouldTraceChannel("ui.system.imgui.frames")) {
            ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400.0f, 140.0f), ImGuiCond_Always);
            ImGui::Begin("ImGui Backend Debug", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Text("No game UI callbacks submitted.");
            ImGui::Text("Enable only for backend diagnostics.");
            ImGui::Separator();
            ImGui::Text("Last draw lists=%u cmds=%u", last_stats_.cmd_lists, last_stats_.cmds);
            ImGui::End();
        }
        ImGui::Render();
        const ImDrawData* draw_data = ImGui::GetDrawData();
        if (!draw_data || draw_data->TotalIdxCount <= 0) {
            out.allow_fallback = false;
            out.wants_mouse_capture = ImGui::GetIO().WantCaptureMouse;
            out.wants_keyboard_capture = ImGui::GetIO().WantCaptureKeyboard;
            return;
        }

        RasterStats stats{};
        RasterizeDrawData(draw_data,
                          output_width_,
                          output_height_,
                          atlas_texture_id_,
                          atlas_pixels_,
                          atlas_width_,
                          atlas_height_,
                          working_pixels_,
                          stats);
        last_stats_ = stats;

        const bool size_changed = texture_.width != output_width_ || texture_.height != output_height_;
        const bool content_changed =
            size_changed ||
            texture_.pixels.size() != working_pixels_.size() ||
            (texture_.pixels.size() == working_pixels_.size() &&
             std::memcmp(texture_.pixels.data(), working_pixels_.data(), texture_.pixels.size()) != 0);

        if (content_changed) {
            texture_.width = output_width_;
            texture_.height = output_height_;
            texture_.channels = 4;
            texture_.pixels = working_pixels_;
            ++texture_revision_;
            const std::string stats_key =
                std::to_string(stats.cmd_lists) + ":" +
                std::to_string(stats.cmds) + ":" +
                std::to_string(stats.vertices) + ":" +
                std::to_string(stats.indices) + ":" +
                std::to_string(output_width_) + "x" +
                std::to_string(output_height_);
            KARMA_TRACE_CHANGED("ui.system.imgui",
                                stats_key,
                                "UiBackend[{}]: texture profile lists={} cmds={} vtx={} idx={} size={}x{}",
                                name(),
                                stats.cmd_lists,
                                stats.cmds,
                                stats.vertices,
                                stats.indices,
                                output_width_,
                                output_height_);
        }

        out.texture = &texture_;
        out.texture_revision = texture_revision_;
        out.distance = 0.75f;
        out.width = 0.95f;
        out.height = 0.95f * (static_cast<float>(output_height_) / static_cast<float>(output_width_));
        out.wants_mouse_capture = ImGui::GetIO().WantCaptureMouse;
        out.wants_keyboard_capture = ImGui::GetIO().WantCaptureKeyboard;
    }

 private:
    ImGuiContext* context_ = nullptr;
    renderer::MeshData::TextureData texture_{};
    std::vector<uint8_t> working_pixels_{};
    std::vector<uint8_t> atlas_pixels_{};
    int atlas_width_ = 0;
    int atlas_height_ = 0;
    ImTextureID atlas_texture_id_ = static_cast<ImTextureID>(1);
    uint64_t texture_revision_ = 1;
    uint16_t output_width_ = 640;
    uint16_t output_height_ = 360;
    RasterStats last_stats_{};
};
#else
class ImGuiBackend final : public UiBackend {
 public:
    const char* name() const override {
        return "imgui";
    }
    bool init() override {
        KARMA_TRACE("ui.system.imgui", "UiBackend[{}]: imgui support not compiled", name());
        return false;
    }
    void shutdown() override {}
    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        (void)dt;
        (void)events;
    }
    void build(const std::vector<UiDrawContext::ImGuiDrawCallback>& imgui_draw_callbacks,
               const std::vector<UiDrawContext::RmlUiDrawCallback>& rmlui_draw_callbacks,
               const std::vector<UiDrawContext::TextPanel>& text_panels,
               UiOverlayFrame& out) override {
        (void)imgui_draw_callbacks;
        (void)rmlui_draw_callbacks;
        (void)text_panels;
        (void)out;
    }
};
#endif

} // namespace

std::unique_ptr<UiBackend> CreateImGuiBackend() {
    return std::make_unique<ImGuiBackend>();
}

} // namespace karma::ui

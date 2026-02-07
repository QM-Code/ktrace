#include "ui/ui_rmlui_adapter.hpp"

#if defined(KARMA_HAS_RMLUI)

#include "karma/common/config_helpers.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace karma::ui {
namespace {

struct RmlGeometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

struct RmlTexture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

struct RmlDrawCall {
    Rml::CompiledGeometryHandle geometry = 0;
    Rml::Vector2f translation{0.0f, 0.0f};
    Rml::TextureHandle texture = 0;
    bool scissor_enabled = false;
    Rml::Rectanglei scissor = Rml::Rectanglei::FromSize({0, 0});
};

inline int ClampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(hi, value));
}

inline float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

std::string EscapeRmlText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

int MapModifiers(const platform::Modifiers& mods) {
    int flags = 0;
    if (mods.ctrl) {
        flags |= Rml::Input::KM_CTRL;
    }
    if (mods.shift) {
        flags |= Rml::Input::KM_SHIFT;
    }
    if (mods.alt) {
        flags |= Rml::Input::KM_ALT;
    }
    if (mods.super) {
        flags |= Rml::Input::KM_META;
    }
    return flags;
}

Rml::Input::KeyIdentifier MapKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return Rml::Input::KI_A;
        case platform::Key::B: return Rml::Input::KI_B;
        case platform::Key::C: return Rml::Input::KI_C;
        case platform::Key::D: return Rml::Input::KI_D;
        case platform::Key::E: return Rml::Input::KI_E;
        case platform::Key::F: return Rml::Input::KI_F;
        case platform::Key::G: return Rml::Input::KI_G;
        case platform::Key::H: return Rml::Input::KI_H;
        case platform::Key::I: return Rml::Input::KI_I;
        case platform::Key::J: return Rml::Input::KI_J;
        case platform::Key::K: return Rml::Input::KI_K;
        case platform::Key::L: return Rml::Input::KI_L;
        case platform::Key::M: return Rml::Input::KI_M;
        case platform::Key::N: return Rml::Input::KI_N;
        case platform::Key::O: return Rml::Input::KI_O;
        case platform::Key::P: return Rml::Input::KI_P;
        case platform::Key::Q: return Rml::Input::KI_Q;
        case platform::Key::R: return Rml::Input::KI_R;
        case platform::Key::S: return Rml::Input::KI_S;
        case platform::Key::T: return Rml::Input::KI_T;
        case platform::Key::U: return Rml::Input::KI_U;
        case platform::Key::V: return Rml::Input::KI_V;
        case platform::Key::W: return Rml::Input::KI_W;
        case platform::Key::X: return Rml::Input::KI_X;
        case platform::Key::Y: return Rml::Input::KI_Y;
        case platform::Key::Z: return Rml::Input::KI_Z;
        case platform::Key::Num0: return Rml::Input::KI_0;
        case platform::Key::Num1: return Rml::Input::KI_1;
        case platform::Key::Num2: return Rml::Input::KI_2;
        case platform::Key::Num3: return Rml::Input::KI_3;
        case platform::Key::Num4: return Rml::Input::KI_4;
        case platform::Key::Num5: return Rml::Input::KI_5;
        case platform::Key::Num6: return Rml::Input::KI_6;
        case platform::Key::Num7: return Rml::Input::KI_7;
        case platform::Key::Num8: return Rml::Input::KI_8;
        case platform::Key::Num9: return Rml::Input::KI_9;
        case platform::Key::Minus: return Rml::Input::KI_OEM_MINUS;
        case platform::Key::Equals: return Rml::Input::KI_OEM_PLUS;
        case platform::Key::LeftBracket: return Rml::Input::KI_OEM_4;
        case platform::Key::RightBracket: return Rml::Input::KI_OEM_6;
        case platform::Key::Backslash: return Rml::Input::KI_OEM_5;
        case platform::Key::Semicolon: return Rml::Input::KI_OEM_1;
        case platform::Key::Apostrophe: return Rml::Input::KI_OEM_7;
        case platform::Key::Comma: return Rml::Input::KI_OEM_COMMA;
        case platform::Key::Slash: return Rml::Input::KI_OEM_2;
        case platform::Key::Grave: return Rml::Input::KI_OEM_3;
        case platform::Key::LeftControl: return Rml::Input::KI_LCONTROL;
        case platform::Key::RightControl: return Rml::Input::KI_RCONTROL;
        case platform::Key::LeftAlt: return Rml::Input::KI_LMENU;
        case platform::Key::RightAlt: return Rml::Input::KI_RMENU;
        case platform::Key::LeftSuper: return Rml::Input::KI_LMETA;
        case platform::Key::RightSuper: return Rml::Input::KI_RMETA;
        case platform::Key::Menu: return Rml::Input::KI_APPS;
        case platform::Key::Home: return Rml::Input::KI_HOME;
        case platform::Key::End: return Rml::Input::KI_END;
        case platform::Key::PageUp: return Rml::Input::KI_PRIOR;
        case platform::Key::PageDown: return Rml::Input::KI_NEXT;
        case platform::Key::Insert: return Rml::Input::KI_INSERT;
        case platform::Key::Delete: return Rml::Input::KI_DELETE;
        case platform::Key::CapsLock: return Rml::Input::KI_CAPITAL;
        case platform::Key::NumLock: return Rml::Input::KI_NUMLOCK;
        case platform::Key::ScrollLock: return Rml::Input::KI_SCROLL;
        case platform::Key::Left: return Rml::Input::KI_LEFT;
        case platform::Key::Right: return Rml::Input::KI_RIGHT;
        case platform::Key::Up: return Rml::Input::KI_UP;
        case platform::Key::Down: return Rml::Input::KI_DOWN;
        case platform::Key::LeftShift: return Rml::Input::KI_LSHIFT;
        case platform::Key::RightShift: return Rml::Input::KI_RSHIFT;
        case platform::Key::F1: return Rml::Input::KI_F1;
        case platform::Key::F2: return Rml::Input::KI_F2;
        case platform::Key::F3: return Rml::Input::KI_F3;
        case platform::Key::F4: return Rml::Input::KI_F4;
        case platform::Key::F5: return Rml::Input::KI_F5;
        case platform::Key::F6: return Rml::Input::KI_F6;
        case platform::Key::F7: return Rml::Input::KI_F7;
        case platform::Key::F8: return Rml::Input::KI_F8;
        case platform::Key::F9: return Rml::Input::KI_F9;
        case platform::Key::F10: return Rml::Input::KI_F10;
        case platform::Key::F11: return Rml::Input::KI_F11;
        case platform::Key::F12: return Rml::Input::KI_F12;
        case platform::Key::Enter: return Rml::Input::KI_RETURN;
        case platform::Key::Space: return Rml::Input::KI_SPACE;
        case platform::Key::Tab: return Rml::Input::KI_TAB;
        case platform::Key::Period: return Rml::Input::KI_OEM_PERIOD;
        case platform::Key::Backspace: return Rml::Input::KI_BACK;
        case platform::Key::Escape: return Rml::Input::KI_ESCAPE;
        case platform::Key::Unknown:
        default: return Rml::Input::KI_UNKNOWN;
    }
}

int MapMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}

class RmlSystemInterface final : public Rml::SystemInterface {
 public:
    double GetElapsedTime() override {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - start_;
        return elapsed.count();
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        switch (type) {
            case Rml::Log::Type::LT_ERROR:
                spdlog::error("RmlUi: {}", message);
                break;
            case Rml::Log::Type::LT_WARNING:
                spdlog::warn("RmlUi: {}", message);
                break;
            case Rml::Log::Type::LT_INFO:
            case Rml::Log::Type::LT_DEBUG:
            case Rml::Log::Type::LT_ASSERT:
            default:
                KARMA_TRACE("ui.system.rmlui", "RmlUi: {}", message);
                break;
        }
        return true;
    }

 private:
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

class RmlCpuRenderInterface final : public Rml::RenderInterface {
 public:
    bool beginFrame(int width, int height) {
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

    void rasterize() {
        for (const auto& call : draw_calls_) {
            const auto geometry_it = geometries_.find(call.geometry);
            if (geometry_it == geometries_.end()) {
                continue;
            }

            const RmlTexture* texture = nullptr;
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

    const renderer::MeshData::TextureData& frameTexture() const {
        return frame_texture_;
    }

    size_t drawCallCount() const {
        return draw_call_count_;
    }

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override {
        if (vertices.empty() || indices.empty()) {
            return 0;
        }

        const Rml::CompiledGeometryHandle handle = next_geometry_handle_++;
        RmlGeometry geometry{};
        geometry.vertices.assign(vertices.begin(), vertices.end());
        geometry.indices.assign(indices.begin(), indices.end());
        geometries_[handle] = std::move(geometry);
        return handle;
    }

    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override {
        if (geometry == 0) {
            return;
        }
        RmlDrawCall call{};
        call.geometry = geometry;
        call.translation = translation;
        call.texture = texture;
        call.scissor_enabled = scissor_enabled_;
        call.scissor = scissor_;
        draw_calls_.push_back(call);
        ++draw_call_count_;
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
        geometries_.erase(geometry);
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override {
        texture_dimensions = Rml::Vector2i(0, 0);
        const std::string source_key = source;
        if (!missing_texture_sources_.contains(source_key)) {
            missing_texture_sources_.insert(source_key);
            KARMA_TRACE("ui.system.rmlui",
                        "UiRmlUiAdapter[rmlui]: external texture load not implemented '{}'",
                        source_key);
        }
        return 0;
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                       Rml::Vector2i source_dimensions) override {
        if (source_dimensions.x <= 0 || source_dimensions.y <= 0) {
            return 0;
        }

        const size_t required_bytes =
            static_cast<size_t>(source_dimensions.x) * static_cast<size_t>(source_dimensions.y) * 4u;
        if (source.size() < required_bytes) {
            return 0;
        }

        RmlTexture texture{};
        texture.width = source_dimensions.x;
        texture.height = source_dimensions.y;
        texture.pixels.assign(source.begin(), source.begin() + static_cast<ptrdiff_t>(required_bytes));

        const Rml::TextureHandle handle = next_texture_handle_++;
        textures_[handle] = std::move(texture);
        return handle;
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        textures_.erase(texture);
    }

    void EnableScissorRegion(bool enable) override {
        scissor_enabled_ = enable;
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        scissor_ = region;
    }

 private:
    static void sampleTexture(const RmlTexture* texture, float u, float v, float& r, float& g, float& b, float& a) {
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

    void rasterizeTriangle(const Rml::Vertex& va,
                           const Rml::Vertex& vb,
                           const Rml::Vertex& vc,
                           const Rml::Vector2f& translation,
                           const RmlTexture* texture,
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

        const int tri_min_x = std::max(clip_min_x, std::max(0, static_cast<int>(std::floor(std::min({ax, bx, cx})))));
        const int tri_min_y = std::max(clip_min_y, std::max(0, static_cast<int>(std::floor(std::min({ay, by, cy})))));
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

    int frame_width_ = 0;
    int frame_height_ = 0;
    bool scissor_enabled_ = false;
    Rml::Rectanglei scissor_ = Rml::Rectanglei::FromSize({0, 0});
    size_t draw_call_count_ = 0;
    Rml::CompiledGeometryHandle next_geometry_handle_ = 1;
    Rml::TextureHandle next_texture_handle_ = 1;
    std::unordered_map<Rml::CompiledGeometryHandle, RmlGeometry> geometries_{};
    std::unordered_map<Rml::TextureHandle, RmlTexture> textures_{};
    std::unordered_set<std::string> missing_texture_sources_{};
    std::vector<RmlDrawCall> draw_calls_{};
    renderer::MeshData::TextureData frame_texture_{};
};

std::string BuildBaseDocument() {
    return R"(<rml>
<head>
<style>
body {
    margin: 0px;
    background-color: rgba(0,0,0,0);
    color: #d7dce8;
    font-family: "Google Sans";
    font-size: 16px;
}
#root {
    position: absolute;
    left: 0px;
    top: 0px;
    width: 100%;
    height: 100%;
}
.panel {
    position: absolute;
    display: inline-block;
    padding: 10px 12px;
    border-width: 1px;
    border-style: solid;
    border-color: #7084a0;
    background-color: rgba(12, 18, 26, 210);
}
.title {
    margin-bottom: 8px;
    font-size: 20px;
    color: #8ec8ff;
}
.line {
    margin-bottom: 2px;
    white-space: pre;
}
</style>
</head>
<body>
<div id="root"></div>
</body>
</rml>)";
}

std::string BuildPanelsMarkup(const std::vector<UiDrawContext::TextPanel>& text_panels) {
    auto estimate_panel_size = [](const UiDrawContext::TextPanel& panel) -> std::pair<int, int> {
        size_t max_chars = panel.title.size();
        for (const auto& line : panel.lines) {
            max_chars = std::max(max_chars, line.size());
        }
        // Conservative layout estimate for the temporary debug overlay.
        const int text_width = static_cast<int>(max_chars * 9u);
        const int width = std::max(280, text_width + 24);
        const int height = std::max(96, 36 + static_cast<int>(panel.lines.size()) * 20 + 12);
        return {width, height};
    };

    std::string markup;
    markup.reserve(1024);
    for (const auto& panel : text_panels) {
        const auto [panel_width, panel_height] = estimate_panel_size(panel);
        const int bg_alpha = static_cast<int>(std::lround(std::clamp(panel.bg_alpha, 0.0f, 1.0f) * 255.0f));
        markup.append("<div class=\"panel\" style=\"left:");
        markup.append(std::to_string(static_cast<int>(std::lround(panel.x))));
        markup.append("px;top:");
        markup.append(std::to_string(static_cast<int>(std::lround(panel.y))));
        markup.append("px;width:");
        markup.append(std::to_string(panel_width));
        markup.append("px;height:");
        markup.append(std::to_string(panel_height));
        markup.append("px;background-color:rgba(12,18,26,");
        markup.append(std::to_string(bg_alpha));
        markup.append(");\">");
        markup.append("<div class=\"title\">");
        markup.append(EscapeRmlText(panel.title));
        markup.append("</div>");
        for (const auto& line : panel.lines) {
            markup.append("<div class=\"line\">");
            markup.append(EscapeRmlText(line));
            markup.append("</div>");
        }
        markup.append("</div>");
    }
    return markup;
}

class UiRmlUiAdapterReal final : public UiRmlUiAdapter {
 public:
    bool init() override {
        output_width_ =
            std::max<uint16_t>(256u, config::ReadUInt16Config({"ui.rmlui.SoftwareBridge.Width"}, 1024u));
        output_height_ =
            std::max<uint16_t>(144u, config::ReadUInt16Config({"ui.rmlui.SoftwareBridge.Height"}, 576u));
        distance_ = config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.Distance"}, 0.75f);
        width_ = config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.WidthMeters"}, 0.95f);
        height_ = config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.HeightMeters"}, 0.95f);
        allow_fallback_ = config::ReadBoolConfig({"ui.rmlui.SoftwareBridge.AllowFallback"}, false);

        Rml::SetSystemInterface(&system_interface_);
        Rml::SetRenderInterface(&render_interface_);
        if (!Rml::Initialise()) {
            spdlog::error("UiRmlUiAdapter[rmlui]: RmlUi::Initialise failed");
            return false;
        }
        initialized_ = true;

        context_ = Rml::CreateContext("karma-ui-rmlui", Rml::Vector2i(output_width_, output_height_));
        if (!context_) {
            spdlog::error("UiRmlUiAdapter[rmlui]: failed to create RmlUi context");
            return false;
        }

        const auto font_regular =
            data::ResolveConfiguredAsset("assets.hud.fonts.console.Regular.Font", "client/fonts/GoogleSans.ttf");
        if (!font_regular.empty()) {
            const std::string font_regular_str = font_regular.string();
            const bool loaded = Rml::LoadFontFace(font_regular_str.c_str(), true);
            KARMA_TRACE("ui.system.rmlui",
                        "UiRmlUiAdapter[rmlui]: load font '{}' success={}",
                        font_regular_str,
                        loaded ? 1 : 0);
            if (!loaded) {
                spdlog::warn("UiRmlUiAdapter[rmlui]: failed to load font '{}'", font_regular_str);
            }
        } else {
            KARMA_TRACE("ui.system.rmlui", "UiRmlUiAdapter[rmlui]: regular font asset resolved empty path");
        }

        document_ = context_->LoadDocumentFromMemory(BuildBaseDocument(), "[engine-ui]");
        if (!document_) {
            spdlog::error("UiRmlUiAdapter[rmlui]: failed to create base document");
            return false;
        }
        document_->Show();
        root_ = document_->GetElementById("root");
        if (!root_) {
            spdlog::error("UiRmlUiAdapter[rmlui]: missing root element");
            return false;
        }

        texture_.width = output_width_;
        texture_.height = output_height_;
        texture_.channels = 4;
        texture_.pixels.assign(static_cast<size_t>(output_width_) * static_cast<size_t>(output_height_) * 4u, 0);
        texture_revision_ = 1;

        KARMA_TRACE("ui.system.rmlui",
                    "UiRmlUiAdapter[rmlui]: initialized output={}x{}",
                    output_width_,
                    output_height_);
        return true;
    }

    void shutdown() override {
        if (document_) {
            document_->Close();
            document_ = nullptr;
            root_ = nullptr;
        }
        if (context_) {
            const std::string context_name = context_->GetName();
            Rml::RemoveContext(context_name);
            context_ = nullptr;
        }
        if (initialized_) {
            Rml::Shutdown();
            initialized_ = false;
        }
    }

    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        (void)dt;
        if (!context_) {
            return;
        }

        for (const auto& event : events) {
            const int modifiers = MapModifiers(event.mods);
            switch (event.type) {
                case platform::EventType::MouseMove:
                    context_->ProcessMouseMove(event.mouse_x, event.mouse_y, modifiers);
                    break;
                case platform::EventType::MouseButtonDown:
                    context_->ProcessMouseButtonDown(MapMouseButton(event.mouse_button), modifiers);
                    break;
                case platform::EventType::MouseButtonUp:
                    context_->ProcessMouseButtonUp(MapMouseButton(event.mouse_button), modifiers);
                    break;
                case platform::EventType::MouseScroll:
                    context_->ProcessMouseWheel(Rml::Vector2f(event.scroll_x, event.scroll_y), modifiers);
                    break;
                case platform::EventType::KeyDown: {
                    const auto key = MapKey(event.key);
                    if (key != Rml::Input::KI_UNKNOWN) {
                        context_->ProcessKeyDown(key, modifiers);
                    }
                    break;
                }
                case platform::EventType::KeyUp: {
                    const auto key = MapKey(event.key);
                    if (key != Rml::Input::KI_UNKNOWN) {
                        context_->ProcessKeyUp(key, modifiers);
                    }
                    break;
                }
                case platform::EventType::TextInput:
                    if (!event.text.empty()) {
                        context_->ProcessTextInput(event.text);
                    } else if (event.codepoint != 0) {
                        context_->ProcessTextInput(static_cast<Rml::Character>(event.codepoint));
                    }
                    break;
                case platform::EventType::WindowFocus:
                    if (!event.focused) {
                        context_->ProcessMouseLeave();
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void build(const std::vector<UiDrawContext::RmlUiDrawCallback>& draw_callbacks,
               const std::vector<UiDrawContext::TextPanel>& text_panels,
               UiOverlayFrame& out) override {
        if (!context_ || !root_) {
            return;
        }

        for (const auto& callback : draw_callbacks) {
            if (callback) {
                callback();
            }
        }

        const std::string panel_markup = BuildPanelsMarkup(text_panels);
        if (panel_markup != last_markup_) {
            root_->SetInnerRML(panel_markup);
            last_markup_ = panel_markup;
        }

        context_->SetDimensions(Rml::Vector2i(output_width_, output_height_));
        if (!render_interface_.beginFrame(output_width_, output_height_)) {
            return;
        }
        context_->Update();
        context_->Render();
        render_interface_.rasterize();

        const auto& frame_texture = render_interface_.frameTexture();
        const bool texture_changed =
            texture_.pixels.size() != frame_texture.pixels.size() ||
            std::memcmp(texture_.pixels.data(), frame_texture.pixels.data(), frame_texture.pixels.size()) != 0;
        if (texture_changed) {
            texture_ = frame_texture;
            ++texture_revision_;
        }

        out.distance = distance_;
        out.width = width_;
        out.height = height_ * (static_cast<float>(output_height_) / static_cast<float>(output_width_));
        out.allow_fallback = allow_fallback_;
        out.wants_mouse_capture = context_->IsMouseInteracting();
        out.wants_keyboard_capture = context_->GetFocusElement() != nullptr;

        if (render_interface_.drawCallCount() == 0) {
            KARMA_TRACE_CHANGED("ui.system.rmlui",
                                std::string("0:") + std::to_string(text_panels.size()),
                                "UiRmlUiAdapter[rmlui]: draw_calls=0 panels={} (no overlay texture submitted)",
                                text_panels.size());
            return;
        }
        out.texture = &texture_;
        out.texture_revision = texture_revision_;

        KARMA_TRACE_CHANGED("ui.system.rmlui",
                            std::to_string(render_interface_.drawCallCount()) + ":" +
                                std::to_string(text_panels.size()),
                            "UiRmlUiAdapter[rmlui]: draw_calls={} panels={}",
                            render_interface_.drawCallCount(),
                            text_panels.size());

        if (logging::ShouldTraceChannel("ui.system.rmlui.frames")) {
            KARMA_TRACE("ui.system.rmlui.frames",
                        "UiRmlUiAdapter[rmlui]: frame callbacks={} panels={} draw_calls={} texture_rev={}",
                        draw_callbacks.size(),
                        text_panels.size(),
                        render_interface_.drawCallCount(),
                        texture_revision_);
        }
    }

 private:
    RmlSystemInterface system_interface_{};
    RmlCpuRenderInterface render_interface_{};
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    Rml::Element* root_ = nullptr;
    uint16_t output_width_ = 1024;
    uint16_t output_height_ = 576;
    float distance_ = 0.75f;
    float width_ = 0.95f;
    float height_ = 0.95f;
    bool allow_fallback_ = false;
    bool initialized_ = false;
    renderer::MeshData::TextureData texture_{};
    uint64_t texture_revision_ = 0;
    std::string last_markup_{};
};

} // namespace

std::unique_ptr<UiRmlUiAdapter> CreateRmlUiAdapter() {
    return std::make_unique<UiRmlUiAdapterReal>();
}

} // namespace karma::ui

#endif // defined(KARMA_HAS_RMLUI)

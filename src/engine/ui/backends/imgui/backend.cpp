#include "ui/backends/imgui/internal.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(KARMA_HAS_IMGUI)

#include <imgui.h>

namespace karma::ui {
namespace {

class ImGuiBackend final : public BackendDriver {
 public:
    const char* name() const override {
        return "imgui";
    }

    bool init() override {
        IMGUI_CHECKVERSION();
        context_ = ImGui::CreateContext();
        if (!context_) {
            KARMA_TRACE("ui.system.imgui", "Backend[{}]: failed to create context", name());
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
            KARMA_TRACE("ui.system.imgui", "Backend[{}]: failed to build font atlas", name());
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
                    "Backend[{}]: initialized output={}x{} font={}x{}",
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
                    imgui::PushModifiers(io, event.mods);
                    const int button = imgui::MapMouseButton(event.mouse_button);
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
                    imgui::PushModifiers(io, event.mods);
                    const ImGuiKey key = imgui::MapKey(event.key);
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
               OverlayFrame& out) override {
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

        imgui::RasterStats stats{};
        imgui::RasterizeDrawData(draw_data,
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
                                "Backend[{}]: texture profile lists={} cmds={} vtx={} idx={} size={}x{}",
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
    imgui::RasterStats last_stats_{};
};

} // namespace

std::unique_ptr<BackendDriver> CreateImGuiBackend() {
    return std::make_unique<ImGuiBackend>();
}

} // namespace karma::ui

#else

namespace karma::ui {

std::unique_ptr<BackendDriver> CreateImGuiBackend() {
    return imgui::CreateStubBackend();
}

} // namespace karma::ui

#endif // defined(KARMA_HAS_IMGUI)

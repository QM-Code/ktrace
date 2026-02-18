#include "ui/backends/rmlui/internal.hpp"

#if defined(KARMA_HAS_RMLUI)

#include "karma/common/config/helpers.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/common/logging/logging.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace karma::ui::rmlui {
namespace {

class AdapterReal final : public Adapter {
 public:
    bool init() override {
        output_width_ =
            std::max<uint16_t>(256u, common::config::ReadUInt16Config({"ui.rmlui.SoftwareBridge.Width"}, 1024u));
        output_height_ =
            std::max<uint16_t>(144u, common::config::ReadUInt16Config({"ui.rmlui.SoftwareBridge.Height"}, 576u));
        distance_ = common::config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.Distance"}, 0.75f);
        width_ = common::config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.WidthMeters"}, 0.95f);
        height_ = common::config::ReadFloatConfig({"ui.rmlui.SoftwareBridge.HeightMeters"}, 0.95f);
        allow_fallback_ = common::config::ReadBoolConfig({"ui.rmlui.SoftwareBridge.AllowFallback"}, false);

        Rml::SetSystemInterface(&system_interface_);
        Rml::SetRenderInterface(&render_interface_);
        if (!Rml::Initialise()) {
            spdlog::error("RmlAdapter[rmlui]: RmlUi::Initialise failed");
            return false;
        }
        initialized_ = true;

        context_ = Rml::CreateContext("karma-ui-rmlui", Rml::Vector2i(output_width_, output_height_));
        if (!context_) {
            spdlog::error("RmlAdapter[rmlui]: failed to create RmlUi context");
            return false;
        }

        const auto font_regular =
            common::data::ResolveConfiguredAsset("assets.hud.fonts.console.Regular.Font", "client/fonts/GoogleSans.ttf");
        if (!font_regular.empty()) {
            const std::string font_regular_str = font_regular.string();
            const bool loaded = Rml::LoadFontFace(font_regular_str.c_str(), true);
            KARMA_TRACE("ui.system.rmlui",
                        "RmlAdapter[rmlui]: load font '{}' success={}",
                        font_regular_str,
                        loaded ? 1 : 0);
            if (!loaded) {
                spdlog::warn("RmlAdapter[rmlui]: failed to load font '{}'", font_regular_str);
            }
        } else {
            KARMA_TRACE("ui.system.rmlui", "RmlAdapter[rmlui]: regular font asset resolved empty path");
        }

        document_ = context_->LoadDocumentFromMemory(BuildBaseDocument(), "[engine-ui]");
        if (!document_) {
            spdlog::error("RmlAdapter[rmlui]: failed to create base document");
            return false;
        }
        document_->Show();
        root_ = document_->GetElementById("root");
        if (!root_) {
            spdlog::error("RmlAdapter[rmlui]: missing root element");
            return false;
        }

        texture_.width = output_width_;
        texture_.height = output_height_;
        texture_.channels = 4;
        texture_.pixels.assign(static_cast<size_t>(output_width_) * static_cast<size_t>(output_height_) * 4u, 0);
        texture_revision_ = 1;

        KARMA_TRACE("ui.system.rmlui",
                    "RmlAdapter[rmlui]: initialized output={}x{}",
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
               OverlayFrame& out) override {
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
                                "RmlAdapter[rmlui]: draw_calls=0 panels={} (no overlay texture submitted)",
                                text_panels.size());
            return;
        }
        out.texture = &texture_;
        out.texture_revision = texture_revision_;

        KARMA_TRACE_CHANGED("ui.system.rmlui",
                            std::to_string(render_interface_.drawCallCount()) + ":" +
                                std::to_string(text_panels.size()),
                            "RmlAdapter[rmlui]: draw_calls={} panels={}",
                            render_interface_.drawCallCount(),
                            text_panels.size());

        if (common::logging::ShouldTraceChannel("ui.system.rmlui.frames")) {
            KARMA_TRACE("ui.system.rmlui.frames",
                        "RmlAdapter[rmlui]: frame callbacks={} panels={} draw_calls={} texture_rev={}",
                        draw_callbacks.size(),
                        text_panels.size(),
                        render_interface_.drawCallCount(),
                        texture_revision_);
        }
    }

 private:
    SystemInterface system_interface_{};
    CpuRenderInterface render_interface_{};
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

std::unique_ptr<Adapter> CreateAdapter() {
    return std::make_unique<AdapterReal>();
}

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)

#include "ui/backends/rmlui/adapter.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"

#if !defined(KARMA_HAS_RMLUI)

#include <cstddef>
#include <cstdint>
#include <string>

namespace karma::ui::rmlui {
namespace {

renderer::MeshData::TextureData BuildRmlDebugTexture(int width, int height) {
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool checker = ((x / 12) + (y / 12)) % 2 == 0;
            const size_t idx =
                (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            tex.pixels[idx + 0] = checker ? 30 : 44;
            tex.pixels[idx + 1] = checker ? 58 : 78;
            tex.pixels[idx + 2] = checker ? 94 : 132;
            tex.pixels[idx + 3] = 255;
        }
    }
    return tex;
}

class AdapterStub final : public Adapter {
 public:
    bool init() override {
        debug_overlay_ = config::ReadBoolConfig({"ui.rmlui.stub.DebugOverlay"}, false);
        allow_fallback_ = config::ReadBoolConfig({"ui.rmlui.stub.AllowFallback"}, false);
        distance_ = config::ReadFloatConfig({"ui.rmlui.stub.Distance"}, 0.75f);
        width_ = config::ReadFloatConfig({"ui.rmlui.stub.Width"}, 1.1f);
        height_ = config::ReadFloatConfig({"ui.rmlui.stub.Height"}, 0.62f);

        texture_ = BuildRmlDebugTexture(256, 144);
        texture_revision_ = 1;
        KARMA_TRACE("ui.system.rmlui",
                    "RmlAdapter[stub]: initialized debug_overlay={} allow_fallback={}",
                    debug_overlay_,
                    allow_fallback_);
        return true;
    }

    void shutdown() override {}

    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        (void)dt;
        (void)events;
    }

    void build(const std::vector<UiDrawContext::RmlUiDrawCallback>& draw_callbacks,
               const std::vector<UiDrawContext::TextPanel>& text_panels,
               OverlayFrame& out) override {
        for (const auto& callback : draw_callbacks) {
            if (callback) {
                callback();
            }
        }

        const size_t callback_count = draw_callbacks.size();
        const size_t panel_count = text_panels.size();
        KARMA_TRACE_CHANGED("ui.system.rmlui",
                            std::to_string(callback_count) + ":" + std::to_string(panel_count),
                            "RmlAdapter[stub]: callbacks={} text_panels={} (bridge)",
                            callback_count,
                            panel_count);

        out.distance = distance_;
        out.width = width_;
        out.height = height_;
        out.allow_fallback = allow_fallback_;

        const bool show_overlay =
            debug_overlay_ || logging::ShouldTraceChannel("ui.system.rmlui.frames");
        if (!show_overlay) {
            out.allow_fallback = false;
            return;
        }

        out.texture = &texture_;
        out.texture_revision = texture_revision_;
    }

 private:
    bool debug_overlay_ = false;
    bool allow_fallback_ = false;
    float distance_ = 0.75f;
    float width_ = 1.1f;
    float height_ = 0.62f;
    renderer::MeshData::TextureData texture_{};
    uint64_t texture_revision_ = 0;
};

} // namespace

std::unique_ptr<Adapter> CreateAdapter() {
    return std::make_unique<AdapterStub>();
}

} // namespace karma::ui::rmlui

#endif // !defined(KARMA_HAS_RMLUI)

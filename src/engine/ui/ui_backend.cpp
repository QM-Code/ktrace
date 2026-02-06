#include "ui/ui_backend.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"

#include <cstddef>
#include <cstdint>

namespace karma::ui {
namespace {

renderer::MeshData::TextureData BuildOverlayTexture(int width, int height) {
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool checker = ((x / 16) + (y / 16)) % 2 == 0;
            const uint8_t grain = static_cast<uint8_t>((x * 13 + y * 7) & 0xff);
            const uint8_t r = checker ? static_cast<uint8_t>(220u - (grain / 4u))
                                      : static_cast<uint8_t>(40u + (grain / 3u));
            const uint8_t g = checker ? static_cast<uint8_t>(80u + (grain / 3u))
                                      : static_cast<uint8_t>(170u - (grain / 4u));
            const uint8_t b = checker ? static_cast<uint8_t>(40u + (grain / 5u))
                                      : static_cast<uint8_t>(220u - (grain / 6u));
            const size_t idx =
                (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            tex.pixels[idx + 0] = r;
            tex.pixels[idx + 1] = g;
            tex.pixels[idx + 2] = b;
            tex.pixels[idx + 3] = 255;
        }
    }

    return tex;
}

class SoftwareOverlayBackend final : public UiBackend {
 public:
    const char* name() const override {
        return "software-overlay";
    }

    bool init() override {
        enabled_ = config::ReadBoolConfig({"ui.overlayTest.Enabled"}, true);
        distance_ = config::ReadFloatConfig({"ui.overlayTest.Distance"}, 0.75f);
        width_ = config::ReadFloatConfig({"ui.overlayTest.Width"}, 1.2f);
        height_ = config::ReadFloatConfig({"ui.overlayTest.Height"}, 0.7f);

        if (!enabled_) {
            KARMA_TRACE("ui.system", "UiBackend[{}]: disabled via ui.overlayTest.Enabled", name());
            return true;
        }

        texture_ = BuildOverlayTexture(128, 128);
        texture_revision_ = 1;
        KARMA_TRACE("ui.system",
                    "UiBackend[{}]: texture ready {}x{}",
                    name(),
                    texture_.width,
                    texture_.height);
        return true;
    }

    void shutdown() override {}

    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        (void)dt;
        (void)events;
    }

    void build(UiOverlayFrame& out) override {
        out.distance = distance_;
        out.width = width_;
        out.height = height_;

        if (!enabled_) {
            return;
        }
        out.texture = &texture_;
        out.texture_revision = texture_revision_;
    }

 private:
    bool enabled_ = true;
    float distance_ = 0.75f;
    float width_ = 1.2f;
    float height_ = 0.7f;
    renderer::MeshData::TextureData texture_{};
    uint64_t texture_revision_ = 0;
};

} // namespace

std::unique_ptr<UiBackend> CreateSoftwareOverlayBackend() {
    return std::make_unique<SoftwareOverlayBackend>();
}

} // namespace karma::ui

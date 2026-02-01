#include "ui/core/ui_layer_adapter.hpp"
#include "ui/core/system.hpp"

UiLayerAdapter::UiLayerAdapter(UiSystem &system)
    : system_(system) {}

void UiLayerAdapter::onEvent(const platform::Event &event) {
    pending_events_.push_back(event);
}

void UiLayerAdapter::onFrame(karma::app::UIContext &ctx) {
    if (!pending_events_.empty()) {
        system_.handleEvents(pending_events_);
        pending_events_.clear();
    }
    system_.update();

    ctx.setBrightness(system_.getRenderBrightness());

    auto &draw = ctx.drawData();
    draw.clear();

    if (system_.buildDrawData(ctx)) {
        return;
    }

    const auto output = system_.getRenderOutput();
    if (!output.valid()) {
        return;
    }

    const auto handle = ctx.registerExternalTexture(output.texture);
    if (handle == 0) {
        return;
    }

    const auto frame = ctx.frame();
    const float w = static_cast<float>(frame.viewport_w);
    const float h = static_cast<float>(frame.viewport_h);
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    draw.vertices = {
        {0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff},
        {w, 0.0f, 1.0f, 0.0f, 0xffffffff},
        {w, h, 1.0f, 1.0f, 0xffffffff},
        {0.0f, h, 0.0f, 1.0f, 0xffffffff},
    };
    draw.indices = {0, 1, 2, 0, 2, 3};
    karma::app::UIDrawCmd cmd{};
    cmd.index_offset = 0;
    cmd.index_count = 6;
    cmd.texture = handle;
    draw.commands = {cmd};
    draw.premultiplied_alpha = false;
}

void UiLayerAdapter::onShutdown() {
    pending_events_.clear();
}

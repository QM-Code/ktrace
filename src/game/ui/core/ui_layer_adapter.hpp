#pragma once

#include <vector>

#include "karma/app/ui_context.h"
#include "karma/platform/events.hpp"

class UiSystem;

class UiLayerAdapter final : public karma::app::UiLayer {
public:
    explicit UiLayerAdapter(UiSystem &system);
    ~UiLayerAdapter() override = default;

    void onFrame(karma::app::UIContext &ctx) override;
    void onEvent(const platform::Event &event) override;
    void onShutdown() override;

private:
    UiSystem &system_;
    std::vector<platform::Event> pending_events_;
};

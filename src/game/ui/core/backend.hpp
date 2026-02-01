#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "karma/platform/events.hpp"
#include "ui/models/hud_model.hpp"
#include "ui/models/hud_render_state.hpp"
#include "ui/core/types.hpp"
#include "karma_extras/ui/bridges/renderer_bridge.hpp"
#include "ui/console/console_interface.hpp"

namespace platform {
class Window;
}

namespace ui_backend {

class Backend {
public:
    virtual ~Backend() = default;

    virtual ui::ConsoleInterface &console() = 0;
    virtual const ui::ConsoleInterface &console() const = 0;
    virtual void handleEvents(const std::vector<platform::Event> &events) = 0;
    virtual void update() = 0;
    virtual void reloadFonts() = 0;

    virtual void setHudModel(const ui::HudModel &model) = 0;
    virtual void addConsoleLine(const std::string &playerName, const std::string &line) = 0;
    virtual std::string getChatInputBuffer() const = 0;
    virtual void clearChatInputBuffer() = 0;
    virtual void focusChatInput() = 0;
    virtual bool getChatInputFocus() const = 0;
    virtual bool consumeKeybindingsReloadRequest() = 0;
    virtual std::optional<ui::QuickMenuAction> consumeQuickMenuAction() = 0;
    virtual void setRendererBridge(const ui::RendererBridge *bridge) = 0;
    virtual ui::RenderOutput getRenderOutput() const { return {}; }
    virtual float getRenderBrightness() const { return 1.0f; }
    virtual bool isRenderBrightnessDragActive() const { return false; }
    virtual bool isUiInputEnabled() const = 0;
    virtual const char *name() const = 0;
    virtual ui::HudRenderState getHudRenderState() const = 0;
};

std::unique_ptr<Backend> CreateUiBackend(platform::Window &window);

} // namespace ui_backend

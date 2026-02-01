#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "karma/platform/events.hpp"
#include "ui/controllers/hud_controller.hpp"
#include "ui/models/hud_model.hpp"
#include "ui/core/types.hpp"
#include "ui/core/validation.hpp"
#include "ui/console/console_interface.hpp"
#include "karma_extras/ui/overlay.hpp"
#include "karma_extras/ui/bridges/renderer_bridge.hpp"
#include "karma/app/ui_context.h"

namespace platform {
class Window;
}

namespace ui_backend {
class Backend;
}

class UiSystem {

public:
    UiSystem(platform::Window &window);
    ~UiSystem();

    ui::ConsoleInterface &console();
    const ui::ConsoleInterface &console() const;
    void handleEvents(const std::vector<platform::Event> &events);
    void update();

private:
    std::unique_ptr<ui_backend::Backend> backend;
    ui::HudModel hudModel;
    ui::HudController hudController;
    uint64_t lastConfigRevision = 0;
    bool validateHudState = false;
    ui::HudValidator hudValidator;

    void reloadFonts();

public:
    void setLanguage(const std::string &language);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setDialogText(const std::string &text);
    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;
    void setDialogVisible(bool show);
    void setQuickMenuVisible(bool show);
    void toggleQuickMenuVisible();
    bool isQuickMenuVisible() const;
    std::optional<ui::QuickMenuAction> consumeQuickMenuAction();
    bool consumeKeybindingsReloadRequest();
    void setRendererBridge(const ui::RendererBridge *bridge);
    ui::RenderOutput getRenderOutput() const;
    bool buildDrawData(karma::app::UIContext &ctx);
    float getRenderBrightness() const;
    bool isUiInputEnabled() const;
    bool isGameplayInputEnabled() const;
};

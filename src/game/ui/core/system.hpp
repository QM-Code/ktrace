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
#include "karma/ui/overlay.hpp"
#include "karma_extras/ui/bridges/renderer_bridge.hpp"

namespace platform {
class Window;
}

namespace ui_backend {
class Backend;
}

class UiSystem : public ui::Overlay {

public:
    UiSystem(platform::Window &window);
    ~UiSystem();

    ui::ConsoleInterface &console();
    const ui::ConsoleInterface &console() const;
    void handleEvents(const std::vector<platform::Event> &events) override;
    void update() override;

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
    ui::RenderOutput getRenderOutput() const override;
    float getRenderBrightness() const override;
    bool isUiInputEnabled() const;
    bool isGameplayInputEnabled() const;
};

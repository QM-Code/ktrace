#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/frontends/imgui/hud/hud.hpp"
#include "ui/frontends/imgui/console/console.hpp"
#include "ui/core/backend.hpp"

namespace window {
class Window;
}

namespace ui::backend {

class ImGuiBackend final : public Backend {
public:
    explicit ImGuiBackend(window::Window &window);
    ~ImGuiBackend() override;

    ui::ConsoleInterface &console() override;
    const ui::ConsoleInterface &console() const override;
    void handleEvents(const std::vector<window::Event> &events) override;
    void update() override;
    void reloadFonts() override;

    void setHudModel(const ui::HudModel &model) override;
    void addConsoleLine(const std::string &playerName, const std::string &line) override;
    std::string getChatInputBuffer() const override;
    void clearChatInputBuffer() override;
    void focusChatInput() override;
    bool getChatInputFocus() const override;
    bool consumeKeybindingsReloadRequest() override;
    std::optional<ui::QuickMenuAction> consumeQuickMenuAction() override;
    void setRendererBridge(const ui::RendererBridge *bridge) override;
    bool buildDrawData(karma::app::UIContext &ctx) override;
    ui::RenderOutput getRenderOutput() const override;
    float getRenderBrightness() const override { return consoleView.getRenderBrightness(); }
    bool isRenderBrightnessDragActive() const override;
    bool isUiInputEnabled() const override {
        return consoleView.isVisible() || hud.getChatInputFocus() || quickMenuVisible;
    }
    const char *name() const override { return "imgui"; }
    ui::HudRenderState getHudRenderState() const override { return lastHudRenderState; }

private:
    window::Window *window = nullptr;
    std::chrono::steady_clock::time_point lastFrameTime;
    ImFont *bigFont = nullptr;
    ui::ConsoleView consoleView;
    ui::ImGuiHud hud;
    ui::HudModel hudModel;
    const ui::RendererBridge *rendererBridge = nullptr;
    ui::UiRenderTargetBridge* uiBridge = nullptr;
    bool languageReloadArmed = false;
    std::optional<std::string> pendingLanguage;
    bool fontsDirty = false;
    bool outputVisible = false;
    bool quickMenuVisible = false;
    std::optional<ui::QuickMenuAction> pendingQuickMenuAction;
    ui::HudRenderState lastHudRenderState{};
    void drawTexture(const graphics::TextureHandle& texture);
};

} // namespace ui::backend

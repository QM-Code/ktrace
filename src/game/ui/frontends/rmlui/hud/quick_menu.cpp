#include "ui/frontends/rmlui/hud/quick_menu.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

namespace ui {

class RmlUiHudQuickMenu::QuickMenuListener final : public Rml::EventListener {
public:
    QuickMenuListener(RmlUiHudQuickMenu *menuIn, QuickMenuAction actionIn)
        : menu(menuIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (menu) {
            menu->setPendingAction(action);
        }
        event.StopPropagation();
    }

private:
    RmlUiHudQuickMenu *menu = nullptr;
    QuickMenuAction action;
};

void RmlUiHudQuickMenu::bind(Rml::ElementDocument *document) {
    overlay = nullptr;
    consoleButton = nullptr;
    resumeButton = nullptr;
    disconnectButton = nullptr;
    quitButton = nullptr;
    listeners.clear();
    if (!document) {
        return;
    }
    overlay = document->GetElementById("hud-quick-menu-overlay");
    consoleButton = document->GetElementById("hud-quick-menu-console");
    resumeButton = document->GetElementById("hud-quick-menu-resume");
    disconnectButton = document->GetElementById("hud-quick-menu-disconnect");
    quitButton = document->GetElementById("hud-quick-menu-quit");
    bindButton(consoleButton, QuickMenuAction::OpenConsole);
    bindButton(resumeButton, QuickMenuAction::Resume);
    bindButton(disconnectButton, QuickMenuAction::Disconnect);
    bindButton(quitButton, QuickMenuAction::Quit);
    if (overlay) {
        overlay->SetClass("hidden", !visible);
    }
}

void RmlUiHudQuickMenu::show(bool visibleIn) {
    visible = visibleIn;
    if (overlay) {
        overlay->SetClass("hidden", !visible);
    }
}

std::optional<QuickMenuAction> RmlUiHudQuickMenu::consumeAction() {
    if (!pendingAction) {
        return std::nullopt;
    }
    auto action = *pendingAction;
    pendingAction.reset();
    return action;
}

void RmlUiHudQuickMenu::bindButton(Rml::Element *button, QuickMenuAction action) {
    if (!button) {
        return;
    }
    auto listener = std::make_unique<QuickMenuListener>(this, action);
    button->AddEventListener("click", listener.get());
    listeners.emplace_back(std::move(listener));
}

void RmlUiHudQuickMenu::setPendingAction(QuickMenuAction action) {
    pendingAction = action;
}

} // namespace ui

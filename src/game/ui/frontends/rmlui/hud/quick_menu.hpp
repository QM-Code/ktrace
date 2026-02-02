#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <RmlUi/Core/EventListener.h>

#include "ui/core/types.hpp"

namespace Rml {
class Element;
class ElementDocument;
}

namespace ui {

class RmlUiHudQuickMenu {
public:
    void bind(Rml::ElementDocument *document);
    void show(bool visible);
    bool isVisible() const { return visible; }
    std::optional<QuickMenuAction> consumeAction();

private:
    class QuickMenuListener;
    Rml::Element *overlay = nullptr;
    Rml::Element *consoleButton = nullptr;
    Rml::Element *resumeButton = nullptr;
    Rml::Element *disconnectButton = nullptr;
    Rml::Element *quitButton = nullptr;
    bool visible = false;
    std::optional<QuickMenuAction> pendingAction;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;

    void bindButton(Rml::Element *button, QuickMenuAction action);
    void setPendingAction(QuickMenuAction action);
};

} // namespace ui

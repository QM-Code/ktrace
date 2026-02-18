#include "ui/frontends/rmlui/console/panels/panel.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include "karma/common/data/path_resolver.hpp"
#include "karma/common/i18n/i18n.hpp"
#include "ui/frontends/rmlui/translate.hpp"

namespace ui {

RmlUiPanel::RmlUiPanel(std::string key, std::string rmlPath)
    : panelKey(std::move(key)), panelRmlPath(std::move(rmlPath)) {}

const std::string &RmlUiPanel::key() const {
    return panelKey;
}

void RmlUiPanel::load(Rml::ElementDocument *document) {
    if (!document) {
        return;
    }
    const std::string panelId = "panel-" + panelKey;
    Rml::Element *panel = document->GetElementById(panelId);
    if (!panel) {
        return;
    }

    const auto resolvedPath = karma::common::data::Resolve(panelRmlPath);
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        return;
    }
    std::ifstream file(resolvedPath);
    if (!file) {
        spdlog::warn("RmlUi: failed to open panel file '{}'.", resolvedPath.string());
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    panel->SetInnerRML(buffer.str());
    rmlui::ApplyTranslations(panel, karma::common::i18n::Get());
    onLoaded(document);
}

void RmlUiPanel::onLoaded(Rml::ElementDocument *) {}

void RmlUiPanel::update() {
    onTick();
}

void RmlUiPanel::onUpdate() {}

void RmlUiPanel::show() {
    onShow();
}

void RmlUiPanel::hide() {
    onHide();
}

void RmlUiPanel::configChanged() {
    onConfigChanged();
}

void RmlUiPanel::onShow() {}

void RmlUiPanel::onHide() {}

void RmlUiPanel::onConfigChanged() {}

void RmlUiPanel::onTick() {
    onUpdate();
}

} // namespace ui

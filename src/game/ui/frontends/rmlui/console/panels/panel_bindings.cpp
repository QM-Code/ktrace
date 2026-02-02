#include "ui/frontends/rmlui/console/panels/panel_bindings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <sstream>

#include "ui/console/status_banner.hpp"
#include "ui/console/keybindings.hpp"

namespace ui {
namespace {

std::string escapeRmlText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

void WriteBuffer(std::array<char, 128> &buffer, const std::string &value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

} // namespace

class RmlUiPanelBindings::BindingCellListener final : public Rml::EventListener {
public:
    BindingCellListener(RmlUiPanelBindings *panelIn, int rowIndexIn, ui::BindingsModel::Column columnIn)
        : panel(panelIn), rowIndex(rowIndexIn), column(columnIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->setSelected(rowIndex, column);
        }
    }

private:
    RmlUiPanelBindings *panel = nullptr;
    int rowIndex = 0;
    ui::BindingsModel::Column column = ui::BindingsModel::Column::Keyboard;
};

class RmlUiPanelBindings::SettingsActionListener final : public Rml::EventListener {
public:
    enum class Action {
        Clear,
        Save,
        Reset
    };

    SettingsActionListener(RmlUiPanelBindings *panelIn, Action actionIn)
        : panel(panelIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        switch (action) {
            case Action::Clear: panel->clearSelected(); break;
            case Action::Save: panel->saveBindings(); break;
            case Action::Reset: panel->showResetDialog(); break;
        }
    }

private:
    RmlUiPanelBindings *panel = nullptr;
    Action action = Action::Clear;
};

class RmlUiPanelBindings::SettingsKeyListener final : public Rml::EventListener {
public:
    explicit SettingsKeyListener(RmlUiPanelBindings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
        panel->captureKey(keyIdentifier);
    }

private:
    RmlUiPanelBindings *panel = nullptr;
};

class RmlUiPanelBindings::SettingsMouseListener final : public Rml::EventListener {
public:
    explicit SettingsMouseListener(RmlUiPanelBindings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int button = event.GetParameter<int>("button", -1);
        panel->handleMouseClick(event.GetTargetElement(), button);
        panel->captureMouse(button);
    }

private:
    RmlUiPanelBindings *panel = nullptr;
};

RmlUiPanelBindings::RmlUiPanelBindings()
    : RmlUiPanel("bindings", "client/ui/console_panel_bindings.rml") {}

void RmlUiPanelBindings::setUserConfigPath(const std::string &path) {
    (void)path;
    bindingsModel.loaded = false;
    bindingsModel.statusText.clear();
    bindingsModel.statusIsError = false;
    bindingsModel.selectedIndex = -1;
    bindingsModel.selectedColumn = ui::BindingsModel::Column::Keyboard;
    selectionJustChanged = false;

    if (document) {
        loadBindings();
        rebuildBindings();
        updateSelectedLabel();
        updateStatus();
    }
}

bool RmlUiPanelBindings::consumeKeybindingsReloadRequest() {
    const bool requested = bindingsModel.keybindingsReloadRequested;
    bindingsModel.keybindingsReloadRequested = false;
    return requested;
}

void RmlUiPanelBindings::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    bindingsList = document->GetElementById("bindings-list-inner");
    selectedLabel = document->GetElementById("bindings-selected");
    statusLabel = document->GetElementById("bindings-status");
    clearButton = document->GetElementById("bindings-clear");
    saveButton = document->GetElementById("bindings-save");
    resetButton = document->GetElementById("bindings-reset");
    resetDialog.bind(document,
                     "bindings-reset-overlay",
                     "bindings-reset-message",
                     "bindings-reset-yes",
                     "bindings-reset-no");
    resetDialog.setOnAccept([this]() {
        resetBindings();
        resetDialog.hide();
    });
    resetDialog.setOnCancel([this]() { resetDialog.hide(); });

    listeners.clear();
    if (clearButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Clear);
        clearButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (saveButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Save);
        saveButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (resetButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Reset);
        resetButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (document) {
        auto listener = std::make_unique<SettingsKeyListener>(this);
        document->AddEventListener("keydown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (document) {
        auto listener = std::make_unique<SettingsMouseListener>(this);
        document->AddEventListener("mousedown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    resetDialog.installListeners(listeners);

    loadBindings();
    rebuildBindings();
    updateSelectedLabel();
    updateStatus();
}

void RmlUiPanelBindings::onUpdate() {
    if (!document) {
        return;
    }
    if (!bindingsModel.loaded) {
        loadBindings();
        rebuildBindings();
        updateSelectedLabel();
        updateStatus();
    }
    selectionJustChanged = false;
}

void RmlUiPanelBindings::loadBindings() {
    const auto result = bindingsController.loadFromConfig();
    if (!result.status.empty()) {
        showStatus(result.status, result.statusIsError);
    }
}

void RmlUiPanelBindings::rebuildBindings() {
    if (!bindingsList || !document) {
        return;
    }
    bindingsList->SetInnerRML("");
    rowListeners.clear();
    rows.clear();
    auto defs = ui::bindings::Definitions();
    const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
    rows.reserve(count);

    auto appendElement = [&](Rml::Element *parent, const char *tag) -> Rml::Element* {
        auto child = document->CreateElement(tag);
        Rml::Element *ptr = child.get();
        parent->AppendChild(std::move(child));
        return ptr;
    };

    for (std::size_t i = 0; i < count; ++i) {
        auto *row = appendElement(bindingsList, "div");
        row->SetClass("bindings-row", true);

        auto *actionCell = appendElement(row, "div");
        actionCell->SetClass("bindings-cell", true);
        actionCell->SetClass("action", true);
        actionCell->SetInnerRML(escapeRmlText(defs[i].label));

        if (defs[i].isHeader) {
            row->SetClass("section", true);
            BindingRow bindingRow;
            bindingRow.action = actionCell;
            rows.push_back(bindingRow);
            continue;
        }

        auto makeBindingCell = [&](ui::BindingsModel::Column column, const std::string &value, const char *columnClass) -> Rml::Element* {
            auto *cell = appendElement(row, "div");
            cell->SetClass("bindings-cell", true);
            cell->SetClass(columnClass, true);
            auto *binding = appendElement(cell, "div");
            binding->SetClass("binding-cell", true);
            std::string display = value.empty() ? "Unbound" : value;
            binding->SetInnerRML(escapeRmlText(display));
            auto listener = std::make_unique<BindingCellListener>(this, static_cast<int>(i), column);
            binding->AddEventListener("click", listener.get());
            rowListeners.emplace_back(std::move(listener));
            return binding;
        };

        BindingRow bindingRow;
        bindingRow.action = actionCell;
        bindingRow.keyboard = makeBindingCell(ui::BindingsModel::Column::Keyboard, bindingsModel.keyboard[i].data(), "keyboard");
        bindingRow.mouse = makeBindingCell(ui::BindingsModel::Column::Mouse, bindingsModel.mouse[i].data(), "mouse");
        bindingRow.controller = makeBindingCell(ui::BindingsModel::Column::Controller, bindingsModel.controller[i].data(), "controller");
        rows.push_back(bindingRow);
    }
    updateSelectedLabel();
    updateStatus();
    selectionJustChanged = false;
    updateSelectedLabel();
}

void RmlUiPanelBindings::updateSelectedLabel() {
    if (!selectedLabel) {
        return;
    }
    std::string label = "Selected cell: None";
    auto defs = ui::bindings::Definitions();
    const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
    if (bindingsModel.selectedIndex >= 0 && bindingsModel.selectedIndex < static_cast<int>(count)) {
        if (defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].isHeader) {
            selectedLabel->SetInnerRML(escapeRmlText(label));
            return;
        }
        const char *colName = bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard
            ? "Keyboard"
            : (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse ? "Mouse" : "Controller");
        label = std::string("Selected cell: ") + defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].label + " / " + colName;
    }
    selectedLabel->SetInnerRML(escapeRmlText(label));
}

void RmlUiPanelBindings::updateStatus() {
    if (!statusLabel) {
        return;
    }
    const auto banner = ui::status_banner::MakeStatusBanner(bindingsModel.statusText,
                                                            bindingsModel.statusIsError);
    if (!banner.visible) {
        statusLabel->SetClass("hidden", true);
        return;
    }
    statusLabel->SetClass("hidden", false);
    statusLabel->SetClass("status-error", banner.tone == ui::MessageTone::Error);
    statusLabel->SetClass("status-pending", banner.tone == ui::MessageTone::Pending);
    const std::string text = ui::status_banner::FormatStatusText(banner);
    statusLabel->SetInnerRML(escapeRmlText(text));
}

void RmlUiPanelBindings::setSelected(int index, ui::BindingsModel::Column column) {
    bindingsModel.selectedIndex = index;
    bindingsModel.selectedColumn = column;
    selectionJustChanged = true;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto setSelected = [&](Rml::Element *element, ui::BindingsModel::Column col) {
            if (!element) {
                return;
            }
            const bool selected = (static_cast<int>(i) == bindingsModel.selectedIndex && col == bindingsModel.selectedColumn);
            element->SetClass("selected", selected);
        };
        setSelected(rows[i].keyboard, ui::BindingsModel::Column::Keyboard);
        setSelected(rows[i].mouse, ui::BindingsModel::Column::Mouse);
        setSelected(rows[i].controller, ui::BindingsModel::Column::Controller);
    }
    updateSelectedLabel();
}

void RmlUiPanelBindings::clearSelection() {
    bindingsModel.selectedIndex = -1;
    for (auto &row : rows) {
        if (row.keyboard) {
            row.keyboard->SetClass("selected", false);
        }
        if (row.mouse) {
            row.mouse->SetClass("selected", false);
        }
        if (row.controller) {
            row.controller->SetClass("selected", false);
        }
    }
    updateSelectedLabel();
}

void RmlUiPanelBindings::clearSelected() {
    if (bindingsModel.selectedIndex < 0 || bindingsModel.selectedIndex >= static_cast<int>(rows.size())) {
        return;
    }
    auto defs = ui::bindings::Definitions();
    if (bindingsModel.selectedIndex >= 0 && bindingsModel.selectedIndex < static_cast<int>(defs.size())) {
        if (defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].isHeader) {
            return;
        }
    }
    if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard) {
        bindingsModel.keyboard[bindingsModel.selectedIndex][0] = '\0';
    } else if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse) {
        bindingsModel.mouse[bindingsModel.selectedIndex][0] = '\0';
    } else {
        bindingsModel.controller[bindingsModel.selectedIndex][0] = '\0';
    }
    rebuildBindings();
}

void RmlUiPanelBindings::saveBindings() {
    const auto result = bindingsController.saveToConfig();
    showStatus(result.status, result.statusIsError);
    if (!result.ok) {
        return;
    }
    requestKeybindingsReload();
}

void RmlUiPanelBindings::resetBindings() {
    const auto result = bindingsController.resetToDefaults();
    requestKeybindingsReload();
    showStatus(result.status, result.statusIsError);

    rebuildBindings();
}

void RmlUiPanelBindings::showResetDialog() {
    resetDialog.show("Reset all keybindings to defaults? This will overwrite your custom bindings.");
}

void RmlUiPanelBindings::showStatus(const std::string &message, bool isError) {
    bindingsModel.statusText = message;
    bindingsModel.statusIsError = isError;
    updateStatus();
}

void RmlUiPanelBindings::requestKeybindingsReload() {
    bindingsModel.keybindingsReloadRequested = true;
}

void RmlUiPanelBindings::captureKey(int keyIdentifier) {
    if (bindingsModel.selectedIndex < 0 || bindingsModel.selectedIndex >= static_cast<int>(rows.size())) {
        return;
    }
    auto defs = ui::bindings::Definitions();
    if (bindingsModel.selectedIndex >= 0 && bindingsModel.selectedIndex < static_cast<int>(defs.size())) {
        if (defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].isHeader) {
            return;
        }
    }
    if (keyIdentifier == Rml::Input::KI_UNKNOWN) {
        return;
    }
    if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse && keyIdentifier == Rml::Input::KI_ESCAPE) {
        saveBindings();
        clearSelection();
        return;
    }
    if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard &&
        (keyIdentifier == Rml::Input::KI_ESCAPE || keyIdentifier == Rml::Input::KI_OEM_3)) {
        return;
    }
    if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse) {
        return;
    }
    const std::string name = keyIdentifierToName(keyIdentifier);
    if (name.empty()) {
        return;
    }
    if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard) {
        auto entries = ui::bindings::SplitBindings(bindingsModel.keyboard[bindingsModel.selectedIndex].data());
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            WriteBuffer(bindingsModel.keyboard[bindingsModel.selectedIndex], ui::bindings::JoinBindings(entries));
            rebuildBindings();
        }
    } else {
        auto entries = ui::bindings::SplitBindings(bindingsModel.controller[bindingsModel.selectedIndex].data());
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            WriteBuffer(bindingsModel.controller[bindingsModel.selectedIndex], ui::bindings::JoinBindings(entries));
            rebuildBindings();
        }
    }
}

void RmlUiPanelBindings::onShow() {
    bindingsModel.loaded = false;
}

void RmlUiPanelBindings::onHide() {
    clearSelection();
}

void RmlUiPanelBindings::onConfigChanged() {
    bindingsModel.loaded = false;
}

void RmlUiPanelBindings::handleMouseClick(Rml::Element *target, int button) {
    if (button != 0) {
        return;
    }
    if (bindingsModel.selectedIndex < 0 || bindingsModel.selectedIndex >= static_cast<int>(rows.size())) {
        return;
    }
    if (bindingsModel.selectedColumn != ui::BindingsModel::Column::Keyboard) {
        return;
    }
    if (target == clearButton || target == saveButton || target == resetButton) {
        return;
    }
    bool targetIsBindingCell = false;
    int targetIndex = -1;
    ui::BindingsModel::Column targetColumn = ui::BindingsModel::Column::Keyboard;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (target == rows[i].keyboard) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = ui::BindingsModel::Column::Keyboard;
            break;
        }
        if (target == rows[i].mouse) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = ui::BindingsModel::Column::Mouse;
            break;
        }
        if (target == rows[i].controller) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = ui::BindingsModel::Column::Controller;
            break;
        }
    }
    if (targetIsBindingCell && targetIndex == bindingsModel.selectedIndex && targetColumn == bindingsModel.selectedColumn) {
        return;
    }
    saveBindings();
    if (!targetIsBindingCell) {
        clearSelection();
    }
}

void RmlUiPanelBindings::captureMouse(int button) {
    if (selectionJustChanged) {
        return;
    }
    if (bindingsModel.selectedIndex < 0 || bindingsModel.selectedIndex >= static_cast<int>(rows.size())) {
        return;
    }
    auto defs = ui::bindings::Definitions();
    if (bindingsModel.selectedIndex >= 0 && bindingsModel.selectedIndex < static_cast<int>(defs.size())) {
        if (defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].isHeader) {
            return;
        }
    }
    if (bindingsModel.selectedColumn != ui::BindingsModel::Column::Mouse) {
        return;
    }
    std::string name;
    switch (button) {
        case 0: name = "LEFT_MOUSE"; break;
        case 1: name = "RIGHT_MOUSE"; break;
        case 2: name = "MIDDLE_MOUSE"; break;
        case 3: name = "MOUSE4"; break;
        case 4: name = "MOUSE5"; break;
        case 5: name = "MOUSE6"; break;
        case 6: name = "MOUSE7"; break;
        case 7: name = "MOUSE8"; break;
        default: break;
    }
    if (name.empty()) {
        return;
    }
    auto entries = ui::bindings::SplitBindings(bindingsModel.mouse[bindingsModel.selectedIndex].data());
    if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
        entries.push_back(name);
        WriteBuffer(bindingsModel.mouse[bindingsModel.selectedIndex], ui::bindings::JoinBindings(entries));
        rebuildBindings();
    }
}

std::string RmlUiPanelBindings::keyIdentifierToName(int keyIdentifier) const {
    using KeyInfo = struct { int key; const char *name; };
    static const KeyInfo keyMap[] = {
        {Rml::Input::KI_SPACE, "SPACE"},
        {Rml::Input::KI_RETURN, "ENTER"},
        {Rml::Input::KI_ESCAPE, "ESCAPE"},
        {Rml::Input::KI_TAB, "TAB"},
        {Rml::Input::KI_BACK, "BACKSPACE"},
        {Rml::Input::KI_LEFT, "LEFT"},
        {Rml::Input::KI_RIGHT, "RIGHT"},
        {Rml::Input::KI_UP, "UP"},
        {Rml::Input::KI_DOWN, "DOWN"},
        {Rml::Input::KI_OEM_4, "LEFT_BRACKET"},
        {Rml::Input::KI_OEM_6, "RIGHT_BRACKET"},
        {Rml::Input::KI_OEM_MINUS, "MINUS"},
        {Rml::Input::KI_OEM_PLUS, "EQUAL"},
        {Rml::Input::KI_OEM_7, "APOSTROPHE"},
        {Rml::Input::KI_OEM_3, "GRAVE_ACCENT"},
        {Rml::Input::KI_HOME, "HOME"},
        {Rml::Input::KI_END, "END"},
        {Rml::Input::KI_PRIOR, "PAGE_UP"},
        {Rml::Input::KI_NEXT, "PAGE_DOWN"},
        {Rml::Input::KI_INSERT, "INSERT"},
        {Rml::Input::KI_DELETE, "DELETE"},
        {Rml::Input::KI_CAPITAL, "CAPS_LOCK"},
        {Rml::Input::KI_NUMLOCK, "NUM_LOCK"},
        {Rml::Input::KI_SCROLL, "SCROLL_LOCK"},
        {Rml::Input::KI_LSHIFT, "LEFT_SHIFT"},
        {Rml::Input::KI_RSHIFT, "RIGHT_SHIFT"},
        {Rml::Input::KI_LCONTROL, "LEFT_CONTROL"},
        {Rml::Input::KI_RCONTROL, "RIGHT_CONTROL"},
        {Rml::Input::KI_LMENU, "LEFT_ALT"},
        {Rml::Input::KI_RMENU, "RIGHT_ALT"},
        {Rml::Input::KI_LMETA, "LEFT_SUPER"},
        {Rml::Input::KI_RMETA, "RIGHT_SUPER"},
        {Rml::Input::KI_UNKNOWN, "MENU"}
    };

    if (keyIdentifier >= Rml::Input::KI_A && keyIdentifier <= Rml::Input::KI_Z) {
        char name[2] = { static_cast<char>('A' + (keyIdentifier - Rml::Input::KI_A)), '\0' };
        return std::string(name);
    }
    if (keyIdentifier >= Rml::Input::KI_0 && keyIdentifier <= Rml::Input::KI_9) {
        char name[2] = { static_cast<char>('0' + (keyIdentifier - Rml::Input::KI_0)), '\0' };
        return std::string(name);
    }
    if (keyIdentifier >= Rml::Input::KI_F1 && keyIdentifier <= Rml::Input::KI_F12) {
        return "F" + std::to_string(1 + (keyIdentifier - Rml::Input::KI_F1));
    }
    for (const auto &entry : keyMap) {
        if (entry.key == keyIdentifier) {
            return std::string(entry.name);
        }
    }
    return {};
}

} // namespace ui

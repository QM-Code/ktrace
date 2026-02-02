#include "ui/frontends/imgui/console/console.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ui/console/status_banner.hpp"
#include "ui/console/keybindings.hpp"

namespace {

void WriteBuffer(std::array<char, 128> &buffer, const std::string &value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

void AppendBinding(std::array<char, 128> &buffer, const std::string &value) {
    std::vector<std::string> existing = ui::bindings::SplitBindings(buffer.data());
    if (std::find(existing.begin(), existing.end(), value) == existing.end()) {
        existing.push_back(value);
        WriteBuffer(buffer, ui::bindings::JoinBindings(existing));
    }
}

std::optional<std::string> DetectKeyboardBinding() {
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
        const ImGuiKey keyValue = static_cast<ImGuiKey>(key);
#ifdef ImGuiKey_GraveAccent
        if (keyValue == ImGuiKey_Escape || keyValue == ImGuiKey_GraveAccent) {
            continue;
        }
#else
        if (keyValue == ImGuiKey_Escape) {
            continue;
        }
#endif
#ifdef ImGuiKey_MouseLeft
        if (keyValue >= ImGuiKey_MouseLeft && keyValue <= ImGuiKey_MouseWheelY) {
            continue;
        }
#endif
#ifdef ImGuiKey_GamepadStart
        if (keyValue >= ImGuiKey_GamepadStart && keyValue <= ImGuiKey_GamepadR3) {
            continue;
        }
#endif
        if (ImGui::IsKeyPressed(keyValue, false)) {
            const char *name = ImGui::GetKeyName(keyValue);
            if (name && *name) {
                return std::string(name);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> DetectMouseBinding(bool skipCapture) {
    if (skipCapture) {
        return std::nullopt;
    }
    const std::array<std::pair<int, const char *>, 8> mouseButtons = {{
        {ImGuiMouseButton_Left, "LEFT_MOUSE"},
        {ImGuiMouseButton_Right, "RIGHT_MOUSE"},
        {ImGuiMouseButton_Middle, "MIDDLE_MOUSE"},
        {3, "MOUSE4"},
        {4, "MOUSE5"},
        {5, "MOUSE6"},
        {6, "MOUSE7"},
        {7, "MOUSE8"}
    }};
    for (const auto &entry : mouseButtons) {
        if (ImGui::IsMouseClicked(entry.first)) {
            return std::string(entry.second);
        }
    }
    return std::nullopt;
}

std::optional<std::string> DetectControllerBinding() {
#ifdef ImGuiKey_GamepadStart
    const struct { ImGuiKey key; const char *name; } gamepadKeys[] = {
        {ImGuiKey_GamepadStart, "GAMEPAD_START"},
        {ImGuiKey_GamepadBack, "GAMEPAD_BACK"},
        {ImGuiKey_GamepadFaceDown, "GAMEPAD_A"},
        {ImGuiKey_GamepadFaceRight, "GAMEPAD_B"},
        {ImGuiKey_GamepadFaceLeft, "GAMEPAD_X"},
        {ImGuiKey_GamepadFaceUp, "GAMEPAD_Y"},
        {ImGuiKey_GamepadDpadLeft, "GAMEPAD_DPAD_LEFT"},
        {ImGuiKey_GamepadDpadRight, "GAMEPAD_DPAD_RIGHT"},
        {ImGuiKey_GamepadDpadUp, "GAMEPAD_DPAD_UP"},
        {ImGuiKey_GamepadDpadDown, "GAMEPAD_DPAD_DOWN"},
        {ImGuiKey_GamepadL1, "GAMEPAD_LB"},
        {ImGuiKey_GamepadR1, "GAMEPAD_RB"},
        {ImGuiKey_GamepadL2, "GAMEPAD_LT"},
        {ImGuiKey_GamepadR2, "GAMEPAD_RT"},
        {ImGuiKey_GamepadL3, "GAMEPAD_LS"},
        {ImGuiKey_GamepadR3, "GAMEPAD_RS"}
    };

    for (const auto &entry : gamepadKeys) {
        if (ImGui::IsKeyPressed(entry.key)) {
            return std::string(entry.name);
        }
    }
#endif
    return std::nullopt;
}

} // namespace

namespace ui {

void ConsoleView::drawBindingsPanel(const MessageColors &colors) {
    const auto defs = ui::bindings::Definitions();
    auto resetBindings = [&]() {
        const auto result = bindingsController.resetToDefaults();
        requestKeybindingsReload();
        bindingsModel.statusText = result.status;
        bindingsModel.statusIsError = result.statusIsError;
    };
    auto saveBindings = [&]() -> bool {
        const auto result = bindingsController.saveToConfig();
        bindingsModel.statusText = result.status;
        bindingsModel.statusIsError = result.statusIsError;
        if (!result.ok) {
            return false;
        }
        requestKeybindingsReload();
        return true;
    };

    if (!bindingsModel.loaded) {
        bindingsModel.loaded = true;
        bindingsModel.statusText.clear();
        bindingsModel.statusIsError = false;
        bindingsModel.selectedIndex = -1;

        const auto result = bindingsController.loadFromConfig();
        if (!result.status.empty()) {
            bindingsModel.statusText = result.status;
            bindingsModel.statusIsError = result.statusIsError;
        }
    }

    ImGui::TextDisabled("Select a cell, then press a key/button to add it. Changes apply on next launch.");
    ImGui::Spacing();

    const int previousBindingIndex = bindingsModel.selectedIndex;
    const auto previousBindingColumn = bindingsModel.selectedColumn;
    bool selectionChanged = false;
    bool selectedCellHovered = false;
    bool anyBindingCellHovered = false;
    if (ImGui::BeginTable("KeybindingsTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Mouse");
        ImGui::TableSetupColumn("Controller");
        ImGui::TableHeadersRow();
        const std::size_t count = std::min(defs.size(), ui::BindingsModel::kKeybindingCount);
        for (std::size_t i = 0; i < count; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (defs[i].isHeader) {
                ImGui::TextDisabled("%s", defs[i].label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted("");
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted("");
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted("");
                continue;
            }
            ImGui::TextUnformatted(defs[i].label);

            auto drawBindingCell = [&](ui::BindingsModel::Column column,
                                       std::array<char, 128> &buffer,
                                       const char *columnId) {
                const bool isSelected = (bindingsModel.selectedIndex == static_cast<int>(i) &&
                                         bindingsModel.selectedColumn == column);
                std::string display = buffer.data();
                if (display.empty()) {
                    display = "Unbound";
                }
                std::string label = display + "##Bind_" + defs[i].action + "_" + columnId;
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    bindingsModel.selectedIndex = static_cast<int>(i);
                    bindingsModel.selectedColumn = column;
                    selectionChanged = true;
                }
                if (isSelected && ImGui::IsItemHovered()) {
                    selectedCellHovered = true;
                }
                if (ImGui::IsItemHovered()) {
                    anyBindingCellHovered = true;
                }
            };

            ImGui::TableSetColumnIndex(1);
            drawBindingCell(ui::BindingsModel::Column::Keyboard, bindingsModel.keyboard[i], "Keyboard");
            ImGui::TableSetColumnIndex(2);
            drawBindingCell(ui::BindingsModel::Column::Mouse, bindingsModel.mouse[i], "Mouse");
            ImGui::TableSetColumnIndex(3);
            drawBindingCell(ui::BindingsModel::Column::Controller, bindingsModel.controller[i], "Controller");
        }
        ImGui::EndTable();
    }

    if (previousBindingIndex >= 0 && previousBindingColumn == ui::BindingsModel::Column::Keyboard) {
        if (selectionChanged) {
            saveBindings();
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectedCellHovered) {
            if (anyBindingCellHovered || !ImGui::IsAnyItemHovered()) {
                saveBindings();
                if (!anyBindingCellHovered) {
                    bindingsModel.selectedIndex = -1;
                }
            }
        }
    }

    ImGui::Spacing();
    const char *selectedLabel = "None";
    const char *selectedColumn = "None";
    if (bindingsModel.selectedIndex >= 0 && bindingsModel.selectedIndex < static_cast<int>(defs.size())) {
        const auto &def = defs[static_cast<std::size_t>(bindingsModel.selectedIndex)];
        if (!def.isHeader) {
            selectedLabel = def.label;
            selectedColumn = (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard)
                ? "Keyboard"
                : (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse ? "Mouse" : "Controller");
        }
    }
    ImGui::TextDisabled("Selected cell: %s / %s", selectedLabel, selectedColumn);

    if (bindingsModel.selectedIndex >= 0) {
        if (bindingsModel.selectedIndex >= static_cast<int>(defs.size()) ||
            defs[static_cast<std::size_t>(bindingsModel.selectedIndex)].isHeader) {
            bindingsModel.selectedIndex = -1;
        }
    }

    if (bindingsModel.selectedIndex >= 0) {
        const bool skipMouseCapture = selectionChanged || ImGui::IsAnyItemActive();
        std::optional<std::string> captured;

        if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard) {
            captured = DetectKeyboardBinding();
            if (captured) {
                AppendBinding(bindingsModel.keyboard[bindingsModel.selectedIndex], *captured);
            }
        } else if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                saveBindings();
                bindingsModel.selectedIndex = -1;
            } else {
                captured = DetectMouseBinding(skipMouseCapture);
                if (captured) {
                    AppendBinding(bindingsModel.mouse[bindingsModel.selectedIndex], *captured);
                }
            }
        } else {
            captured = DetectControllerBinding();
            if (captured) {
                AppendBinding(bindingsModel.controller[bindingsModel.selectedIndex], *captured);
            }
        }
    }

    ImGui::Spacing();
    bool saveClicked = ImGui::Button("Save Bindings");
    ImGui::SameLine();
    bool resetClicked = ImGui::Button("Reset to Defaults");
    ImGui::SameLine();
    if (ImGui::Button("Clear Selected")) {
        if (bindingsModel.selectedIndex >= 0) {
            if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Keyboard) {
                bindingsModel.keyboard[bindingsModel.selectedIndex][0] = '\0';
            } else if (bindingsModel.selectedColumn == ui::BindingsModel::Column::Mouse) {
                bindingsModel.mouse[bindingsModel.selectedIndex][0] = '\0';
            } else {
                bindingsModel.controller[bindingsModel.selectedIndex][0] = '\0';
            }
        }
    }

    if (saveClicked) {
        saveBindings();
    }

    if (resetClicked) {
        ImGui::OpenPopup("Reset Bindings?");
        bindingsResetConfirmOpen = true;
    }

    if (bindingsResetConfirmOpen && ImGui::BeginPopupModal("Reset Bindings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Reset all keybindings to defaults? This will overwrite your custom bindings.");
        ImGui::Spacing();
        if (ImGui::Button("Reset")) {
            resetBindings();
            bindingsResetConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            bindingsResetConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    const auto banner = ui::status_banner::MakeStatusBanner(bindingsModel.statusText,
                                                            bindingsModel.statusIsError);
    if (banner.visible) {
        ImGui::Spacing();
        ImVec4 statusColor = colors.notice;
        if (banner.tone == ui::MessageTone::Error) {
            statusColor = colors.error;
        } else if (banner.tone == ui::MessageTone::Pending) {
            statusColor = colors.pending;
        }
        const std::string text = ui::status_banner::FormatStatusText(banner);
        ImGui::TextColored(statusColor, "%s", text.c_str());
        ImGui::Spacing();
    }
}

} // namespace ui

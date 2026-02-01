#include "ui/frontends/imgui/console/console.hpp"
#include "karma_extras/ui/imgui/texture_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace {
std::string toSmallCaps(const std::string &value) {
    std::string out = value;
    for (char &ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string normalizedCommunityUrl(const std::string &host) {
    if (host.empty()) {
        return {};
    }
    if (host.rfind("http://", 0) == 0 || host.rfind("https://", 0) == 0) {
        return host;
    }
    return "http://" + host;
}

std::string urlEncode(const std::string &value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex << std::setfill('0');
    for (unsigned char ch : value) {
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else if (ch == ' ') {
            encoded << "%20";
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

std::string buildServerPageUrl(const std::string &communityHost, const std::string &serverCode) {
    if (communityHost.empty() || serverCode.empty()) {
        return {};
    }
    std::string base = normalizedCommunityUrl(communityHost);
    if (base.empty()) {
        return {};
    }
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + "/servers/" + urlEncode(serverCode);
}

std::string sanitizeTextForImGui(const std::string &text) {
#ifdef IMGUI_USE_WCHAR32
    return text;
#else
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }

        std::size_t len = 0;
        uint32_t codepoint = 0;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
            codepoint = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
            codepoint = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
            codepoint = c & 0x07;
        } else {
            ++i;
            continue;
        }

        if (i + len > text.size()) {
            break;
        }

        bool valid = true;
        for (std::size_t j = 1; j < len; ++j) {
            unsigned char cc = static_cast<unsigned char>(text[i + j]);
            if ((cc & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cc & 0x3F);
        }

        if (!valid) {
            ++i;
            continue;
        }

        if (codepoint <= 0xFFFF) {
            out.append(text.substr(i, len));
        }

        i += len;
    }
    return out;
#endif
}

std::string makeServerDetailsKey(const ui::CommunityBrowserEntry &entry) {
    if (entry.sourceHost.empty()) {
        return {};
    }
    if (!entry.code.empty()) {
        return entry.sourceHost + "|" + entry.code;
    }
    std::string name = entry.worldName;
    if (name.empty()) {
        name = entry.label.empty() ? entry.host : entry.label;
    }
    if (name.empty()) {
        return {};
    }
    return entry.sourceHost + "|" + name + "|" + entry.host + ":" + std::to_string(entry.port);
}

bool openUrlInBrowser(const std::string &url) {
    if (url.empty()) {
        return false;
    }
#if defined(_WIN32)
    HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
#elif defined(__APPLE__)
    std::string command = "open \"" + url + "\"";
    int result = std::system(command.c_str());
    return result == 0;
#else
    std::string command = "xdg-open \"" + url + "\"";
    int result = std::system(command.c_str());
    return result == 0;
#endif
}

void renderInlineTextWithLinks(const std::string &text,
                               const ImVec4 &linkColor,
                               std::string &linkStatusText,
                               bool &linkStatusIsError) {
    std::size_t pos = 0;
    bool first = true;

    auto emitText = [&](const std::string &segment) {
        if (segment.empty()) {
            return;
        }
        if (!first) {
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImGui::TextUnformatted(segment.c_str());
        first = false;
    };

    auto emitLink = [&](const std::string &label, const std::string &url) {
        if (label.empty()) {
            return;
        }
        if (!first) {
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Open in browser");
        }
        if (ImGui::IsItemClicked()) {
            if (!openUrlInBrowser(url)) {
                linkStatusText = "Failed to open your browser.";
                linkStatusIsError = true;
            } else {
                linkStatusText.clear();
                linkStatusIsError = false;
            }
        }
        first = false;
    };

    while (pos < text.size()) {
        std::size_t linkStart = text.find('[', pos);
        if (linkStart == std::string::npos) {
            emitText(text.substr(pos));
            break;
        }
        std::size_t linkEnd = text.find(']', linkStart + 1);
        if (linkEnd == std::string::npos) {
            emitText(text.substr(pos));
            break;
        }
        if (linkEnd + 1 >= text.size() || text[linkEnd + 1] != '(') {
            emitText(text.substr(pos, linkEnd - pos + 1));
            pos = linkEnd + 1;
            continue;
        }
        std::size_t urlEnd = text.find(')', linkEnd + 2);
        if (urlEnd == std::string::npos) {
            emitText(text.substr(pos));
            break;
        }

        emitText(text.substr(pos, linkStart - pos));
        std::string label = text.substr(linkStart + 1, linkEnd - linkStart - 1);
        std::string url = text.substr(linkEnd + 2, urlEnd - linkEnd - 2);
        emitLink(label, url);

        pos = urlEnd + 1;
    }
}

void renderMarkdown(const std::string &text,
                    ImFont *titleFont,
                    ImFont *headingFont,
                    const ImVec4 &linkColor,
                    std::string &linkStatusText,
                    bool &linkStatusIsError) {
    const std::string safeText = sanitizeTextForImGui(text);
    std::istringstream input(safeText);
    std::string line;
    bool firstLine = true;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!firstLine) {
            ImGui::Spacing();
        }
        firstLine = false;

        if (line.empty()) {
            ImGui::Spacing();
            continue;
        }

        std::size_t hashCount = 0;
        while (hashCount < line.size() && line[hashCount] == '#') {
            ++hashCount;
        }
        if (hashCount > 0 && hashCount < line.size() && line[hashCount] == ' ') {
            std::string headingText = line.substr(hashCount + 1);
            ImFont *font = (hashCount <= 2 && titleFont) ? titleFont : headingFont;
            if (font) {
                ImGui::PushFont(font);
            }
            ImGui::TextWrapped("%s", headingText.c_str());
            if (font) {
                ImGui::PopFont();
            }
            continue;
        }

        if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0 || line.rfind("+ ", 0) == 0) {
            ImGui::Bullet();
            ImGui::SameLine();
            renderInlineTextWithLinks(
                line.substr(2),
                linkColor,
                linkStatusText,
                linkStatusIsError);
            continue;
        }

        renderInlineTextWithLinks(line, linkColor, linkStatusText, linkStatusIsError);
    }
}

std::string normalizedHost(const std::string &host) {
    if (host.empty()) {
        return {};
    }
    std::string trimmed = host;
    while (!trimmed.empty() && trimmed.back() == '/') {
        trimmed.pop_back();
    }
    return trimmed;
}
}

namespace ui {

void ConsoleView::drawCommunityPanel(const MessageColors &messageColors) {
    const bool hasHeadingFont = (headingFont != nullptr);
    const bool hasButtonFont = (buttonFont != nullptr);
    bool joinRequested = false;
    bool roamRequested = false;
    const bool connected = consoleModel.connectionState.connected;
    auto &community = consoleModel.community;
    auto &entries = community.entries;
    auto &selectedIndex = community.selectedIndex;
    auto &listOptions = community.listOptions;
    auto &listSelectedIndex = community.listSelectedIndex;
    auto &statusText = community.statusText;
    auto &statusIsError = community.statusIsError;
    auto &listStatusText = community.listStatusText;
    auto &listStatusIsError = community.listStatusIsError;
    auto &communityStatusText = community.communityStatusText;
    auto &communityStatusTone = community.statusTone;
    auto &communityDetailsText = community.detailsText;
    auto &communityLinkStatusText = community.communityLinkStatusText;
    auto &communityLinkStatusIsError = community.communityLinkStatusIsError;
    auto &serverLinkStatusText = community.serverLinkStatusText;
    auto &serverLinkStatusIsError = community.serverLinkStatusIsError;
    auto &serverDescriptionLoadingKey = community.serverDescriptionLoadingKey;
    auto &serverDescriptionLoading = community.serverDescriptionLoading;
    auto &serverDescriptionErrorKey = community.serverDescriptionErrorKey;
    auto &serverDescriptionErrorText = community.serverDescriptionErrorText;

    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const ImGuiStyle &style = ImGui::GetStyle();
    const float minDetailWidth = 300.0f;
    const float minListWidth = 280.0f;
    float maxListWidth = std::max(minListWidth, contentAvail.x - minDetailWidth - style.ItemSpacing.x);
    float listPanelWidth = std::max(320.0f, contentAvail.x * 0.5f);
    listPanelWidth = std::clamp(listPanelWidth, minListWidth, maxListWidth);

    ImGui::BeginChild("CommunityBrowserListPane", ImVec2(listPanelWidth, 0), false);

    auto formatListLabel = [](const ServerListOption &option) {
        if (!option.name.empty()) {
            return option.name;
        }
        if (!option.host.empty()) {
            return option.host;
        }
        return std::string("Unnamed list");
    };

    ImGui::Spacing();

    if (!listOptions.empty()) {
        listSelectedIndex = std::clamp(
            listSelectedIndex,
            0,
            static_cast<int>(listOptions.size()) - 1);
    } else {
        listSelectedIndex = -1;
    }
    const bool hasActiveServers = !entries.empty();
    const auto &connectionState = consoleModel.connectionState;
    std::vector<CommunityBrowserEntry> connectedEntries;
    const std::vector<CommunityBrowserEntry> *displayEntries = &entries;
    int displaySelectedIndex = selectedIndex;
    bool useConnectedEntry = false;
    if (entries.empty() && connected && !connectionState.host.empty()) {
        ui::CommunityBrowserEntry entry;
        entry.label = connectionState.host + ":" + std::to_string(connectionState.port);
        entry.host = connectionState.host;
        entry.port = connectionState.port;
        entry.description = "Connected server";
        entry.displayHost = connectionState.host;
        entry.longDescription = entry.description;
        entry.activePlayers = -1;
        entry.maxPlayers = -1;
        connectedEntries.push_back(std::move(entry));
        displayEntries = &connectedEntries;
        displaySelectedIndex = 0;
        useConnectedEntry = true;
    }

    std::string comboLabel = "No communities";
    if (listSelectedIndex >= 0 && listSelectedIndex < static_cast<int>(listOptions.size())) {
        comboLabel = formatListLabel(listOptions[listSelectedIndex]);
    }

    if (ImGui::BeginTable("##CommunitySelectorRow", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##CommunityLabel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##CommunityCombo", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##CommunityInfo", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Community");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##ServerListSelector", comboLabel.c_str())) {
            for (int i = 0; i < static_cast<int>(listOptions.size()); ++i) {
                const auto &option = listOptions[i];
                std::string optionLabel = formatListLabel(option);
                bool selected = (i == listSelectedIndex);
                if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                    if (!selected) {
                        listSelectedIndex = i;
                        consoleController.queueListSelection(i);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableSetColumnIndex(2);
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (ImGui::Button("?")) {
            selectedIndex = -1;
        }
        ImGui::PopStyleColor();
        if (hasButtonFont) {
            ImGui::PopFont();
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    refreshCommunityCredentials();

    bool usernameChanged = false;
    bool passwordChanged = false;

    const bool isLanCommunity =
        listSelectedIndex >= 0 &&
        listSelectedIndex < static_cast<int>(listOptions.size()) &&
        listOptions[listSelectedIndex].name == "Local Area Network";

    const float joinInlineWidth = ImGui::CalcTextSize("Join").x + style.FramePadding.x * 2.0f;
    const float roamInlineWidth = ImGui::CalcTextSize("Roam").x + style.FramePadding.x * 2.0f;
    const float quitInlineWidth = ImGui::CalcTextSize("Quit").x + style.FramePadding.x * 2.0f;
    const float joinButtonsWidth = joinInlineWidth + roamInlineWidth + style.ItemSpacing.x;
    const float labelSpacing = style.ItemSpacing.x * 2.0f;
    if (ImGui::BeginTable("##CommunityCredentialsRow", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##UserLabel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##UserInput", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##PassLabel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##PassInput", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Username");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        const bool usernameEdited = ImGui::InputText(
            "##Username",
            usernameBuffer.data(),
            usernameBuffer.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (!connected) {
            joinRequested |= usernameEdited;
        }
        usernameChanged |= usernameEdited;
        if (usernameEdited) {
            storedPasswordHash.clear();
            passwordChanged = true;
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Password");

        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        const char *passwordHint = storedPasswordHash.empty() ? "" : "stored";
        const bool passwordEdited = ImGui::InputTextWithHint(
            "##Password",
            passwordHint,
            passwordBuffer.data(),
            passwordBuffer.size(),
            ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
        if (!connected) {
            joinRequested |= passwordEdited;
        }
        if (passwordEdited) {
            storedPasswordHash.clear();
            passwordChanged = true;
        }

        ImGui::EndTable();
    }

    if (usernameChanged || passwordChanged) {
        persistCommunityCredentials(passwordChanged);
    }

    if (!statusText.empty()) {
        ImGui::Spacing();
        ImVec4 color = statusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(color, "%s", statusText.c_str());
    }

    if (!errorDialogMessage.empty()) {
        ImGui::OpenPopup("Community Error");
    }
    if (ImGui::BeginPopupModal("Community Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", errorDialogMessage.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Ok")) {
            errorDialogMessage.clear();
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            errorDialogMessage.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float refreshButtonWidth = ImGui::CalcTextSize("Refresh").x + style.FramePadding.x * 2.0f;

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY;

    float footerHeight = ImGui::GetFrameHeight();
    footerHeight += ImGui::GetTextLineHeightWithSpacing();
    footerHeight += style.ItemSpacing.y * 2.0f;
    if (!listStatusText.empty()) {
        footerHeight += ImGui::GetTextLineHeightWithSpacing();
    }

    float tableHeight = ImGui::GetContentRegionAvail().y - footerHeight;
    if (tableHeight < 0.0f) {
        tableHeight = 0.0f;
    }
    const float playerColumnWidth = 120.0f;

    if (ImGui::BeginTable("##CommunityBrowserPresets", 2, tableFlags, ImVec2(-1.0f, tableHeight))) {
        ImGui::TableSetupColumn("##ServerListColumn", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("##PlayerCountColumn", ImGuiTableColumnFlags_WidthFixed, playerColumnWidth);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

        const char *serversHeadingLabel = "Servers";

        ImGui::TableSetColumnIndex(0);
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted(serversHeadingLabel);
        ImGui::PopStyleColor();
        if (hasHeadingFont) {
            ImGui::PopFont();
        }

        ImGui::TableSetColumnIndex(1);
        const float headerStartX = ImGui::GetCursorPosX();
        const float headerStartY = ImGui::GetCursorPosY();
        const float headerColumnWidth = ImGui::GetColumnWidth();
        float buttonX = headerStartX + headerColumnWidth - refreshButtonWidth;
        float lineBottom = ImGui::GetCursorPosY();

        ImGui::SetCursorPos(ImVec2(buttonX, headerStartY));
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (ImGui::Button("Refresh")) {
            consoleController.requestRefresh();
        }
        ImGui::PopStyleColor();
        if (hasButtonFont) {
            ImGui::PopFont();
        }
        lineBottom = std::max(lineBottom, ImGui::GetCursorPosY());
        ImGui::SetCursorPosY(lineBottom);

        if (displayEntries->empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (!communityStatusText.empty()) {
                ImVec4 statusColor = messageColors.notice;
                if (communityStatusTone == MessageTone::Error) {
                    statusColor = messageColors.error;
                } else if (communityStatusTone == MessageTone::Pending) {
                    statusColor = messageColors.pending;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::TextWrapped("%s", communityStatusText.c_str());
                ImGui::PopStyleColor();
            } else if (!listStatusText.empty()) {
                ImVec4 listColor = listStatusIsError ? messageColors.error : messageColors.action;
                ImGui::PushStyleColor(ImGuiCol_Text, listColor);
                ImGui::TextWrapped("%s", listStatusText.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("No servers available.");
            }
        } else {
            for (int i = 0; i < static_cast<int>(displayEntries->size()); ++i) {
                const auto &entry = (*displayEntries)[i];
                bool selected = (i == displaySelectedIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                std::string label = entry.label;
                if (label.empty()) {
                    label = entry.host;
                }

                if (ImGui::Selectable(label.c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (!useConnectedEntry) {
                        selectedIndex = i;
                    }
                    if (!connected && ImGui::IsMouseDoubleClicked(0)) {
                        consoleController.queueSelection(CommunityBrowserSelection{
                            entry.host,
                            entry.port,
                            false,
                            entry.sourceHost,
                            entry.worldName,
                            false
                        });
                    }
                }
                ImGui::TableSetColumnIndex(1);
                if (entry.activePlayers >= 0) {
                    if (entry.maxPlayers >= 0) {
                        std::string activeText = std::to_string(entry.activePlayers);
                        std::string maxText = std::to_string(entry.maxPlayers);
                        ImVec2 activeSize = ImGui::CalcTextSize(activeText.c_str());
                        ImVec2 maxSize = ImGui::CalcTextSize(maxText.c_str());
                        float totalWidth = activeSize.x + maxSize.x + ImGui::CalcTextSize(" / ").x;
                        float columnWidth = ImGui::GetColumnWidth();
                        float startX = ImGui::GetCursorPosX() + std::max(0.0f, columnWidth - totalWidth);
                        ImGui::SetCursorPosX(startX);
                        ImGui::TextUnformatted(activeText.c_str());
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(" / ");
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(maxText.c_str());
                    } else {
                        ImGui::Text("%d", entry.activePlayers);
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    bool saveListClicked = false;
    if (ImGui::BeginTable("##CommunityAddRow", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##AddLabel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##AddInput", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##AddButton", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("New Community");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint(
            "##CommunityHostInput",
            "http://host[:port]",
            listUrlBuffer.data(),
            listUrlBuffer.size());

        ImGui::TableSetColumnIndex(2);
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (ImGui::Button("Add")) {
            saveListClicked = true;
        }
        ImGui::PopStyleColor();
        if (hasButtonFont) {
            ImGui::PopFont();
        }

        ImGui::EndTable();
    }
    if (saveListClicked) {
        std::string urlValue(listUrlBuffer.data());
        if (urlValue.empty()) {
            listStatusText = "Enter a host before saving.";
            listStatusIsError = true;
        } else {
            listStatusText.clear();
            listStatusIsError = false;
            consoleController.queueNewListRequest(ServerListOption{std::string{}, urlValue});
            listUrlBuffer.fill('\0');
        }
    }
    if (!listStatusText.empty()) {
        ImGui::Spacing();
        ImVec4 listColor = listStatusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(listColor, "%s", listStatusText.c_str());
    }

    ImGui::EndChild();

    ImGui::SameLine();

    const CommunityBrowserEntry *selectedEntry = nullptr;
    if (useConnectedEntry && !connectedEntries.empty()) {
        selectedEntry = &connectedEntries[0];
    } else if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
        selectedEntry = &entries[selectedIndex];
    }

    std::string activeCommunityHost;
    std::string activeCommunityLabel;
    if (listSelectedIndex >= 0 && listSelectedIndex < static_cast<int>(listOptions.size())) {
        activeCommunityHost = listOptions[listSelectedIndex].host;
        activeCommunityLabel = formatListLabel(listOptions[listSelectedIndex]);
    }
    const float baseScale = ImGui::GetIO().FontGlobalScale;
    const float smallcapsScale = baseScale * 0.6f;

    ImGui::BeginChild("CommunityBrowserDetailsPane", ImVec2(0, 0), true);
    const bool showDelete = (!selectedEntry && !isLanCommunity && !activeCommunityHost.empty());
    float headerButtonsWidth = (connected && selectedEntry) ? quitInlineWidth : (selectedEntry ? joinButtonsWidth : 0.0f);
    if (showDelete) {
        headerButtonsWidth = headerButtonsWidth > 0.0f
            ? headerButtonsWidth + style.ItemSpacing.x + ImGui::CalcTextSize("Delete").x + style.FramePadding.x * 2.0f
            : ImGui::CalcTextSize("Delete").x + style.FramePadding.x * 2.0f;
    }
    if (ImGui::BeginTable("##CommunityDetailsHeader", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##Title", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted(selectedEntry ? "Server Details" : "Community Details");
        ImGui::PopStyleColor();
        if (hasHeadingFont) {
            ImGui::PopFont();
        }

        ImGui::TableSetColumnIndex(1);
        if (headerButtonsWidth > 0.0f) {
            const float columnWidth = ImGui::GetColumnWidth();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, columnWidth - headerButtonsWidth));
        }
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (connected && selectedEntry) {
            if (ImGui::Button("Quit")) {
                pendingQuitRequest = true;
            }
        } else if (selectedEntry) {
            ImGui::BeginDisabled(!hasActiveServers);
            if (ImGui::Button("Join")) {
                joinRequested = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!hasActiveServers);
            if (ImGui::Button("Roam")) {
                roamRequested = true;
            }
            ImGui::EndDisabled();
        }
        if (showDelete) {
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                ImGui::OpenPopup("Delete Community?");
            }
        }
        ImGui::PopStyleColor();
        if (hasButtonFont) {
            ImGui::PopFont();
        }

        ImGui::EndTable();
    }

    if (!hasActiveServers) {
        joinRequested = false;
        roamRequested = false;
    }
    if (connected) {
        joinRequested = false;
        roamRequested = false;
    }

    if (ImGui::IsPopupOpen("Delete Community?")) {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        const float targetWidth = std::min(viewport->Size.x * 0.45f, 1000.0f);
        ImGui::SetNextWindowSize(ImVec2(targetWidth, 0.0f));
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("Delete Community?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string displayName = activeCommunityLabel.empty() ? activeCommunityHost : activeCommunityLabel;
        ImGui::TextWrapped("Delete community \"%s\" from the list?", displayName.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool confirmDelete = false;
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (ImGui::Button("Delete")) {
            confirmDelete = true;
        }
        ImGui::PopStyleColor();
        if (hasButtonFont) {
            ImGui::PopFont();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        if (confirmDelete && !activeCommunityHost.empty()) {
            consoleController.queueDeleteListRequest(activeCommunityHost);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (joinRequested || roamRequested) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            const auto &entry = entries[selectedIndex];
            consoleController.queueSelection(CommunityBrowserSelection{
                entry.host,
                entry.port,
                true,
                entry.sourceHost,
                entry.worldName,
                roamRequested
            });
            statusText.clear();
            statusIsError = false;
        } else {
            statusText = "Choose a server from the list first.";
            statusIsError = true;
        }
    }

    if (!selectedEntry) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (isLanCommunity) {
            ImGui::TextWrapped(
                "Local Area Network (LAN) shows servers running on your local network. "
                "If you want to play with friends nearby, start a server from the Start Server panel "
                "and it will appear here for everyone on the same LAN.");
        } else if (!activeCommunityHost.empty()) {
            const std::string displayName = activeCommunityLabel.empty()
                ? std::string("Community")
                : activeCommunityLabel;
            const std::string website = normalizedCommunityUrl(activeCommunityHost);

            if (hasHeadingFont) {
                ImGui::PushFont(headingFont);
            }
            ImGui::SetWindowFontScale(smallcapsScale);
            ImGui::TextUnformatted(toSmallCaps("Community Name").c_str());
            ImGui::SetWindowFontScale(baseScale);
            if (hasHeadingFont) {
                ImGui::PopFont();
            }
            if (titleFont) {
                ImGui::PushFont(titleFont);
            }
            ImGui::TextWrapped("%s", displayName.c_str());
            if (titleFont) {
                ImGui::PopFont();
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            if (hasHeadingFont) {
                ImGui::PushFont(headingFont);
            }
            ImGui::SetWindowFontScale(smallcapsScale);
            ImGui::TextUnformatted(toSmallCaps("Website").c_str());
            ImGui::SetWindowFontScale(baseScale);
            if (hasHeadingFont) {
                ImGui::PopFont();
            }
            if (titleFont) {
                ImGui::PushFont(titleFont);
            }
            if (!website.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, messageColors.action);
                ImGui::TextUnformatted(website.c_str());
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Open in browser");
                }
                if (ImGui::IsItemClicked()) {
                    if (!openUrlInBrowser(website)) {
                        communityLinkStatusText = "Failed to open your browser.";
                        communityLinkStatusIsError = true;
                    } else {
                        communityLinkStatusText.clear();
                        communityLinkStatusIsError = false;
                    }
                }
            } else {
                ImGui::TextDisabled("No website available.");
            }
            if (titleFont) {
                ImGui::PopFont();
            }

            if (!communityLinkStatusText.empty()) {
                ImGui::Spacing();
                ImVec4 linkColor = communityLinkStatusIsError ? messageColors.error : messageColors.action;
                ImGui::TextColored(linkColor, "%s", communityLinkStatusText.c_str());
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            if (hasHeadingFont) {
                ImGui::PushFont(headingFont);
            }
            ImGui::SetWindowFontScale(smallcapsScale);
            ImGui::TextUnformatted(toSmallCaps("Description").c_str());
            ImGui::SetWindowFontScale(baseScale);
            if (hasHeadingFont) {
                ImGui::PopFont();
            }
            if (!communityDetailsText.empty()) {
                renderMarkdown(
                    communityDetailsText,
                    titleFont,
                    headingFont,
                    messageColors.action,
                    communityLinkStatusText,
                    communityLinkStatusIsError);
            } else {
                ImGui::TextDisabled("No description provided.");
            }
        } else {
            ImGui::TextDisabled("No community details available.");
        }
    } else {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

        const std::string serverName = selectedEntry->worldName.empty()
            ? (selectedEntry->label.empty() ? std::string("Server") : selectedEntry->label)
            : selectedEntry->worldName;
        const std::string serverCommunityHost = selectedEntry->sourceHost.empty()
            ? activeCommunityHost
            : selectedEntry->sourceHost;
        const std::string serverPageUrl = buildServerPageUrl(serverCommunityHost, selectedEntry->code);

        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::SetWindowFontScale(smallcapsScale);
        ImGui::TextUnformatted(toSmallCaps("Server").c_str());
        ImGui::SetWindowFontScale(baseScale);
        if (hasHeadingFont) {
            ImGui::PopFont();
        }
        if (titleFont) {
            ImGui::PushFont(titleFont);
        }
        ImGui::TextWrapped("%s", serverName.c_str());
        if (titleFont) {
            ImGui::PopFont();
        }

        ImGui::Spacing();
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::SetWindowFontScale(smallcapsScale);
        ImGui::TextUnformatted(toSmallCaps("Website").c_str());
        ImGui::SetWindowFontScale(baseScale);
        if (hasHeadingFont) {
            ImGui::PopFont();
        }
        if (titleFont) {
            ImGui::PushFont(titleFont);
        }
        if (!serverPageUrl.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, messageColors.action);
            ImGui::TextUnformatted(serverPageUrl.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Open in browser");
            }
            if (ImGui::IsItemClicked()) {
                if (!openUrlInBrowser(serverPageUrl)) {
                    serverLinkStatusText = "Failed to open your browser.";
                    serverLinkStatusIsError = true;
                } else {
                    serverLinkStatusText.clear();
                    serverLinkStatusIsError = false;
                }
            }
        } else {
            ImGui::TextDisabled("No website available.");
        }
        if (titleFont) {
            ImGui::PopFont();
        }
        if (!serverLinkStatusText.empty()) {
            ImGui::Spacing();
            ImVec4 linkColor = serverLinkStatusIsError ? messageColors.error : messageColors.action;
            ImGui::TextColored(linkColor, "%s", serverLinkStatusText.c_str());
        }

        const std::string &displayHost = selectedEntry->displayHost.empty() ? selectedEntry->host : selectedEntry->displayHost;
        ImGui::Text("Host: %s", displayHost.c_str());
        ImGui::Text("Port: %u", selectedEntry->port);

        if (selectedEntry->activePlayers >= 0) {
            if (selectedEntry->maxPlayers >= 0) {
                ImGui::Text("Players: %d/%d", selectedEntry->activePlayers, selectedEntry->maxPlayers);
            } else {
                ImGui::Text("Players: %d", selectedEntry->activePlayers);
            }
        } else if (selectedEntry->maxPlayers >= 0) {
            ImGui::Text("Capacity: %d", selectedEntry->maxPlayers);
        }

        if (!selectedEntry->gameMode.empty()) {
            ImGui::Text("Mode: %s", selectedEntry->gameMode.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted("Description");
        ImGui::PopStyleColor();
        if (!selectedEntry->longDescription.empty()) {
            renderMarkdown(
                selectedEntry->longDescription,
                titleFont,
                headingFont,
                messageColors.action,
                serverLinkStatusText,
                serverLinkStatusIsError);
        } else if (isServerDescriptionLoading(makeServerDetailsKey(*selectedEntry))) {
            ImGui::TextDisabled("Fetching server description...");
        } else {
            if (auto errorText = getServerDescriptionError(makeServerDetailsKey(*selectedEntry))) {
                ImGui::TextDisabled("Description unavailable: %s", errorText->c_str());
            } else {
                ImGui::TextDisabled("No description provided.");
            }
        }

        if (!selectedEntry->screenshotId.empty() && !selectedEntry->sourceHost.empty()) {
            std::string hostBase = normalizedHost(selectedEntry->sourceHost);
            std::string thumbnailUrl = hostBase + "/uploads/" + selectedEntry->screenshotId + "_thumb.jpg";

            if (auto *thumb = getOrLoadThumbnail(thumbnailUrl)) {
                if (thumb->texture.valid()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
                    ImGui::TextUnformatted("Screenshot");
                    ImGui::PopStyleColor();

                    const float maxWidth = ImGui::GetContentRegionAvail().x;
                    const float maxHeight = 220.0f;
                    float scale = 1.0f;
                    scale = std::min(scale, maxWidth / static_cast<float>(thumb->texture.width));
                    scale = std::min(scale, maxHeight / static_cast<float>(thumb->texture.height));
                    if (scale <= 0.0f) {
                        scale = 1.0f;
                    }

                    ImVec2 imageSize(
                        static_cast<float>(thumb->texture.width) * scale,
                        static_cast<float>(thumb->texture.height) * scale);

                    ImGui::Image(ui::ToImGuiTextureId(thumb->texture), imageSize);
                } else if (thumb->failed) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Screenshot unavailable.");
                } else if (thumb->loading) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Loading screenshot...");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted("Plugins");
        ImGui::PopStyleColor();
        if (!selectedEntry->flags.empty()) {
            for (const auto &flag : selectedEntry->flags) {
                ImGui::BulletText("%s", flag.c_str());
            }
        } else {
            ImGui::TextDisabled("No plugins reported.");
        }
    }

    ImGui::EndChild();
}

} // namespace ui

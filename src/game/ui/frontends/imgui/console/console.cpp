#include "ui/frontends/imgui/console/console.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <string_view>
#include <vector>

#include "karma/common/json.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/i18n.hpp"
#include "spdlog/spdlog.h"
#include "ui/config/config.hpp"
#include "ui/console/tab_spec.hpp"
#include "ui/fonts/console_fonts.hpp"
#include "ui/config/ui_config.hpp"

namespace {
std::string trimCopy(const std::string &value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (begin >= end) {
        return {};
    }

    return std::string(begin, end);
}

ImVec4 readColorConfig(const char *path, const ImVec4 &fallback) {
    const auto *value = karma::config::ConfigStore::Get(path);
    if (!value || !value->is_array()) {
        return fallback;
    }
    const auto &arr = *value;
    if (arr.size() < 3 || arr.size() > 4) {
        return fallback;
    }
    auto readComponent = [&](std::size_t index, float defaultValue) -> float {
        if (index >= arr.size() || !arr[index].is_number()) {
            return defaultValue;
        }
        return static_cast<float>(arr[index].get<double>());
    };
    ImVec4 color = fallback;
    color.x = readComponent(0, color.x);
    color.y = readComponent(1, color.y);
    color.z = readComponent(2, color.z);
    if (arr.size() >= 4) {
        color.w = readComponent(3, color.w);
    }
    return color;
}

std::string tabLabelForSpec(const ui::ConsoleTabSpec &spec) {
    if (spec.labelKey) {
        return karma::i18n::Get().get(spec.labelKey);
    }
    if (spec.fallbackLabel) {
        return spec.fallbackLabel;
    }
    return spec.key ? spec.key : "";
}

const ui::ConsoleTabSpec *findTabSpec(std::string_view key) {
    for (const auto &spec : ui::GetConsoleTabSpecs()) {
        if (spec.key && key == spec.key) {
            return &spec;
        }
    }
    return nullptr;
}

}

namespace ui {

void ConsoleView::initializeFonts(ImGuiIO &io) {
    const ImVec4 defaultTextColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
    auto addFallbackFont = [&](const char *assetKey, float size, const ImWchar *ranges, const char *label) {
        const auto fontPath = karma::data::ResolveConfiguredAsset(assetKey);
        if (fontPath.empty()) {
            return;
        }
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        ImFont *font = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), size, &config, ranges);
        if (!font) {
            spdlog::warn("Failed to load fallback font {} ({}).", label, fontPath.string());
        }
    };
    auto addFallbacksForSelection = [&](float size, ui::fonts::Script script) {
        const ImWchar *latinRanges = io.Fonts->GetGlyphRangesDefault();
        addFallbackFont("hud.fonts.console.FallbackLatin.Font", size, latinRanges, "FallbackLatin");
        if (script == ui::fonts::Script::Cyrillic) {
            addFallbackFont("hud.fonts.console.FallbackLatin.Font", size, io.Fonts->GetGlyphRangesCyrillic(), "FallbackCyrillic");
        } else if (script == ui::fonts::Script::Arabic) {
            static const ImWchar arabicRanges[] = {
                0x0600, 0x06FF, 0x0750, 0x077F, 0x08A0, 0x08FF, 0xFB50, 0xFDFF, 0xFE70, 0xFEFF, 0
            };
            addFallbackFont("hud.fonts.console.FallbackArabic.Font", size, arabicRanges, "FallbackArabic");
        } else if (script == ui::fonts::Script::Devanagari) {
            static const ImWchar devanagariRanges[] = { 0x0900, 0x097F, 0 };
            addFallbackFont("hud.fonts.console.FallbackDevanagari.Font", size, devanagariRanges, "FallbackDevanagari");
        } else if (script == ui::fonts::Script::CjkJp) {
            addFallbackFont("hud.fonts.console.FallbackCJK_JP.Font", size, io.Fonts->GetGlyphRangesJapanese(), "FallbackCJK_JP");
        } else if (script == ui::fonts::Script::CjkKr) {
            addFallbackFont("hud.fonts.console.FallbackCJK_KR.Font", size, io.Fonts->GetGlyphRangesKorean(), "FallbackCJK_KR");
        } else if (script == ui::fonts::Script::CjkSc) {
            addFallbackFont("hud.fonts.console.FallbackCJK_SC.Font", size, io.Fonts->GetGlyphRangesChineseSimplifiedCommon(), "FallbackCJK_SC");
        }
    };
    const std::string language = karma::i18n::Get().language();
    const ui::fonts::ConsoleFontAssets assets = ui::fonts::GetConsoleFontAssets(language, true);
    const ui::fonts::ConsoleFontSelection &selection = assets.selection;
    const char *regularFontKey = selection.regularFontKey.c_str();
    const auto regularFontPath = karma::data::ResolveConfiguredAsset(regularFontKey);
    const std::string regularFontPathStr = regularFontPath.string();
    const ImWchar *regularRanges = nullptr;
    if (selection.script == ui::fonts::Script::Cyrillic) {
        regularRanges = io.Fonts->GetGlyphRangesCyrillic();
    } else if (selection.script == ui::fonts::Script::Arabic) {
        static const ImWchar arabicRanges[] = {
            0x0600, 0x06FF, 0x0750, 0x077F, 0x08A0, 0x08FF, 0xFB50, 0xFDFF, 0xFE70, 0xFEFF, 0
        };
        regularRanges = arabicRanges;
    } else if (selection.script == ui::fonts::Script::Devanagari) {
        static const ImWchar devanagariRanges[] = { 0x0900, 0x097F, 0 };
        regularRanges = devanagariRanges;
    } else if (selection.script == ui::fonts::Script::CjkJp) {
        regularRanges = io.Fonts->GetGlyphRangesJapanese();
    } else if (selection.script == ui::fonts::Script::CjkKr) {
        regularRanges = io.Fonts->GetGlyphRangesKorean();
    } else if (selection.script == ui::fonts::Script::CjkSc) {
        regularRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    }
    const float regularFontSize = ui::config::GetRequiredFloat("assets.hud.fonts.console.Regular.Size");
    this->regularFontSize = regularFontSize;
    regularFont = io.Fonts->AddFontFromFileTTF(
        regularFontPathStr.c_str(),
        regularFontSize,
        nullptr,
        regularRanges
    );
    regularColor = readColorConfig("assets.hud.fonts.console.Regular.Color", defaultTextColor);
    if (regularFont) {
        addFallbacksForSelection(regularFontSize, selection.script);
    }

    if (!regularFont) {
        spdlog::warn("Failed to load console regular font for community browser ({}).", regularFontPathStr);
    }



    const auto titleFontPath = karma::data::ResolveConfiguredAsset(assets.titleKey);
    const std::string titleFontPathStr = titleFontPath.string();
    const float titleFontSize = ui::config::GetRequiredFloat("assets.hud.fonts.console.Title.Size");
    this->titleFontSize = titleFontSize;
    titleFont = io.Fonts->AddFontFromFileTTF(
        titleFontPathStr.c_str(),
        titleFontSize
    );
    titleColor = readColorConfig("assets.hud.fonts.console.Title.Color", defaultTextColor);
    if (titleFont) {
        addFallbacksForSelection(titleFontSize, selection.script);
    }

    if (!titleFont) {
        spdlog::warn("Failed to load console title font for community browser ({}).", titleFontPathStr);
    }

    const auto headingFontPath = karma::data::ResolveConfiguredAsset(assets.headingKey);
    const std::string headingFontPathStr = headingFontPath.string();
    const float headingFontSize = ui::config::GetRequiredFloat("assets.hud.fonts.console.Heading.Size");
    this->headingFontSize = headingFontSize;
    headingFont = io.Fonts->AddFontFromFileTTF(
        headingFontPathStr.c_str(),
        headingFontSize
    );
    headingColor = readColorConfig("assets.hud.fonts.console.Heading.Color", defaultTextColor);
    if (headingFont) {
        addFallbacksForSelection(headingFontSize, selection.script);
    }

    if (!headingFont) {
        spdlog::warn("Failed to load console heading font for community browser ({}).", headingFontPathStr);
    }

    const auto buttonFontPath = karma::data::ResolveConfiguredAsset(assets.buttonKey);
    const std::string buttonFontPathStr = buttonFontPath.string();
    const float buttonFontSize = ui::config::GetRequiredFloat("assets.hud.fonts.console.Button.Size");
    buttonFont = io.Fonts->AddFontFromFileTTF(
        buttonFontPathStr.c_str(),
        buttonFontSize
    );
    buttonColor = readColorConfig("assets.hud.fonts.console.Button.Color", defaultTextColor);
    if (buttonFont) {
        addFallbacksForSelection(buttonFontSize, selection.script);
    }

    if (!buttonFont) {
        spdlog::warn("Failed to load console button font for community browser ({}).", buttonFontPathStr);
    }

}

void ConsoleView::draw(ImGuiIO &io) {
    if (!visible) {
        return;
    }

    thumbnails.processUploads();

    const bool pushedRegularFont = (regularFont != nullptr);
    if (pushedRegularFont) {
        ImGui::PushFont(regularFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, regularColor);
    const bool pushedRegularColor = true;

    const bool hasHeadingFont = (headingFont != nullptr);
    const bool hasButtonFont = (buttonFont != nullptr);

    const ImVec2 windowSize(1200.0f, 680.0f);
    const ImVec2 windowPos(
        (io.DisplaySize.x - windowSize.x) * 0.5f,
        (io.DisplaySize.y - windowSize.y) * 0.5f
    );

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    const float bgAlpha = consoleModel.connectionState.connected ? 0.95f : 1.0f;
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;

    const ImGuiStyle &style = ImGui::GetStyle();
    if (!consoleModel.connectionState.connected) {
        const ImVec2 screenMin(0.0f, 0.0f);
        const ImVec2 screenMax(io.DisplaySize.x, io.DisplaySize.y);
        ImVec4 bg = style.Colors[ImGuiCol_WindowBg];
        bg.w = 1.0f;
        ImGui::GetBackgroundDrawList()->AddRectFilled(screenMin, screenMax,
                                                      ImGui::GetColorU32(bg));
    }
    ImFont *titleFontToUse = titleFont ? titleFont : (headingFont ? headingFont : regularFont);
    if (titleFontToUse) {
        ImGui::PushFont(titleFontToUse);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x + 6.0f, style.FramePadding.y + 4.0f));
    auto &i18n = karma::i18n::Get();
    const std::string windowTitle = i18n.get("ui.console.title");
    ImGui::Begin((windowTitle + "###MainConsole").c_str(), nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (titleFontToUse) {
        ImGui::PopFont();
    }

    const MessageColors messageColors = getMessageColors();
    const uint64_t revision = karma::config::ConfigStore::Revision();
    if (revision != lastConfigRevision) {
        lastConfigRevision = revision;
        handleConfigChanged();
    }
    if (ImGui::BeginTabBar("CommunityBrowserTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        std::string nextActiveTab = activeTabKey;
        bool activeTabFound = false;
        for (const auto &spec : ui::GetConsoleTabSpecs()) {
            const std::string label = spec.labelKey ? i18n.get(spec.labelKey)
                                                    : (spec.fallbackLabel ? spec.fallbackLabel : spec.key);
            const std::string tabId = label + "###Tab" + spec.key;
            const bool tabOpen = ImGui::BeginTabItem(tabId.c_str());
            if (spec.refreshOnActivate && (ImGui::IsItemActivated() || ImGui::IsItemClicked())) {
                consoleController.requestRefresh();
            }
            if (tabOpen) {
                nextActiveTab = spec.key ? spec.key : "";
                activeTabFound = true;
                handleTabTick(nextActiveTab);
                drawTabContent(spec.key, messageColors);
                ImGui::EndTabItem();
            }
        }
        if (activeTabFound && nextActiveTab != activeTabKey) {
            if (!activeTabKey.empty()) {
                handleTabHide(activeTabKey);
            }
            if (!nextActiveTab.empty()) {
                handleTabShow(nextActiveTab);
            }
            activeTabKey = nextActiveTab;
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (pushedRegularColor) {
        ImGui::PopStyleColor();
    }
    if (pushedRegularFont) {
        ImGui::PopFont();
    }
}

void ConsoleView::drawTabContent(const std::string &key, const MessageColors &colors) {
    if (key == "community") {
        drawPanelHeader(key);
        drawCommunityPanel(colors);
        return;
    }
    if (key == "start-server") {
        drawPanelHeader(key);
        drawStartServerPanel(colors);
        return;
    }
    if (key == "settings") {
        drawPanelHeader(key);
        drawSettingsPanel(colors);
        return;
    }
    if (key == "bindings") {
        drawPanelHeader(key);
        drawBindingsPanel(colors);
        return;
    }
    if (key == "documentation") {
        drawPanelHeader(key);
        drawDocumentationPanel(colors);
        return;
    }
    drawPlaceholderPanel("Panel missing", "This panel is not available.", colors);
}

void ConsoleView::handleConfigChanged() {
    settingsModel.loaded = false;
    bindingsModel.loaded = false;
}

void ConsoleView::handleTabShow(const std::string &key) {
    if (key == "settings") {
        settingsModel.loaded = false;
    } else if (key == "bindings") {
        bindingsModel.loaded = false;
    }
}

void ConsoleView::handleTabHide(const std::string &key) {
    if (key == "bindings") {
        bindingsModel.selectedIndex = -1;
    }
}

void ConsoleView::handleTabTick(const std::string & /*key*/) {
    if (bindingsModel.selectedIndex >= static_cast<int>(ui::BindingsModel::kKeybindingCount)) {
        bindingsModel.selectedIndex = -1;
    }
}

void ConsoleView::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    settingsModel.loaded = false;
    bindingsModel.loaded = false;
    settingsModel.render.reset();
}

void ConsoleView::setLanguageCallback(std::function<void(const std::string &)> callback) {
    languageCallback = std::move(callback);
}

bool ConsoleView::consumeFontReloadRequest() {
    const bool requested = fontReloadRequested;
    fontReloadRequested = false;
    return requested;
}

bool ConsoleView::consumeKeybindingsReloadRequest() {
    const bool requested = keybindingsReloadRequested;
    keybindingsReloadRequested = false;
    return requested;
}

void ConsoleView::requestKeybindingsReload() {
    keybindingsReloadRequested = true;
}

void ConsoleView::setConnectionState(const ConnectionState &state) {
    consoleModel.connectionState = state;
}

ConsoleInterface::ConnectionState ConsoleView::getConnectionState() const {
    return consoleModel.connectionState;
}

bool ConsoleView::consumeQuitRequest() {
    if (!pendingQuitRequest) {
        return false;
    }
    pendingQuitRequest = false;
    return true;
}

void ConsoleView::showErrorDialog(const std::string &message) {
    errorDialogMessage = message;
}

void ConsoleView::drawPlaceholderPanel(const char *heading,
                                                const char *body,
                                                const MessageColors &colors) const {
    if (headingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::TextUnformatted(heading);
    ImGui::PopStyleColor();
    if (headingFont) {
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, colors.notice);
    ImGui::TextWrapped("%s", body);
    ImGui::PopStyleColor();
}

void ConsoleView::drawPanelHeader(std::string_view tabKey) const {
    const ui::ConsoleTabSpec *spec = findTabSpec(tabKey);
    std::string label = spec ? tabLabelForSpec(*spec) : std::string(tabKey);
    if (label.empty()) {
        label = std::string(tabKey);
    }

    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
    if (headingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();
    if (headingFont) {
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void ConsoleView::show(const std::vector<CommunityBrowserEntry> &newEntries) {
    visible = true;
    setEntries(newEntries);
    consoleController.clearPending();
    consoleModel.community.statusText = "Select a server to connect.";
    consoleModel.community.statusIsError = false;
    consoleModel.community.listStatusText.clear();
    consoleModel.community.listStatusIsError = false;
    consoleModel.community.communityStatusText.clear();
    consoleModel.community.detailsText.clear();
    consoleModel.community.communityLinkStatusText.clear();
    consoleModel.community.communityLinkStatusIsError = false;
    consoleModel.community.serverLinkStatusText.clear();
    consoleModel.community.serverLinkStatusIsError = false;
    consoleModel.community.serverDescriptionLoadingKey.clear();
    consoleModel.community.serverDescriptionLoading = false;
    consoleModel.community.serverDescriptionErrorKey.clear();
    consoleModel.community.serverDescriptionErrorText.clear();
    consoleModel.community.statusTone = MessageTone::Notice;
    clearPassword();
    listUrlBuffer.fill(0);
}

ConsoleView::~ConsoleView() {
    stopAllLocalServers();
    thumbnails.shutdown();
}

void ConsoleView::setEntries(const std::vector<CommunityBrowserEntry> &newEntries) {
    consoleModel.community.entries = newEntries;
    if (consoleModel.community.entries.empty()) {
        consoleModel.community.selectedIndex = -1;
    } else if (consoleModel.community.selectedIndex < 0) {
        consoleModel.community.selectedIndex = 0;
    } else if (consoleModel.community.selectedIndex >= static_cast<int>(consoleModel.community.entries.size())) {
        consoleModel.community.selectedIndex = static_cast<int>(consoleModel.community.entries.size()) - 1;
    }
}

void ConsoleView::setListOptions(const std::vector<ServerListOption> &options, int selectedIndexIn) {
    consoleModel.community.listOptions = options;
    if (consoleModel.community.listOptions.empty()) {
        consoleModel.community.listSelectedIndex = -1;
        serverCommunityIndex = -1;
        lastCredentialsListIndex = -1;
        consoleController.clearPending();
        return;
    }

    if (selectedIndexIn < 0) {
        consoleModel.community.listSelectedIndex = 0;
    } else if (selectedIndexIn >= static_cast<int>(consoleModel.community.listOptions.size())) {
        consoleModel.community.listSelectedIndex = static_cast<int>(consoleModel.community.listOptions.size()) - 1;
    } else {
        consoleModel.community.listSelectedIndex = selectedIndexIn;
    }

    if (serverCommunityIndex < 0 || serverCommunityIndex >= static_cast<int>(consoleModel.community.listOptions.size())) {
        serverCommunityIndex = consoleModel.community.listSelectedIndex;
    }
}

std::string ConsoleView::communityKeyForIndex(int index) const {
    return consoleController.communityKeyForIndex(index);
}

void ConsoleView::refreshCommunityCredentials() {
    if (consoleModel.community.listSelectedIndex == lastCredentialsListIndex) {
        return;
    }
    lastCredentialsListIndex = consoleModel.community.listSelectedIndex;
    usernameBuffer.fill(0);
    passwordBuffer.fill(0);
    storedPasswordHash.clear();

    const auto creds = consoleController.loadCommunityCredentials(consoleModel.community.listSelectedIndex);
    if (!creds.username.empty()) {
        std::snprintf(usernameBuffer.data(), usernameBuffer.size(), "%s", creds.username.c_str());
    }
    if (!creds.storedPasswordHash.empty()) {
        storedPasswordHash = creds.storedPasswordHash;
    }
}

void ConsoleView::persistCommunityCredentials(bool passwordChanged) {
    const std::string username = trimCopy(usernameBuffer.data());
    const auto result = consoleController.persistCommunityCredentials(
        consoleModel.community.listSelectedIndex,
        username,
        storedPasswordHash,
        passwordChanged);
    if (result.clearStoredPasswordHash) {
        storedPasswordHash.clear();
    }
}

void ConsoleView::storeCommunityAuth(const std::string &communityHost,
                                      const std::string &username,
                                      const std::string &passhash,
                                      const std::string &salt) {
    if (communityHost.empty() || username.empty()) {
        return;
    }

    std::string key = communityHost;
    while (!key.empty() && key.back() == '/') {
        key.pop_back();
    }

    karma::json::Value creds = karma::json::Object();
    if (const auto *existing = ui::UiConfig::GetCommunityCredentials()) {
        if (existing->is_object()) {
            creds = *existing;
        }
    }
    if (!creds.contains(key) || !creds[key].is_object()) {
        creds[key] = karma::json::Object();
    }
    creds[key]["username"] = username;
    if (!passhash.empty()) {
        creds[key]["passwordHash"] = passhash;
    }
    if (!salt.empty()) {
        creds[key]["salt"] = salt;
    }
    ui::UiConfig::SetCommunityCredentials(creds);

    const std::string activeKey = communityKeyForIndex(consoleModel.community.listSelectedIndex);
    if (activeKey == key) {
        std::snprintf(usernameBuffer.data(), usernameBuffer.size(), "%s", username.c_str());
        if (!passhash.empty()) {
            storedPasswordHash = passhash;
        }
    }
}

void ConsoleView::hide() {
    visible = false;
    renderBrightnessDragging = false;
    consoleModel.community.statusText.clear();
    consoleModel.community.statusIsError = false;
    consoleController.clearPending();
    consoleModel.community.scanning = false;
    consoleModel.community.listStatusText.clear();
    consoleModel.community.listStatusIsError = false;
    consoleModel.community.communityStatusText.clear();
    consoleModel.community.detailsText.clear();
    consoleModel.community.communityLinkStatusText.clear();
    consoleModel.community.communityLinkStatusIsError = false;
    consoleModel.community.serverLinkStatusText.clear();
    consoleModel.community.serverLinkStatusIsError = false;
    consoleModel.community.serverDescriptionLoadingKey.clear();
    consoleModel.community.serverDescriptionLoading = false;
    consoleModel.community.serverDescriptionErrorKey.clear();
    consoleModel.community.serverDescriptionErrorText.clear();
    consoleModel.community.statusTone = MessageTone::Notice;
    clearPassword();
    thumbnails.shutdown();
}

bool ConsoleView::isVisible() const {
    return visible;
}

void ConsoleView::setStatus(const std::string &text, bool isErrorMessage) {
    consoleModel.community.statusText = text;
    consoleModel.community.statusIsError = isErrorMessage;
}

void ConsoleView::setCommunityDetails(const std::string &detailsText) {
    consoleModel.community.detailsText = detailsText;
}

void ConsoleView::setServerDescriptionLoading(const std::string &key, bool loading) {
    consoleModel.community.serverDescriptionLoadingKey = key;
    consoleModel.community.serverDescriptionLoading = loading;
}

bool ConsoleView::isServerDescriptionLoading(const std::string &key) const {
    if (!consoleModel.community.serverDescriptionLoading || key.empty()) {
        return false;
    }
    return consoleModel.community.serverDescriptionLoadingKey == key;
}

void ConsoleView::setServerDescriptionError(const std::string &key, const std::string &message) {
    consoleModel.community.serverDescriptionErrorKey = key;
    consoleModel.community.serverDescriptionErrorText = message;
}

std::optional<std::string> ConsoleView::getServerDescriptionError(const std::string &key) const {
    if (key.empty() || consoleModel.community.serverDescriptionErrorKey.empty()) {
        return std::nullopt;
    }
    if (consoleModel.community.serverDescriptionErrorKey != key) {
        return std::nullopt;
    }
    if (consoleModel.community.serverDescriptionErrorText.empty()) {
        return std::nullopt;
    }
    return consoleModel.community.serverDescriptionErrorText;
}

std::optional<CommunityBrowserSelection> ConsoleView::consumeSelection() {
    return consoleController.consumeSelection();
}

std::optional<int> ConsoleView::consumeListSelection() {
    return consoleController.consumeListSelection();
}

std::optional<ServerListOption> ConsoleView::consumeNewListRequest() {
    return consoleController.consumeNewListRequest();
}

std::optional<std::string> ConsoleView::consumeDeleteListRequest() {
    return consoleController.consumeDeleteListRequest();
}

void ConsoleView::setListStatus(const std::string &text, bool isErrorMessage) {
    consoleModel.community.listStatusText = text;
    consoleModel.community.listStatusIsError = isErrorMessage;
}

void ConsoleView::clearNewListInputs() {
    listUrlBuffer.fill(0);
}

void ConsoleView::setCommunityStatus(const std::string &text, MessageTone tone) {
    consoleModel.community.communityStatusText = text;
    consoleModel.community.statusTone = tone;
}

std::optional<CommunityBrowserEntry> ConsoleView::getSelectedEntry() const {
    if (consoleModel.community.selectedIndex < 0 ||
        consoleModel.community.selectedIndex >= static_cast<int>(consoleModel.community.entries.size())) {
        return std::nullopt;
    }
    return consoleModel.community.entries[static_cast<std::size_t>(consoleModel.community.selectedIndex)];
}

std::string ConsoleView::getUsername() const {
    return trimCopy(usernameBuffer.data());
}

std::string ConsoleView::getPassword() const {
    return std::string(passwordBuffer.data());
}

std::string ConsoleView::getStoredPasswordHash() const {
    return storedPasswordHash;
}

void ConsoleView::clearPassword() {
    passwordBuffer.fill(0);
}

bool ConsoleView::consumeRefreshRequest() {
    return consoleController.consumeRefreshRequest();
}

void ConsoleView::setScanning(bool isScanning) {
    consoleModel.community.scanning = isScanning;
}

ThumbnailTexture *ConsoleView::getOrLoadThumbnail(const std::string &url) {
    return thumbnails.getOrLoad(url);
}

ConsoleView::MessageColors ConsoleView::getMessageColors() const {
    MessageColors colors;
    colors.error = ImVec4(0.93f, 0.36f, 0.36f, 1.0f);
    colors.notice = ImVec4(0.90f, 0.80f, 0.30f, 1.0f);
    colors.action = ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
    colors.pending = ImVec4(0.35f, 0.70f, 0.95f, 1.0f);
    return colors;
}

} // namespace ui

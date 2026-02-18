#include "ui/frontends/rmlui/console/console.hpp"

#include "ui/frontends/rmlui/console/panels/panel_community.hpp"
#include "ui/frontends/rmlui/console/panels/panel_start_server.hpp"
#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"
#include "ui/frontends/rmlui/console/panels/panel_bindings.hpp"

#include <filesystem>

#include "karma/common/data/path_resolver.hpp"
#include "karma/common/logging/logging.hpp"
#include "spdlog/spdlog.h"

namespace ui {

RmlUiConsole::RmlUiConsole() = default;

void RmlUiConsole::attachCommunityPanel(RmlUiPanelCommunity *panel) {
    communityPanel = panel;
    if (communityPanel) {
        communityPanel->setConsoleModel(&consoleModel, &consoleController);
        communityPanel->setConnectionState(consoleModel.connectionState);
        if (!userConfigPath.empty()) {
            communityPanel->setUserConfigPath(userConfigPath);
        }
    }
    applyListOptionsToPanel();
}

void RmlUiConsole::attachStartServerPanel(RmlUiPanelStartServer *panel) {
    startServerPanel = panel;
    if (startServerPanel) {
        startServerPanel->setConsoleModel(&consoleModel);
        startServerPanel->setListOptions(consoleModel.community.listOptions,
                                         consoleModel.community.listSelectedIndex);
    }
}

void RmlUiConsole::attachSettingsPanel(RmlUiPanelSettings *panel) {
    settingsPanel = panel;
    if (settingsPanel && !userConfigPath.empty()) {
        settingsPanel->setUserConfigPath(userConfigPath);
    }
}

void RmlUiConsole::attachBindingsPanel(RmlUiPanelBindings *panel) {
    bindingsPanel = panel;
    if (bindingsPanel && !userConfigPath.empty()) {
        bindingsPanel->setUserConfigPath(userConfigPath);
    }
}

void RmlUiConsole::show(const std::vector<CommunityBrowserEntry> &entriesIn) {
    if (!entriesIn.empty()) {
        consoleModel.community.entries = entriesIn;
    }
    visible = true;
    consoleController.clearPending();
    if (communityPanel) {
        communityPanel->setEntries(consoleModel.community.entries);
    }
}

void RmlUiConsole::setEntries(const std::vector<CommunityBrowserEntry> &entriesIn) {
    consoleModel.community.entries = entriesIn;
    if (consoleModel.community.selectedIndex >= static_cast<int>(consoleModel.community.entries.size())) {
        consoleModel.community.selectedIndex = -1;
    }
    if (communityPanel) {
        communityPanel->setEntries(consoleModel.community.entries);
    }
}

void RmlUiConsole::setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) {
    consoleModel.community.listOptions = options;
    if (consoleModel.community.listOptions.empty()) {
        consoleModel.community.listSelectedIndex = -1;
        consoleController.clearPending();
        return;
    }
    if (selectedIndex < 0) {
        consoleModel.community.listSelectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(consoleModel.community.listOptions.size())) {
        consoleModel.community.listSelectedIndex = static_cast<int>(consoleModel.community.listOptions.size()) - 1;
    } else {
        consoleModel.community.listSelectedIndex = selectedIndex;
    }
    applyListOptionsToPanel();
    refreshCommunityCredentials();
    if (startServerPanel) {
        startServerPanel->setListOptions(consoleModel.community.listOptions,
                                         consoleModel.community.listSelectedIndex);
    }
}

void RmlUiConsole::hide() {
    visible = false;
    consoleController.clearPending();
}

bool RmlUiConsole::isVisible() const {
    return visible;
}

void RmlUiConsole::setStatus(const std::string &, bool) {}

void RmlUiConsole::setCommunityDetails(const std::string &detailsText) {
    consoleModel.community.detailsText = detailsText;
    if (communityPanel) {
        communityPanel->setCommunityDetails(detailsText);
    }
}

void RmlUiConsole::setServerDescriptionLoading(const std::string &key, bool loading) {
    consoleModel.community.serverDescriptionLoadingKey = key;
    consoleModel.community.serverDescriptionLoading = loading;
    if (!loading && key.empty()) {
        consoleModel.community.serverDescriptionLoadingKey.clear();
    }
    if (communityPanel) {
        communityPanel->setServerDescriptionLoading(key, loading);
    }
}

bool RmlUiConsole::isServerDescriptionLoading(const std::string &key) const {
    return consoleModel.community.serverDescriptionLoading &&
           key == consoleModel.community.serverDescriptionLoadingKey;
}

void RmlUiConsole::setServerDescriptionError(const std::string &key, const std::string &message) {
    consoleModel.community.serverDescriptionErrorKey = key;
    consoleModel.community.serverDescriptionErrorText = message;
    if (communityPanel) {
        communityPanel->setServerDescriptionError(key, message);
    }
}

std::optional<std::string> RmlUiConsole::getServerDescriptionError(const std::string &key) const {
    if (key.empty() || key != consoleModel.community.serverDescriptionErrorKey) {
        return std::nullopt;
    }
    return consoleModel.community.serverDescriptionErrorText;
}

std::optional<CommunityBrowserSelection> RmlUiConsole::consumeSelection() {
    return consoleController.consumeSelection();
}

std::optional<int> RmlUiConsole::consumeListSelection() {
    return consoleController.consumeListSelection();
}

std::optional<ServerListOption> RmlUiConsole::consumeNewListRequest() {
    return consoleController.consumeNewListRequest();
}

std::optional<std::string> RmlUiConsole::consumeDeleteListRequest() {
    return consoleController.consumeDeleteListRequest();
}


void RmlUiConsole::setListStatus(const std::string &statusText, bool isErrorMessage) {
    consoleModel.community.listStatusText = statusText;
    consoleModel.community.listStatusIsError = isErrorMessage;
    if (communityPanel) {
        communityPanel->setAddStatus(statusText, isErrorMessage);
    }
}

void RmlUiConsole::clearNewListInputs() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

std::string RmlUiConsole::getUsername() const {
    if (communityPanel) {
        return communityPanel->getUsernameValue();
    }
    return {};
}

std::string RmlUiConsole::getPassword() const {
    if (communityPanel) {
        return communityPanel->getPasswordValue();
    }
    return {};
}

std::string RmlUiConsole::getStoredPasswordHash() const {
    if (communityPanel) {
        return communityPanel->getStoredPasswordHashValue();
    }
    return {};
}

void RmlUiConsole::clearPassword() {
    if (communityPanel) {
        communityPanel->clearPasswordValue();
    }
}

void RmlUiConsole::storeCommunityAuth(const std::string &communityHost,
                                       const std::string &username,
                                       const std::string &passhash,
                                       const std::string &salt) {
    (void)communityHost;
    (void)salt;
    if (communityPanel && !username.empty()) {
        communityPanel->setUsernameValue(username);
    }
    if (communityPanel && !passhash.empty()) {
        communityPanel->setStoredPasswordHashValue(passhash);
        communityPanel->persistCommunityCredentials(false);
    }
}

void RmlUiConsole::setCommunityStatus(const std::string &text, MessageTone tone) {
    consoleModel.community.communityStatusText = text;
    consoleModel.community.statusTone = tone;
}

std::optional<CommunityBrowserEntry> RmlUiConsole::getSelectedEntry() const {
    if (consoleModel.community.selectedIndex < 0 ||
        consoleModel.community.selectedIndex >= static_cast<int>(consoleModel.community.entries.size())) {
        return std::nullopt;
    }
    return consoleModel.community.entries[static_cast<std::size_t>(consoleModel.community.selectedIndex)];
}

bool RmlUiConsole::consumeRefreshRequest() {
    return consoleController.consumeRefreshRequest();
}

void RmlUiConsole::setScanning(bool scanning) {
    consoleModel.community.scanning = scanning;
}

void RmlUiConsole::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    refreshCommunityCredentials();
    if (communityPanel) {
        communityPanel->setUserConfigPath(path);
    }
    if (settingsPanel) {
        settingsPanel->setUserConfigPath(path);
    }
    if (bindingsPanel) {
        bindingsPanel->setUserConfigPath(path);
    }
}

bool RmlUiConsole::consumeFontReloadRequest() {
    return false;
}

bool RmlUiConsole::consumeKeybindingsReloadRequest() {
    if (!bindingsPanel) {
        return false;
    }
    return bindingsPanel->consumeKeybindingsReloadRequest();
}

void RmlUiConsole::setConnectionState(const ConnectionState &state) {
    consoleModel.connectionState = state;
    if (communityPanel) {
        communityPanel->setConnectionState(state);
    }
}

ConsoleInterface::ConnectionState RmlUiConsole::getConnectionState() const {
    return consoleModel.connectionState;
}

bool RmlUiConsole::consumeQuitRequest() {
    if (!pendingQuitRequest) {
        return false;
    }
    pendingQuitRequest = false;
    return true;
}

void RmlUiConsole::showErrorDialog(const std::string &message) {
    if (communityPanel) {
        communityPanel->showErrorDialog(message);
    }
}

void RmlUiConsole::onCommunitySelection(int index) {
    if (index < 0 || index >= static_cast<int>(consoleModel.community.listOptions.size())) {
        return;
    }
    if (consoleModel.community.listSelectedIndex != index) {
        consoleModel.community.listSelectedIndex = index;
        consoleController.queueListSelection(index);
        consoleModel.community.selectedIndex = -1;
    }
    refreshCommunityCredentials();
}

void RmlUiConsole::onCommunityAddRequested(const std::string &host) {
    if (host.empty()) {
        return;
    }
    consoleController.queueNewListRequest(ServerListOption{std::string{}, host});
}

void RmlUiConsole::onCommunityAddCanceled() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

void RmlUiConsole::onRefreshRequested() {
    consoleController.requestRefresh();
}

void RmlUiConsole::onServerSelection(int index) {
    if (index < 0 || index >= static_cast<int>(consoleModel.community.entries.size())) {
        return;
    }
    consoleModel.community.selectedIndex = index;
}

std::string RmlUiConsole::communityKeyForIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(consoleModel.community.listOptions.size())) {
        return {};
    }
    const auto &option = consoleModel.community.listOptions[index];
    if (option.name == "Local Area Network") {
        return "LAN";
    }
    std::string host = option.host;
    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }
    return host;
}

void RmlUiConsole::refreshCommunityCredentials() {
    if (consoleModel.community.listSelectedIndex == lastCredentialsListIndex) {
        return;
    }
    lastCredentialsListIndex = consoleModel.community.listSelectedIndex;
    if (communityPanel) {
        communityPanel->refreshCommunityCredentials();
    }
}


void RmlUiConsole::onJoinRequested(int index) {
    if (index < 0 || index >= static_cast<int>(consoleModel.community.entries.size())) {
        spdlog::warn("RmlUi Console: Join requested with invalid index {}", index);
        return;
    }
    const auto &entry = consoleModel.community.entries[static_cast<std::size_t>(index)];
    consoleController.queueSelection(CommunityBrowserSelection{
        entry.host,
        entry.port,
        true,
        entry.sourceHost,
        entry.worldName,
        false
    });
    KARMA_TRACE("ui.rmlui",
                "RmlUi Console: Join queued host={} port={} sourceHost={} worldName={}",
                entry.host,
                entry.port,
                entry.sourceHost,
                entry.worldName);
}

void RmlUiConsole::onRoamRequested(int index) {
    if (index < 0 || index >= static_cast<int>(consoleModel.community.entries.size())) {
        spdlog::warn("RmlUi Console: Roam requested with invalid index {}", index);
        return;
    }
    const auto &entry = consoleModel.community.entries[static_cast<std::size_t>(index)];
    consoleController.queueSelection(CommunityBrowserSelection{
        entry.host,
        entry.port,
        true,
        entry.sourceHost,
        entry.worldName,
        true
    });
    KARMA_TRACE("ui.rmlui",
                "RmlUi Console: Roam queued host={} port={} sourceHost={} worldName={}",
                entry.host,
                entry.port,
                entry.sourceHost,
                entry.worldName);
}

void RmlUiConsole::onQuitRequested() {
    pendingQuitRequest = true;
}

void RmlUiConsole::applyListOptionsToPanel() {
    if (!communityPanel) {
        return;
    }
    communityPanel->setListOptions(consoleModel.community.listOptions,
                                   consoleModel.community.listSelectedIndex);
}

} // namespace ui

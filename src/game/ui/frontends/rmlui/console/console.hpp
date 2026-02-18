#pragma once

#include <optional>
#include <string>
#include <vector>

#include "karma/common/serialization/json.hpp"

#include "ui/console/console_interface.hpp"
#include "ui/controllers/console_controller.hpp"
#include "ui/models/console_model.hpp"

namespace ui {

class RmlUiPanelCommunity;
class RmlUiPanelStartServer;
class RmlUiPanelSettings;
class RmlUiPanelBindings;

class RmlUiConsole final : public ConsoleInterface {
public:
    RmlUiConsole();

    void attachCommunityPanel(RmlUiPanelCommunity *panel);
    void attachStartServerPanel(RmlUiPanelStartServer *panel);
    void attachSettingsPanel(RmlUiPanelSettings *panel);
    void attachBindingsPanel(RmlUiPanelBindings *panel);

    void show(const std::vector<CommunityBrowserEntry> &entries) override;
    void setEntries(const std::vector<CommunityBrowserEntry> &entries) override;
    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) override;
    void hide() override;
    bool isVisible() const override;
    void setStatus(const std::string &statusText, bool isErrorMessage) override;
    void setCommunityDetails(const std::string &detailsText) override;
    void setServerDescriptionLoading(const std::string &key, bool loading) override;
    bool isServerDescriptionLoading(const std::string &key) const override;
    void setServerDescriptionError(const std::string &key, const std::string &message) override;
    std::optional<std::string> getServerDescriptionError(const std::string &key) const override;
    std::optional<CommunityBrowserSelection> consumeSelection() override;
    std::optional<int> consumeListSelection() override;
    std::optional<ServerListOption> consumeNewListRequest() override;
    std::optional<std::string> consumeDeleteListRequest() override;
    void setListStatus(const std::string &statusText, bool isErrorMessage) override;
    void clearNewListInputs() override;
    std::string getUsername() const override;
    std::string getPassword() const override;
    std::string getStoredPasswordHash() const override;
    void clearPassword() override;
    void storeCommunityAuth(const std::string &communityHost,
                            const std::string &username,
                            const std::string &passhash,
                            const std::string &salt) override;
    void setCommunityStatus(const std::string &text, MessageTone tone) override;
    std::optional<CommunityBrowserEntry> getSelectedEntry() const override;
    bool consumeRefreshRequest() override;
    void setScanning(bool scanning) override;
    void setUserConfigPath(const std::string &path) override;
    bool consumeFontReloadRequest() override;
    bool consumeKeybindingsReloadRequest() override;
    void setConnectionState(const ConnectionState &state) override;
    ConnectionState getConnectionState() const override;
    bool consumeQuitRequest() override;
    void showErrorDialog(const std::string &message) override;

    void onCommunitySelection(int index);
    void onCommunityAddRequested(const std::string &host);
    void onCommunityAddCanceled();
    void onRefreshRequested();
    void onServerSelection(int index);
    void onJoinRequested(int index);
    void onRoamRequested(int index);
    void onQuitRequested();

private:
    void applyListOptionsToPanel();
    std::string communityKeyForIndex(int index) const;
    void refreshCommunityCredentials();

    bool visible = false;
    int lastCredentialsListIndex = -1;
    bool pendingQuitRequest = false;
    std::string userConfigPath;
    ConsoleModel consoleModel;
    ConsoleController consoleController{consoleModel};
    RmlUiPanelCommunity *communityPanel = nullptr;
    RmlUiPanelStartServer *startServerPanel = nullptr;
    RmlUiPanelSettings *settingsPanel = nullptr;
    RmlUiPanelBindings *bindingsPanel = nullptr;
};

} // namespace ui

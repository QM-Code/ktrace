#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct ImGuiIO;
struct ImFont;

#include <imgui.h>
#include "ui/console/console_interface.hpp"
#include "ui/controllers/bindings_controller.hpp"
#include "ui/controllers/settings_controller.hpp"
#include "karma_extras/ui/imgui/thumbnail_cache.hpp"
#include "ui/controllers/console_controller.hpp"
#include "ui/models/bindings_model.hpp"
#include "ui/models/console_model.hpp"
#include "ui/models/settings_model.hpp"

namespace ui {

class ConsoleView : public ConsoleInterface {
public:
    using MessageTone = ui::MessageTone;

    ~ConsoleView();
    void initializeFonts(ImGuiIO &io);
    void draw(ImGuiIO &io);

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
    void setLanguageCallback(std::function<void(const std::string &)> callback);
    float getRenderBrightness() const;
    bool isRenderBrightnessDragActive() const;
    bool consumeFontReloadRequest() override;
    bool consumeKeybindingsReloadRequest() override;
    void requestKeybindingsReload();
    void setConnectionState(const ConnectionState &state) override;
    ConnectionState getConnectionState() const override;
    bool consumeQuitRequest() override;
    void showErrorDialog(const std::string &message) override;

private:
    struct MessageColors {
        ImVec4 error;
        ImVec4 notice;
        ImVec4 action;
        ImVec4 pending;
    };

    struct LocalServerProcess;

    ThumbnailTexture *getOrLoadThumbnail(const std::string &url);
    MessageColors getMessageColors() const;
    void drawTabContent(const std::string &key, const MessageColors &colors);
    void drawSettingsPanel(const MessageColors &colors);
    void drawBindingsPanel(const MessageColors &colors);
    void drawDocumentationPanel(const MessageColors &colors) const;
    void drawStartServerPanel(const MessageColors &colors);
    void drawPanelHeader(std::string_view tabKey) const;
    void drawPlaceholderPanel(const char *heading, const char *body, const MessageColors &colors) const;
    void drawCommunityPanel(const MessageColors &colors);
    void handleConfigChanged();
    void handleTabShow(const std::string &key);
    void handleTabHide(const std::string &key);
    void handleTabTick(const std::string &key);
    std::string communityKeyForIndex(int index) const;
    void refreshCommunityCredentials();
    void persistCommunityCredentials(bool passwordChanged);
    void applyRenderBrightness(float value, bool fromUser);
    bool commitRenderBrightness();
    void stopAllLocalServers();
    void stopLocalServer(std::size_t index);
    std::string findServerBinary();
    bool startLocalServer(uint16_t port,
                          const std::string &worldDir,
                          bool useDefaultWorld,
                          const std::string &advertiseHost,
                          const std::string &communityUrl,
                          const std::string &communityLabel,
                          const std::string &logLevel,
                          std::string &error);
    bool launchLocalServer(LocalServerProcess &server, std::string &error);
    bool isPortInUse(uint16_t port, int ignoreId) const;

    bool visible = false;
    ImFont *regularFont = nullptr;
    ImFont *titleFont = nullptr;
    ImFont *headingFont = nullptr;
    ImFont *buttonFont = nullptr;
    ImVec4 regularColor{};
    ImVec4 titleColor{};
    ImVec4 headingColor{};
    ImVec4 buttonColor{};
    float regularFontSize = 0.0f;
    float titleFontSize = 0.0f;
    float headingFontSize = 0.0f;
    bool fontReloadRequested = false;
    bool keybindingsReloadRequested = false;

    std::array<char, 64> usernameBuffer{};
    std::array<char, 128> passwordBuffer{};
    std::array<char, 512> listUrlBuffer{};
    int lastCredentialsListIndex = -1;
    std::string storedPasswordHash;
    bool pendingQuitRequest = false;
    std::string errorDialogMessage;

    ThumbnailCache thumbnails;

    std::string userConfigPath;
    ui::BindingsModel bindingsModel;
    ui::BindingsController bindingsController{bindingsModel};
    ui::ConsoleModel consoleModel;
    ui::ConsoleController consoleController{consoleModel};
    ui::SettingsModel settingsModel;
    ui::SettingsController settingsController{settingsModel};
    int selectedLanguageIndex = 0;
    bool renderBrightnessDragging = false;
    std::function<void(const std::string &)> languageCallback;
    bool bindingsResetConfirmOpen = false;
    std::string activeTabKey;
    uint64_t lastConfigRevision = 0;

    struct LocalServerProcess {
        int id = 0;
        uint16_t port = 0;
        std::string worldDir;
        bool useDefaultWorld = false;
        std::string logLevel;
        std::string advertiseHost;
        std::string communityUrl;
        std::string communityLabel;
        std::string dataDir;
        std::string configPath;
        int pid = -1;
        int logFd = -1;
        std::thread logThread;
        std::mutex logMutex;
        std::string logBuffer;
        std::atomic<bool> running{false};
        int exitStatus = 0;
    };

    std::deque<std::unique_ptr<LocalServerProcess>> localServers;
    int nextLocalServerId = 1;
    int selectedLogServerId = -1;
    bool serverBinaryChecked = false;
    std::string serverBinaryPath;
    std::string serverStatusText;
    bool serverStatusIsError = false;
    std::array<char, 64> serverAdvertiseHostBuffer{};
    std::array<char, 128> serverWorldBuffer{};
    int serverPortInput = 11899;
    int serverLogLevelIndex = 2;
    int serverCommunityIndex = -1;
};

} // namespace ui

#include "ui/frontends/rmlui/console/panels/panel_start_server.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "karma/common/serialization/json.hpp"
#include <spdlog/spdlog.h>

#include "karma/common/data/path_resolver.hpp"
#include "ui/config/config.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ui {
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

std::string escapeRmlText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

constexpr const char *kLogLevels[] = {"trace", "debug", "info", "warn", "err", "critical", "off"};
constexpr int kLogLevelCount = static_cast<int>(sizeof(kLogLevels) / sizeof(kLogLevels[0]));

std::string formatExitStatus(int status) {
#if defined(_WIN32)
    return std::to_string(status);
#else
    if (WIFEXITED(status)) {
        return std::to_string(WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
        return std::string("signal ") + std::to_string(WTERMSIG(status));
    }
    return std::to_string(status);
#endif
}

std::string guessLocalIpAddress() {
#if defined(_WIN32)
    return {};
#else
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) {
        return {};
    }

    std::string fallback;
    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((ifa->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        const auto *addr = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
        char buffer[INET_ADDRSTRLEN]{};
        if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer))) {
            continue;
        }
        std::string ip(buffer);
        if (fallback.empty()) {
            fallback = ip;
        }
        if (ip.rfind("169.254.", 0) != 0) {
            freeifaddrs(ifaddr);
            return ip;
        }
    }

    freeifaddrs(ifaddr);
    return fallback;
#endif
}

void appendLog(std::string &logBuffer, const char *data, std::size_t length) {
    constexpr std::size_t kMaxLogBytes = 200000;
    logBuffer.append(data, length);
    if (logBuffer.size() > kMaxLogBytes) {
        const std::size_t trim = logBuffer.size() - kMaxLogBytes;
        logBuffer.erase(0, trim);
    }
}

std::string formatCommunityLabel(const ServerListOption &option) {
    if (!option.name.empty()) {
        return option.name;
    }
    if (!option.host.empty()) {
        return option.host;
    }
    return std::string("Unnamed community");
}

std::size_t hashCombine(std::size_t seed, std::size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}
} // namespace

class RmlUiPanelStartServer::StartServerListener final : public Rml::EventListener {
public:
    enum class Action {
        RefreshIp,
        AdvertiseChanged,
        Start,
        PortChanged,
        PortInc,
        PortDec,
        CommunityChanged,
        WorldChanged,
        WorldPickChanged,
        LogLevelChanged
    };

    StartServerListener(RmlUiPanelStartServer *panelIn, Action actionIn)
        : panel(panelIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        switch (action) {
            case Action::RefreshIp:
                panel->handleRefreshIp();
                break;
            case Action::AdvertiseChanged:
                panel->handleAdvertiseChanged();
                break;
            case Action::Start:
                panel->handleStartServer();
                break;
            case Action::PortChanged:
                panel->handlePortChanged();
                break;
            case Action::PortInc:
                panel->handlePortIncrement(1);
                break;
            case Action::PortDec:
                panel->handlePortIncrement(-1);
                break;
            case Action::CommunityChanged:
                panel->handleCommunityChanged();
                break;
            case Action::WorldChanged:
                panel->handleWorldChanged();
                break;
            case Action::WorldPickChanged:
                panel->handleWorldPickChanged();
                break;
            case Action::LogLevelChanged:
                panel->handleLogLevelChanged();
                break;
        }
    }

private:
    RmlUiPanelStartServer *panel = nullptr;
    Action action;
};

class RmlUiPanelStartServer::ServerRowListener final : public Rml::EventListener {
public:
    ServerRowListener(RmlUiPanelStartServer *panelIn, int serverIdIn)
        : panel(panelIn), serverId(serverIdIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (auto *target = event.GetTargetElement()) {
            if (target->HasAttribute("data-action")) {
                return;
            }
        }
        panel->handleSelectServer(serverId);
    }

private:
    RmlUiPanelStartServer *panel = nullptr;
    int serverId = -1;
};

class RmlUiPanelStartServer::ServerActionListener final : public Rml::EventListener {
public:
    ServerActionListener(RmlUiPanelStartServer *panelIn, int serverIdIn, std::string actionIn)
        : panel(panelIn), serverId(serverIdIn), action(std::move(actionIn)) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->handleServerAction(serverId, action);
        }
    }

private:
    RmlUiPanelStartServer *panel = nullptr;
    int serverId = -1;
    std::string action;
};

class RmlUiPanelStartServer::ServerLogLevelListener final : public Rml::EventListener {
public:
    ServerLogLevelListener(RmlUiPanelStartServer *panelIn, int serverIdIn)
        : panel(panelIn), serverId(serverIdIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        panel->handleServerLogLevel(serverId, -1);
    }

private:
    RmlUiPanelStartServer *panel = nullptr;
    int serverId = -1;
};

RmlUiPanelStartServer::RmlUiPanelStartServer()
    : RmlUiPanel("start-server", "client/ui/console_panel_start_server.rml") {}

RmlUiPanelStartServer::~RmlUiPanelStartServer() {
    stopAllLocalServers();
}

void RmlUiPanelStartServer::setConsoleModel(ConsoleModel *model) {
    consoleModel = model;
}

void RmlUiPanelStartServer::setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) {
    if (!consoleModel) {
        return;
    }
    consoleModel->community.listOptions = options;
    auto &listOptions = consoleModel->community.listOptions;
    if (listOptions.empty()) {
        consoleModel->community.listSelectedIndex = -1;
        serverCommunityIndex = -1;
    } else if (selectedIndex < 0) {
        consoleModel->community.listSelectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(listOptions.size())) {
        consoleModel->community.listSelectedIndex = static_cast<int>(listOptions.size()) - 1;
    } else {
        consoleModel->community.listSelectedIndex = selectedIndex;
    }

    if (serverCommunityIndex < 0 || serverCommunityIndex >= static_cast<int>(listOptions.size())) {
        serverCommunityIndex = consoleModel->community.listSelectedIndex;
    }
    updateCommunitySelect();
}

void RmlUiPanelStartServer::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    panelRoot = document->GetElementById("panel-start-server");
    warningText = document->GetElementById("start-server-warning");
    statusText = document->GetElementById("start-server-status");
    advertiseInput = document->GetElementById("server-advertise-host-input");
    portInput = document->GetElementById("server-port-input");
    communitySelect = document->GetElementById("server-community-select");
    communityEmptyText = document->GetElementById("server-community-empty");
    auto *dataDirInput = document->GetElementById("server-data-dir");
    if (dataDirInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(dataDirInput)) {
            input->SetValue(karma::common::data::DataRoot().string());
        }
    }
    worldInput = document->GetElementById("server-world-input");
    worldSelect = document->GetElementById("server-world-select");
    logLevelSelect = document->GetElementById("server-log-level-select");
    startButton = document->GetElementById("server-start-button");
    runningList = document->GetElementById("running-servers-list");
    logOutput = document->GetElementById("server-log-output");
    errorDialog.bind(document, "start-server-error-overlay", "start-server-error-message",
                     "start-server-error-ok");

    listeners.clear();
    if (advertiseInput) {
        auto refreshListener = std::make_unique<StartServerListener>(this, StartServerListener::Action::RefreshIp);
        if (auto *button = document->GetElementById("server-advertise-refresh")) {
            button->AddEventListener("click", refreshListener.get());
        }
        listeners.emplace_back(std::move(refreshListener));

        auto changeListener = std::make_unique<StartServerListener>(this, StartServerListener::Action::AdvertiseChanged);
        advertiseInput->AddEventListener("change", changeListener.get());
        advertiseInput->AddEventListener("blur", changeListener.get());
        listeners.emplace_back(std::move(changeListener));
    }

    if (startButton) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::Start);
        startButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    if (portInput) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::PortChanged);
        portInput->AddEventListener("change", listener.get());
        portInput->AddEventListener("blur", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (auto *decButton = document->GetElementById("server-port-dec")) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::PortDec);
        decButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (auto *incButton = document->GetElementById("server-port-inc")) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::PortInc);
        incButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    if (communitySelect) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::CommunityChanged);
        communitySelect->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    if (worldInput) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::WorldChanged);
        worldInput->AddEventListener("change", listener.get());
        worldInput->AddEventListener("blur", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    if (worldSelect) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::WorldPickChanged);
        worldSelect->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    if (logLevelSelect) {
        auto listener = std::make_unique<StartServerListener>(this, StartServerListener::Action::LogLevelChanged);
        logLevelSelect->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
    }

    ensureAdvertiseHost();
    if (portInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(portInput)) {
            input->SetValue(std::to_string(serverPortValue));
        }
    }

    updateWorldSelect();
    updateCommunitySelect();

    if (logLevelSelect) {
        auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(logLevelSelect);
        if (select) {
            select->RemoveAll();
            for (int i = 0; i < kLogLevelCount; ++i) {
                const auto *level = kLogLevels[i];
                select->Add(level, level);
            }
            select->SetSelection(serverLogLevelIndex);
        }
    }

    updateStatusText();
    updateServerList();
    updateLogOutput();
    errorDialog.setOnAccept([this]() { errorDialog.hide(); });
    errorDialog.setOnCancel([this]() { errorDialog.hide(); });
    errorDialog.installListeners(listeners);
}

void RmlUiPanelStartServer::onUpdate() {
    if (!panelRoot || panelRoot->IsClassSet("active") == false) {
        return;
    }
    updateServerList();
    updateLogOutput();
}

void RmlUiPanelStartServer::handleRefreshIp() {
    const std::string guessed = guessLocalIpAddress();
    if (!guessed.empty()) {
        advertiseHostValue = guessed;
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(advertiseInput)) {
            input->SetValue(guessed);
        }
    }
}

void RmlUiPanelStartServer::handleAdvertiseChanged() {
    if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(advertiseInput)) {
        advertiseHostValue = input->GetValue();
    }
}

void RmlUiPanelStartServer::handleStartServer() {
    static const std::vector<ServerListOption> kEmptyList;
    const auto &listOptions = consoleModel ? consoleModel->community.listOptions : kEmptyList;
    const std::string worldDir = trimCopy(worldPathValue);
    const std::string advertiseHost = trimCopy(advertiseHostValue);
    const bool useDefaultWorld = worldDir.empty();
    const std::string logLevel = (serverLogLevelIndex >= 0 && serverLogLevelIndex < kLogLevelCount)
        ? std::string(kLogLevels[serverLogLevelIndex])
        : std::string("info");
    const std::string communityUrl = (serverCommunityIndex >= 0 && serverCommunityIndex < static_cast<int>(listOptions.size()))
        ? listOptions[serverCommunityIndex].host
        : std::string();
    const std::string communityLabel = (serverCommunityIndex >= 0 && serverCommunityIndex < static_cast<int>(listOptions.size()))
        ? formatCommunityLabel(listOptions[serverCommunityIndex])
        : std::string();

    std::string error;
    if (!startLocalServer(static_cast<uint16_t>(serverPortValue),
                          worldDir,
                          useDefaultWorld,
                          advertiseHost,
                          communityUrl,
                          communityLabel,
                          logLevel,
                          error)) {
        serverStatusIsError = true;
        serverStatusText = error;
    } else {
        if (serverPortValue < 65535) {
            serverPortValue += 1;
            if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(portInput)) {
                input->SetValue(std::to_string(serverPortValue));
            }
        }
    }
    updateStatusText();
    updateServerList();
    updateLogOutput();
}

void RmlUiPanelStartServer::handlePortChanged() {
    if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(portInput)) {
        const std::string value = trimCopy(input->GetValue());
        int port = 0;
        try {
            port = std::stoi(value);
        } catch (...) {
            showPortError("Port must be a number between 1 and 65535.");
            input->SetValue(std::to_string(serverPortValue));
            return;
        }
        if (port < 1 || port > 65535) {
            showPortError("Port must be between 1 and 65535.");
            input->SetValue(std::to_string(serverPortValue));
            return;
        }
        serverPortValue = port;
        input->SetValue(std::to_string(serverPortValue));
    }
}

void RmlUiPanelStartServer::handlePortIncrement(int delta) {
    int next = serverPortValue + delta;
    if (next < 1 || next > 65535) {
        showPortError("Port must be between 1 and 65535.");
        return;
    }
    serverPortValue = next;
    if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(portInput)) {
        input->SetValue(std::to_string(serverPortValue));
    }
}

void RmlUiPanelStartServer::handleCommunityChanged() {
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(communitySelect);
    if (!select) {
        return;
    }
    serverCommunityIndex = select->GetSelection();
}

void RmlUiPanelStartServer::handleWorldChanged() {
    if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(worldInput)) {
        worldPathValue = input->GetValue();
    }
}

void RmlUiPanelStartServer::handleWorldPickChanged() {
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(worldSelect);
    if (!select) {
        return;
    }
    const int index = select->GetSelection();
    if (index < 0 || index >= static_cast<int>(worldChoices.size())) {
        return;
    }
    const std::string choice = worldChoices[static_cast<std::size_t>(index)];
    worldPathValue = choice;
    if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(worldInput)) {
        input->SetValue(choice);
    }
}

void RmlUiPanelStartServer::handleLogLevelChanged() {
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(logLevelSelect);
    if (!select) {
        return;
    }
    serverLogLevelIndex = select->GetSelection();
}

void RmlUiPanelStartServer::handleSelectServer(int serverId) {
    selectedLogServerId = serverId;
    updateServerList();
    updateLogOutput();
}

void RmlUiPanelStartServer::handleServerAction(int serverId, const std::string &action) {
    const auto index = findServerIndex(serverId);
    if (!index) {
        return;
    }
    if (action == "stop") {
        stopLocalServer(*index);
    } else if (action == "start") {
        std::string error;
        if (!launchLocalServer(*localServers[*index], error)) {
            serverStatusIsError = true;
            serverStatusText = error;
        } else {
            serverStatusIsError = false;
            serverStatusText.clear();
        }
    } else if (action == "remove") {
        stopLocalServer(*index);
        const int removedId = localServers[*index]->id;
        localServers.erase(localServers.begin() + static_cast<std::ptrdiff_t>(*index));
        if (selectedLogServerId == removedId) {
            selectedLogServerId = localServers.empty() ? -1 : localServers.front()->id;
        }
    }
    updateStatusText();
    updateServerList();
    updateLogOutput();
}

void RmlUiPanelStartServer::handleServerLogLevel(int serverId, int logIndex) {
    const auto index = findServerIndex(serverId);
    if (!index) {
        return;
    }
    auto *rowSelect = document ? document->GetElementById("loglevel-" + std::to_string(serverId)) : nullptr;
    if (!rowSelect) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(rowSelect);
    if (!select) {
        return;
    }
    if (logIndex < 0) {
        logIndex = select->GetSelection();
    }
    if (logIndex < 0 || logIndex >= kLogLevelCount) {
        logIndex = 2;
    }
    localServers[*index]->logLevel = kLogLevels[logIndex];
}

void RmlUiPanelStartServer::updateCommunitySelect() {
    if (!communitySelect) {
        return;
    }
    static const std::vector<ServerListOption> kEmptyList;
    const auto &listOptions = consoleModel ? consoleModel->community.listOptions : kEmptyList;
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(communitySelect);
    if (!select) {
        return;
    }
    select->RemoveAll();
    for (std::size_t i = 0; i < listOptions.size(); ++i) {
        const auto &opt = listOptions[i];
        const std::string label = formatCommunityLabel(opt);
        select->Add(label, std::to_string(i));
    }
    if (!listOptions.empty()) {
        if (serverCommunityIndex < 0 || serverCommunityIndex >= static_cast<int>(listOptions.size())) {
            serverCommunityIndex = 0;
        }
        select->SetSelection(serverCommunityIndex);
        if (communityEmptyText) {
            communityEmptyText->SetClass("hidden", true);
        }
        communitySelect->SetClass("hidden", false);
    } else {
        if (communityEmptyText) {
            communityEmptyText->SetClass("hidden", false);
        }
        communitySelect->SetClass("hidden", true);
    }
}

void RmlUiPanelStartServer::updateWorldSelect() {
    worldChoices.clear();
    worldChoices.emplace_back("");

    std::error_code ec;
    const auto addDirectoryEntries = [&](const std::filesystem::path &basePath) {
            if (!std::filesystem::exists(basePath, ec) || !std::filesystem::is_directory(basePath, ec)) {
            return;
        }
        for (const auto &entry : std::filesystem::directory_iterator(basePath, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_directory(ec)) {
                continue;
            }
            worldChoices.push_back(entry.path().string());
        }
    };

    addDirectoryEntries(karma::common::data::EnsureUserWorldsDirectory());
    addDirectoryEntries(karma::common::data::Resolve("server/worlds"));

    if (!worldSelect) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(worldSelect);
    if (!select) {
        return;
    }
    select->RemoveAll();
    select->Add("Use default world", "default");
    for (std::size_t i = 1; i < worldChoices.size(); ++i) {
        select->Add(worldChoices[i], std::to_string(i));
    }
}

void RmlUiPanelStartServer::updateServerList() {
    if (!runningList || !document) {
        return;
    }

    std::size_t signature = localServers.size();
    signature = hashCombine(signature, static_cast<std::size_t>(selectedLogServerId));
    for (const auto &server : localServers) {
        if (!server) {
            continue;
        }
        signature = hashCombine(signature, static_cast<std::size_t>(server->id));
        signature = hashCombine(signature, static_cast<std::size_t>(server->running.load()));
        signature = hashCombine(signature, static_cast<std::size_t>(server->port));
        signature = hashCombine(signature, static_cast<std::size_t>(server->exitStatus));
    }
    if (signature == lastListSignature) {
        return;
    }
    lastListSignature = signature;

    runningList->SetInnerRML("");
    dynamicListeners.clear();

    auto appendElement = [&](Rml::Element *parent, const char *tag) -> Rml::Element* {
        auto child = document->CreateElement(tag);
        Rml::Element *ptr = child.get();
        parent->AppendChild(std::move(child));
        return ptr;
    };

    if (localServers.empty()) {
        auto *row = appendElement(runningList, "div");
        row->SetClass("server-row", true);
        auto *cell = appendElement(row, "div");
        cell->SetClass("server-cell", true);
        cell->SetInnerRML("No servers running.");
        return;
    }

    for (const auto &serverPtr : localServers) {
        if (!serverPtr) {
            continue;
        }
        const auto &server = *serverPtr;
        auto *row = appendElement(runningList, "div");
        row->SetClass("server-row", true);
        if (server.id == selectedLogServerId) {
            row->SetClass("selected", true);
        }

        auto rowListener = std::make_unique<ServerRowListener>(this, server.id);
        row->AddEventListener("click", rowListener.get());
        dynamicListeners.emplace_back(std::move(rowListener));

        auto makeCell = [&](const std::string &cls, const std::string &text) {
            auto *cell = appendElement(row, "div");
            cell->SetClass("server-cell", true);
            if (!cls.empty()) {
                cell->SetClass(cls, true);
            }
            cell->SetInnerRML(escapeRmlText(text));
            return cell;
        };

        makeCell("port", std::to_string(server.port));
        makeCell("world", server.useDefaultWorld ? "Default World" : (server.worldDir.empty() ? "(unset)" : server.worldDir));

        auto *statusCell = appendElement(row, "div");
        statusCell->SetClass("server-cell", true);
        statusCell->SetClass("status-col", true);
        if (server.running.load()) {
            statusCell->SetClass("status-running", true);
            statusCell->SetInnerRML("Running");
        } else {
            statusCell->SetClass("status-stopped", true);
            std::string status = "Stopped";
            if (server.exitStatus != 0) {
                status += " (" + formatExitStatus(server.exitStatus) + ")";
            }
            statusCell->SetInnerRML(escapeRmlText(status));
        }
        auto *actions = appendElement(row, "div");
        actions->SetClass("server-cell", true);
        actions->SetClass("actions", true);
        if (server.running.load()) {
            auto button = document->CreateElement("button");
            auto *buttonPtr = button.get();
            button->SetInnerRML("Stop");
            button->SetClass("danger", true);
            button->SetAttribute("data-action", "stop");
            auto listener = std::make_unique<ServerActionListener>(this, server.id, "stop");
            buttonPtr->AddEventListener("click", listener.get());
            dynamicListeners.emplace_back(std::move(listener));
            actions->AppendChild(std::move(button));
        } else {
            auto start = document->CreateElement("button");
            auto *startPtr = start.get();
            start->SetInnerRML("Start");
            start->SetClass("primary", true);
            start->SetAttribute("data-action", "start");
            auto startListener = std::make_unique<ServerActionListener>(this, server.id, "start");
            startPtr->AddEventListener("click", startListener.get());
            dynamicListeners.emplace_back(std::move(startListener));
            actions->AppendChild(std::move(start));

            auto remove = document->CreateElement("button");
            auto *removePtr = remove.get();
            remove->SetInnerRML("Remove");
            remove->SetClass("danger", true);
            remove->SetAttribute("data-action", "remove");
            auto removeListener = std::make_unique<ServerActionListener>(this, server.id, "remove");
            removePtr->AddEventListener("click", removeListener.get());
            dynamicListeners.emplace_back(std::move(removeListener));
            actions->AppendChild(std::move(remove));
        }
    }
}

void RmlUiPanelStartServer::updateLogOutput() {
    if (!logOutput) {
        return;
    }
    if (selectedLogServerId < 0) {
        const std::string emptyText = localServers.empty()
            ? "No servers running."
            : "Select a server to view its log output.";
        if (lastLogSnapshot != emptyText) {
            logOutput->SetInnerRML(escapeRmlText(emptyText));
            lastLogSnapshot = emptyText;
        }
        return;
    }

    LocalServerProcess *selected = nullptr;
    for (auto &serverPtr : localServers) {
        if (serverPtr && serverPtr->id == selectedLogServerId) {
            selected = serverPtr.get();
            break;
        }
    }
    if (!selected) {
        const std::string text = "Selected server is no longer available.";
        if (lastLogSnapshot != text) {
            logOutput->SetInnerRML(escapeRmlText(text));
            lastLogSnapshot = text;
        }
        return;
    }

    std::string snapshot;
    {
        std::lock_guard<std::mutex> lock(selected->logMutex);
        snapshot = selected->logBuffer;
    }
    if (snapshot.empty()) {
        snapshot = "(no output yet)";
    }
    if (snapshot != lastLogSnapshot) {
        logOutput->SetInnerRML(escapeRmlText(snapshot));
        lastLogSnapshot = snapshot;
    }
}

void RmlUiPanelStartServer::updateStatusText() {
    const std::string serverBinary = findServerBinary();
    if (warningText) {
        warningText->SetClass("hidden", !serverBinary.empty());
    }
    if (!statusText) {
        return;
    }
    if (serverStatusText.empty()) {
        statusText->SetClass("hidden", true);
        return;
    }
    statusText->SetInnerRML(escapeRmlText(serverStatusText));
    statusText->SetClass("hidden", false);
    statusText->SetClass("status-error", serverStatusIsError);
}

void RmlUiPanelStartServer::showPortError(const std::string &message) {
    errorDialog.show(escapeRmlText(message));
}

bool RmlUiPanelStartServer::ensureAdvertiseHost() {
    if (!advertiseHostValue.empty()) {
        return true;
    }
    std::string advertiseHost = ui::config::GetRequiredString("network.ServerAdvertiseHost");
    if (advertiseHost.empty()) {
        advertiseHost = guessLocalIpAddress();
    }
    if (!advertiseHost.empty()) {
        advertiseHostValue = advertiseHost;
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(advertiseInput)) {
            input->SetValue(advertiseHost);
        }
        return true;
    }
    return false;
}

void RmlUiPanelStartServer::stopLocalServer(std::size_t index) {
    if (index >= localServers.size()) {
        return;
    }

    auto &server = *localServers[index];
    if (!server.running.load()) {
        if (server.logThread.joinable()) {
            server.logThread.join();
        }
        return;
    }

#if defined(_WIN32)
    server.running.store(false);
#else
    if (server.pid > 0) {
        kill(server.pid, SIGTERM);
    }
#endif

    if (server.logThread.joinable()) {
        server.logThread.join();
    }
}

void RmlUiPanelStartServer::stopAllLocalServers() {
    for (std::size_t i = 0; i < localServers.size(); ++i) {
        stopLocalServer(i);
    }
}

std::string RmlUiPanelStartServer::findServerBinary() {
    if (serverBinaryChecked) {
        return serverBinaryPath;
    }
    serverBinaryChecked = true;

    std::error_code ec;
    const auto root = karma::common::data::ExecutableDirectory();

    auto isExecutable = [](const std::filesystem::path &path) {
        std::error_code localEc;
        if (!std::filesystem::exists(path, localEc)) {
            return false;
        }
#if defined(_WIN32)
        return true;
#else
        auto perms = std::filesystem::status(path, localEc).permissions();
        if (localEc) {
            return false;
        }
        return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
            || (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none
            || (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
#endif
    };

    if (!root.empty()) {
        const auto candidate = root / "bz3-server";
        if (isExecutable(candidate)) {
            serverBinaryPath = candidate.string();
            return serverBinaryPath;
        }
#if defined(_WIN32)
        const auto candidateExe = root / "bz3-server.exe";
        if (isExecutable(candidateExe)) {
            serverBinaryPath = candidateExe.string();
            return serverBinaryPath;
        }
#endif
    }

    serverBinaryPath.clear();
    return serverBinaryPath;
}

bool RmlUiPanelStartServer::startLocalServer(uint16_t port,
                                             const std::string &worldDir,
                                             bool useDefaultWorld,
                                             const std::string &advertiseHost,
                                             const std::string &communityUrl,
                                             const std::string &communityLabel,
                                             const std::string &logLevel,
                                             std::string &error) {
    error.clear();

#if defined(_WIN32)
    error = "Local server launch is not supported on Windows yet.";
    return false;
#else
    if (isPortInUse(port, -1)) {
        error = "Port is already in use by a server in the list.";
        return false;
    }

    auto server = std::make_unique<LocalServerProcess>();
    server->id = nextLocalServerId++;
    server->port = port;
    server->worldDir = worldDir;
    server->useDefaultWorld = useDefaultWorld;
    server->logLevel = logLevel;
    server->advertiseHost = advertiseHost;
    if (communityLabel == "Local Area Network") {
        server->communityUrl.clear();
        server->communityLabel = communityLabel;
    } else {
        server->communityUrl = communityUrl;
        server->communityLabel = communityLabel;
    }
    server->dataDir = karma::common::data::DataRoot().string();

    if (!launchLocalServer(*server, error)) {
        return false;
    }

    localServers.push_back(std::move(server));
    selectedLogServerId = localServers.back()->id;

    serverStatusIsError = false;
    serverStatusText.clear();
    return true;
#endif
}

bool RmlUiPanelStartServer::isPortInUse(uint16_t port, int ignoreId) const {
    if (port == 0) {
        return true;
    }
    for (const auto &serverPtr : localServers) {
        if (!serverPtr) {
            continue;
        }
        if (serverPtr->id == ignoreId) {
            continue;
        }
        if (serverPtr->port == port && serverPtr->running.load()) {
            return true;
        }
    }
#if !defined(_WIN32)
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return true;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    const int bindResult = ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    ::close(fd);
    if (bindResult != 0) {
        return true;
    }
#endif
    return false;
}

bool RmlUiPanelStartServer::launchLocalServer(LocalServerProcess &server, std::string &error) {
    error.clear();

    if (isPortInUse(server.port, server.id)) {
        error = "Port is already in use by another running server.";
        return false;
    }

    const std::string serverBinary = findServerBinary();
    if (serverBinary.empty()) {
        error = "bz3-server binary not found. Build it first or run from the project tree.";
        return false;
    }

    server.configPath.clear();
    if (!server.advertiseHost.empty()) {
        const auto configDir = karma::common::data::UserConfigDirectory() / "server" / "instances";
        std::error_code dirEc;
        std::filesystem::create_directories(configDir, dirEc);
        if (dirEc) {
            error = "Failed to create config directory: " + dirEc.message();
            return false;
        }

        std::ostringstream name;
        name << "local_server_" << server.port << "_" << server.id << ".json";
        const auto configFile = configDir / name.str();

        karma::common::serialization::Value configJson;
        configJson["network"]["ServerAdvertiseHost"] = server.advertiseHost;
        std::ofstream out(configFile);
        if (!out) {
            error = "Failed to write config override file.";
            return false;
        }
        out << configJson.dump(2) << "\n";
        if (!out) {
            error = "Failed to write config override file.";
            return false;
        }
        server.configPath = configFile.string();
    }

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        error = std::string("Failed to create log pipe: ") + std::strerror(errno);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        error = std::string("Failed to fork server process: ") + std::strerror(errno);
        close(pipeFds[0]);
        close(pipeFds[1]);
        return false;
    }

    if (pid == 0) {
        dup2(pipeFds[1], STDOUT_FILENO);
        dup2(pipeFds[1], STDERR_FILENO);
        close(pipeFds[0]);
        close(pipeFds[1]);

        std::vector<std::string> args;
        args.push_back(serverBinary);
        args.push_back("-p");
        args.push_back(std::to_string(server.port));
        if (!server.logLevel.empty()) {
            args.push_back("-L");
            args.push_back(server.logLevel);
        }
        if (!server.configPath.empty()) {
            args.push_back("-c");
            args.push_back(server.configPath);
        }
        if (!server.communityUrl.empty()) {
            args.push_back("-C");
            args.push_back(server.communityUrl);
        }
        if (!server.dataDir.empty()) {
            args.push_back("-d");
            args.push_back(server.dataDir);
        }
        if (server.useDefaultWorld) {
            args.push_back("-D");
        } else {
            args.push_back("-w");
            args.push_back(server.worldDir);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto &arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        execv(serverBinary.c_str(), argv.data());
        _exit(1);
    }

    close(pipeFds[1]);

    server.pid = static_cast<int>(pid);
    server.logFd = pipeFds[0];
    server.exitStatus = 0;
    server.running.store(true);
    {
        std::lock_guard<std::mutex> lock(server.logMutex);
        server.logBuffer.clear();
    }

    if (server.logThread.joinable()) {
        server.logThread.join();
    }

    server.logThread = std::thread([&server]() {
        char buffer[4096];
        while (true) {
            const ssize_t count = ::read(server.logFd, buffer, sizeof(buffer));
            if (count > 0) {
                std::lock_guard<std::mutex> lock(server.logMutex);
                appendLog(server.logBuffer, buffer, static_cast<std::size_t>(count));
                continue;
            }
            if (count == 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        close(server.logFd);
        server.logFd = -1;
        int status = 0;
        if (server.pid > 0) {
            waitpid(static_cast<pid_t>(server.pid), &status, 0);
            server.exitStatus = status;
        }
        server.running.store(false);
    });

    return true;
}

std::optional<std::size_t> RmlUiPanelStartServer::findServerIndex(int serverId) const {
    for (std::size_t i = 0; i < localServers.size(); ++i) {
        if (localServers[i] && localServers[i]->id == serverId) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace ui

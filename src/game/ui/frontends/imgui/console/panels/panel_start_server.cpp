#include "ui/frontends/imgui/console/console.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <imgui.h>
#include "karma/common/json.hpp"
#include <spdlog/spdlog.h>
#include <optional>

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/i18n.hpp"
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
} // namespace

namespace ui {

void ConsoleView::stopLocalServer(std::size_t index) {
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

void ConsoleView::stopAllLocalServers() {
    for (std::size_t i = 0; i < localServers.size(); ++i) {
        stopLocalServer(i);
    }
}

std::string ConsoleView::findServerBinary() {
    if (serverBinaryChecked) {
        return serverBinaryPath;
    }
    serverBinaryChecked = true;

    std::error_code ec;
    const auto dataRoot = karma::data::DataRoot();
    const auto root = dataRoot.parent_path();

    auto isExecutable = [](const std::filesystem::path &path) {
        std::error_code localEc;
        if (!std::filesystem::exists(path, localEc)) {
            return false;
        }
#if defined(_WIN32)
        return true;
#else
        return ::access(path.c_str(), X_OK) == 0;
#endif
    };

    std::vector<std::filesystem::path> candidates = {
        root / "bz3-server",
        root / "build" / "bz3-server",
        root / "build" / "Debug" / "bz3-server",
        root / "build" / "Release" / "bz3-server"
    };

    for (const auto &candidate : candidates) {
        if (isExecutable(candidate)) {
            serverBinaryPath = candidate.string();
            return serverBinaryPath;
        }
    }

    std::vector<std::filesystem::path> searchDirs = {
        root,
        std::filesystem::current_path(ec)
    };

    for (const auto &dir : searchDirs) {
        if (dir.empty() || !std::filesystem::exists(dir, ec)) {
            continue;
        }
        std::error_code iterEc;
        for (auto it = std::filesystem::recursive_directory_iterator(dir, iterEc);
             it != std::filesystem::recursive_directory_iterator();
             ++it) {
            if (iterEc) {
                break;
            }
            if (it.depth() > 3) {
                it.disable_recursion_pending();
                continue;
            }
            const auto &path = it->path();
            if (path.filename() == "bz3-server" && isExecutable(path)) {
                serverBinaryPath = path.string();
                return serverBinaryPath;
            }
        }
    }

    serverBinaryPath.clear();
    return serverBinaryPath;
}

bool ConsoleView::startLocalServer(uint16_t port,
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
    server->dataDir = karma::data::DataRoot().string();

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

bool ConsoleView::isPortInUse(uint16_t port, int ignoreId) const {
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

bool ConsoleView::launchLocalServer(LocalServerProcess &server, std::string &error) {
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
        const auto configDir = karma::data::UserConfigDirectory() / "server" / "instances";
        std::error_code dirEc;
        std::filesystem::create_directories(configDir, dirEc);
        if (dirEc) {
            error = "Failed to create config directory: " + dirEc.message();
            return false;
        }

        std::ostringstream name;
        name << "local_server_" << server.port << "_" << server.id << ".json";
        const auto configFile = configDir / name.str();

        karma::json::Value configJson;
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

void ConsoleView::drawStartServerPanel(const MessageColors &colors) {
    auto &i18n = karma::i18n::Get();

    const std::string serverBinary = findServerBinary();
    if (serverBinary.empty()) {
        ImGui::TextColored(colors.error, "%s", i18n.get("ui.console.start_server.server_binary_missing").c_str());
        ImGui::Spacing();
    }

    if (serverAdvertiseHostBuffer[0] == '\0') {
        std::string advertiseHost = ui::config::GetRequiredString("network.ServerAdvertiseHost");
        if (advertiseHost.empty()) {
            advertiseHost = guessLocalIpAddress();
        }
        if (!advertiseHost.empty()) {
            std::snprintf(serverAdvertiseHostBuffer.data(), serverAdvertiseHostBuffer.size(), "%s", advertiseHost.c_str());
        }
    }

    auto formatCommunityLabel = [](const ServerListOption &option) {
        if (!option.name.empty()) {
            return option.name;
        }
        if (!option.host.empty()) {
            return option.host;
        }
        return std::string("Unnamed community");
    };

    const ImGuiStyle &style = ImGui::GetStyle();
    const float ipWidth = 120.0f;
    const float portWidth = 90.0f;
    const float loggingWidth = 100.0f;
    const float actionWidth = 120.0f;
    auto &listOptions = consoleModel.community.listOptions;

    static const char *kLogLevels[] = {"trace", "debug", "info", "warn", "err", "critical", "off"};
    constexpr int kLogLevelCount = static_cast<int>(sizeof(kLogLevels) / sizeof(kLogLevels[0]));
    if (serverLogLevelIndex < 0 || serverLogLevelIndex >= kLogLevelCount) {
        serverLogLevelIndex = 2;
    }

    ImGui::TextUnformatted(i18n.get("ui.console.start_server.new_server").c_str());
    if (ImGui::BeginTable("NewServerForm", 6, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter)) {
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.ip_address").c_str(), ImGuiTableColumnFlags_WidthFixed, ipWidth);
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.port").c_str(), ImGuiTableColumnFlags_WidthFixed, portWidth);
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.community").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.world_directory").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.logging").c_str(), ImGuiTableColumnFlags_WidthFixed, loggingWidth);
        ImGui::TableSetupColumn(i18n.get("ui.console.start_server.action").c_str(), ImGuiTableColumnFlags_WidthFixed, actionWidth);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##AdvertiseHost", serverAdvertiseHostBuffer.data(), serverAdvertiseHostBuffer.size());
        ImGui::SameLine();
        if (ImGui::Button("R")) {
            const std::string guessed = guessLocalIpAddress();
            if (!guessed.empty()) {
                std::snprintf(serverAdvertiseHostBuffer.data(), serverAdvertiseHostBuffer.size(), "%s", guessed.c_str());
            }
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputInt("##ServerPort", &serverPortInput)) {
            if (serverPortInput < 1) {
                serverPortInput = 1;
            } else if (serverPortInput > 65535) {
                serverPortInput = 65535;
            }
        }

        ImGui::TableSetColumnIndex(2);
        if (listOptions.empty()) {
            ImGui::TextDisabled("No communities");
        } else {
            if (serverCommunityIndex < 0 || serverCommunityIndex >= static_cast<int>(listOptions.size())) {
                serverCommunityIndex = 0;
            }
            const auto &currentCommunity = listOptions[serverCommunityIndex];
            const std::string communityLabel = formatCommunityLabel(currentCommunity);
            if (ImGui::BeginCombo("##ServerCommunity", communityLabel.c_str())) {
                for (int i = 0; i < static_cast<int>(listOptions.size()); ++i) {
                    const auto &option = listOptions[i];
                    const std::string optionLabel = formatCommunityLabel(option);
                    const bool selected = (i == serverCommunityIndex);
                    if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                        serverCommunityIndex = i;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##WorldDir", serverWorldBuffer.data(), serverWorldBuffer.size());
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            ImGui::OpenPopup("WorldDirPicker");
        }

        if (ImGui::BeginPopup("WorldDirPicker")) {
            ImGui::TextUnformatted("World directories");
            ImGui::Separator();

            if (ImGui::Selectable("Use default world")) {
                serverWorldBuffer.fill(0);
                ImGui::CloseCurrentPopup();
            }

            const auto addDirectoryEntries = [&](const std::filesystem::path &basePath) {
                std::error_code ec;
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
                    const std::string fullPath = entry.path().string();
                    if (ImGui::Selectable(fullPath.c_str())) {
                        std::snprintf(serverWorldBuffer.data(), serverWorldBuffer.size(), "%s", fullPath.c_str());
                        ImGui::CloseCurrentPopup();
                        break;
                    }
                }
            };

            addDirectoryEntries(karma::data::EnsureUserWorldsDirectory());
            addDirectoryEntries(karma::data::Resolve("server/worlds"));

            ImGui::EndPopup();
        }

        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##ServerLogLevel", &serverLogLevelIndex, kLogLevels, kLogLevelCount);

        ImGui::TableSetColumnIndex(5);
        if (ImGui::Button(i18n.get("ui.console.start_server.start_button").c_str())) {
            const std::string worldDir = trimCopy(serverWorldBuffer.data());
            const std::string advertiseHost = trimCopy(serverAdvertiseHostBuffer.data());
            const bool useDefaultWorld = worldDir.empty();
            const std::string logLevel = kLogLevels[serverLogLevelIndex];
            const std::string communityUrl = (serverCommunityIndex >= 0 && serverCommunityIndex < static_cast<int>(listOptions.size()))
                ? listOptions[serverCommunityIndex].host
                : std::string();
            const std::string communityLabel = (serverCommunityIndex >= 0 && serverCommunityIndex < static_cast<int>(listOptions.size()))
                ? formatCommunityLabel(listOptions[serverCommunityIndex])
                : std::string();
            std::string error;
            if (!startLocalServer(static_cast<uint16_t>(serverPortInput),
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
                if (serverPortInput < 65535) {
                    serverPortInput += 1;
                }
            }
        }

        ImGui::EndTable();
    }

    if (!serverStatusText.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(serverStatusIsError ? colors.error : colors.notice, "%s", serverStatusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float runningHeight = std::max(160.0f, availableHeight * 0.44f);
    const float logHeight = std::max(200.0f, availableHeight * 0.56f);

    if (ImGui::BeginChild("RunningServersPane", ImVec2(0, runningHeight), false)) {
        ImGui::TextUnformatted("Running Servers");

        if (localServers.empty()) {
            ImGui::TextDisabled("No servers running.");
        } else {
            std::optional<std::size_t> removeIndex;
            if (ImGui::BeginTable("LocalServerTable", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0, -1.0f))) {
                ImGui::TableSetupColumn("Community", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("IP Address", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Logging", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 160.0f);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < localServers.size(); ++index) {
                    auto &server = *localServers[index];
                    ImGui::TableNextRow();
                    const bool rowSelected = (selectedLogServerId == server.id);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(server.id);
                    const std::string communityLabel = server.communityLabel.empty()
                        ? (server.communityUrl.empty() ? "-" : server.communityUrl)
                        : server.communityLabel;
                    if (ImGui::Selectable(communityLabel.c_str(), rowSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                        selectedLogServerId = server.id;
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(server.advertiseHost.empty() ? "-" : server.advertiseHost.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", static_cast<unsigned int>(server.port));

                    ImGui::TableSetColumnIndex(3);
                    if (server.useDefaultWorld) {
                        ImGui::TextUnformatted("Default World");
                    } else {
                        ImGui::TextUnformatted(server.worldDir.empty() ? "(unset)" : server.worldDir.c_str());
                    }

                    ImGui::TableSetColumnIndex(4);
                    if (server.running.load()) {
                        const char *label = server.logLevel.empty() ? "info" : server.logLevel.c_str();
                        ImGui::TextUnformatted(label);
                    } else {
                        int logIndex = server.logLevel.empty() ? 2 : -1;
                        if (!server.logLevel.empty()) {
                            for (int i = 0; i < kLogLevelCount; ++i) {
                                if (server.logLevel == kLogLevels[i]) {
                                    logIndex = i;
                                    break;
                                }
                            }
                        }
                        if (logIndex < 0) {
                            logIndex = 2;
                        }
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::Combo("##ServerLogLevelRow", &logIndex, kLogLevels, kLogLevelCount)) {
                            server.logLevel = kLogLevels[logIndex];
                        }
                    }

                    ImGui::TableSetColumnIndex(5);
                    if (server.running.load()) {
                        ImGui::TextColored(colors.action, "Running");
                    } else {
                        std::string status = "Stopped";
                        if (server.exitStatus != 0) {
                            status += " (" + formatExitStatus(server.exitStatus) + ")";
                        }
                        ImGui::TextColored(colors.notice, "%s", status.c_str());
                    }

                    ImGui::TableSetColumnIndex(6);
                    if (server.running.load()) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.38f, 0.38f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.22f, 0.22f, 1.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 1.0f));
                        if (ImGui::Button("Stop")) {
                            selectedLogServerId = server.id;
                            stopLocalServer(index);
                        }
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.70f, 0.35f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.78f, 0.40f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.60f, 0.30f, 1.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 1.0f));
                        if (ImGui::Button(i18n.get("ui.console.start_server.start_button").c_str())) {
                            selectedLogServerId = server.id;
                            std::string error;
                            if (!launchLocalServer(server, error)) {
                                serverStatusIsError = true;
                                serverStatusText = error;
                            } else {
                                serverStatusIsError = false;
                                serverStatusText.clear();
                            }
                        }
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 1.0f));
                        if (ImGui::Button("Remove")) {
                            stopLocalServer(index);
                            removeIndex = index;
                        }
                        ImGui::PopStyleVar();
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            if (removeIndex) {
                const int removedId = localServers[*removeIndex]->id;
                localServers.erase(localServers.begin() + static_cast<std::ptrdiff_t>(*removeIndex));
                if (selectedLogServerId == removedId) {
                    selectedLogServerId = localServers.empty() ? -1 : localServers.front()->id;
                }
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();

    if (ImGui::BeginChild("LogOutputPane", ImVec2(0, logHeight), false)) {
        ImGui::TextUnformatted("Log Output");
        if (selectedLogServerId < 0) {
            const char *emptyText = localServers.empty()
                ? "No servers running."
                : "Select a server to view its log output.";
            ImGui::TextDisabled("%s", emptyText);
            ImGui::EndChild();
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
            ImGui::TextDisabled("Selected server is no longer available.");
            ImGui::EndChild();
            return;
        }

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(selected->logMutex);
            snapshot = selected->logBuffer;
        }

        ImGui::BeginChild("ServerLogOutput", ImVec2(0, -1.0f), true);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(snapshot.empty() ? "(no output yet)" : snapshot.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::EndChild();
    }
}


} // namespace ui

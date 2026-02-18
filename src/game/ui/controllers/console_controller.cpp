#include "ui/controllers/console_controller.hpp"

#include "ui/config/ui_config.hpp"

namespace ui {

ConsoleController::ConsoleController(ConsoleModel &modelIn)
    : model(modelIn) {}

std::string ConsoleController::communityKeyForIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(model.community.listOptions.size())) {
        return {};
    }
    const auto &option = model.community.listOptions[static_cast<std::size_t>(index)];
    if (option.name == "Local Area Network") {
        return "LAN";
    }
    std::string host = option.host;
    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }
    return host;
}

ConsoleController::Credentials ConsoleController::loadCommunityCredentials(int listIndex) const {
    Credentials out;
    const std::string key = communityKeyForIndex(listIndex);
    if (key.empty()) {
        return out;
    }

    const auto *credsIt = ui::UiConfig::GetCommunityCredentials();
    if (!credsIt || !credsIt->is_object()) {
        return out;
    }
    const auto entryIt = credsIt->find(key);
    if (entryIt == credsIt->end() || !entryIt->is_object()) {
        return out;
    }
    const auto &entry = *entryIt;
    if (auto userIt = entry.find("username"); userIt != entry.end() && userIt->is_string()) {
        out.username = userIt->get<std::string>();
    }
    if (key != "LAN") {
        if (auto passIt = entry.find("passwordHash"); passIt != entry.end() && passIt->is_string()) {
            out.storedPasswordHash = passIt->get<std::string>();
        }
    }
    return out;
}

ConsoleController::PersistResult ConsoleController::persistCommunityCredentials(int listIndex,
                                                                               const std::string &username,
                                                                               const std::string &storedPasswordHash,
                                                                               bool passwordChanged) const {
    PersistResult result;
    const std::string key = communityKeyForIndex(listIndex);
    if (key.empty()) {
        return result;
    }

    karma::common::serialization::Value creds = karma::common::serialization::Object();
    if (const auto *existing = ui::UiConfig::GetCommunityCredentials()) {
        if (existing->is_object()) {
            creds = *existing;
        }
    }

    if (username.empty()) {
        creds.erase(key);
    } else {
        if (!creds.contains(key) || !creds[key].is_object()) {
            creds[key] = karma::common::serialization::Object();
        }
        creds[key]["username"] = username;
        if (key == "LAN") {
            if (creds[key].is_object()) {
                creds[key].erase("passwordHash");
                creds[key].erase("salt");
            }
        } else if (!storedPasswordHash.empty()) {
            creds[key]["passwordHash"] = storedPasswordHash;
        } else if (passwordChanged) {
            if (creds[key].is_object()) {
                creds[key].erase("passwordHash");
            }
            result.clearStoredPasswordHash = true;
        }
    }

    if (creds.empty()) {
        ui::UiConfig::EraseCommunityCredentials();
    } else {
        ui::UiConfig::SetCommunityCredentials(creds);
    }
    return result;
}

void ConsoleController::queueSelection(const CommunityBrowserSelection &selection) {
    pendingSelection = selection;
}

void ConsoleController::queueListSelection(int index) {
    pendingListSelection = index;
}

void ConsoleController::queueNewListRequest(const ServerListOption &option) {
    pendingNewList = option;
}

void ConsoleController::queueDeleteListRequest(const std::string &host) {
    pendingDeleteListHost = host;
}

void ConsoleController::requestRefresh() {
    refreshRequested = true;
}

void ConsoleController::clearPending() {
    pendingSelection.reset();
    pendingListSelection.reset();
    pendingNewList.reset();
    pendingDeleteListHost.reset();
    refreshRequested = false;
}

std::optional<CommunityBrowserSelection> ConsoleController::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }
    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> ConsoleController::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }
    auto selection = pendingListSelection;
    pendingListSelection.reset();
    return selection;
}

std::optional<ServerListOption> ConsoleController::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }
    auto request = pendingNewList;
    pendingNewList.reset();
    return request;
}

std::optional<std::string> ConsoleController::consumeDeleteListRequest() {
    if (!pendingDeleteListHost.has_value()) {
        return std::nullopt;
    }
    auto host = pendingDeleteListHost;
    pendingDeleteListHost.reset();
    return host;
}

bool ConsoleController::consumeRefreshRequest() {
    if (!refreshRequested) {
        return false;
    }
    refreshRequested = false;
    return true;
}

} // namespace ui

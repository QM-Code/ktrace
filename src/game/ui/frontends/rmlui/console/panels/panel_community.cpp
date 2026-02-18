#include "ui/frontends/rmlui/console/panels/panel_community.hpp"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string_view>

#include <md4c.h>

#include "karma/common/serialization/json.hpp"
#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

#include "karma/common/data/path_resolver.hpp"
#include "ui/frontends/rmlui/console/emoji_utils.hpp"
#include "karma/common/logging/logging.hpp"
#include "spdlog/spdlog.h"

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
            default: out.push_back(ch); break;
        }
    }
    return out;
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

struct MarkdownContext {
    std::string out;
    std::vector<std::string> linkStack;
    struct ListState {
        bool ordered = false;
        int index = 0;
        int start = 1;
    };
    std::vector<ListState> listStack;
};

int mdEnterBlock(MD_BLOCKTYPE type, void *detail, void *userdata) {
    auto *ctx = static_cast<MarkdownContext *>(userdata);
    if (!ctx) {
        return 0;
    }
    switch (type) {
        case MD_BLOCK_P:
            ctx->out.append("<div class=\"md-paragraph\">");
            break;
        case MD_BLOCK_H: {
            auto *h = static_cast<MD_BLOCK_H_DETAIL *>(detail);
            int level = h ? static_cast<int>(h->level) : 1;
            if (level < 1) {
                level = 1;
            }
            if (level > 6) {
                level = 6;
            }
            ctx->out.append("<div class=\"md-heading md-h");
            ctx->out.append(std::to_string(level));
            ctx->out.append("\">");
            break;
        }
        case MD_BLOCK_TABLE:
            ctx->out.append("<table class=\"md-table\">");
            break;
        case MD_BLOCK_THEAD:
            ctx->out.append("<thead>");
            break;
        case MD_BLOCK_TBODY:
            ctx->out.append("<tbody>");
            break;
        case MD_BLOCK_TR:
            ctx->out.append("<tr>");
            break;
        case MD_BLOCK_TH:
            ctx->out.append("<th class=\"md-th\">");
            break;
        case MD_BLOCK_TD:
            ctx->out.append("<td class=\"md-td\">");
            break;
        case MD_BLOCK_UL:
            ctx->listStack.push_back({false, 0, 1});
            ctx->out.append("<div class=\"md-list\">");
            break;
        case MD_BLOCK_OL:
        {
            int start = 1;
            if (auto *ol = static_cast<MD_BLOCK_OL_DETAIL *>(detail)) {
                start = static_cast<int>(ol->start);
                if (start < 1) {
                    start = 1;
                }
            }
            ctx->listStack.push_back({true, 0, start});
            ctx->out.append("<div class=\"md-list\">");
            break;
        }
        case MD_BLOCK_LI:
        {
            std::string bullet = "•";
            if (!ctx->listStack.empty() && ctx->listStack.back().ordered) {
                int number = ctx->listStack.back().start + ctx->listStack.back().index;
                bullet = std::to_string(number) + ".";
                ctx->listStack.back().index++;
            } else if (!ctx->listStack.empty()) {
                ctx->listStack.back().index++;
            }
            ctx->out.append("<div class=\"md-li\"><span class=\"md-li-marker\">");
            ctx->out.append(bullet);
            ctx->out.append("</span><span class=\"md-li-text\">");
            break;
        }
        case MD_BLOCK_QUOTE:
            ctx->out.append("<blockquote class=\"md-quote\">");
            break;
        case MD_BLOCK_CODE:
            ctx->out.append("<pre class=\"md-code\"><code class=\"md-inline-code\">");
            break;
        default:
            break;
    }
    return 0;
}

int mdLeaveBlock(MD_BLOCKTYPE type, void *, void *userdata) {
    auto *ctx = static_cast<MarkdownContext *>(userdata);
    if (!ctx) {
        return 0;
    }
    switch (type) {
        case MD_BLOCK_P:
            ctx->out.append("</div>");
            break;
        case MD_BLOCK_H:
            ctx->out.append("</div>");
            break;
        case MD_BLOCK_TABLE:
            ctx->out.append("</table>");
            break;
        case MD_BLOCK_THEAD:
            ctx->out.append("</thead>");
            break;
        case MD_BLOCK_TBODY:
            ctx->out.append("</tbody>");
            break;
        case MD_BLOCK_TR:
            ctx->out.append("</tr>");
            break;
        case MD_BLOCK_TH:
            ctx->out.append("</th>");
            break;
        case MD_BLOCK_TD:
            ctx->out.append("</td>");
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            ctx->out.append("</div>");
            if (!ctx->listStack.empty()) {
                ctx->listStack.pop_back();
            }
            break;
        case MD_BLOCK_LI:
            ctx->out.append("</span></div>");
            break;
        case MD_BLOCK_QUOTE:
            ctx->out.append("</blockquote>");
            break;
        case MD_BLOCK_CODE:
            ctx->out.append("</code></pre>");
            break;
        default:
            break;
    }
    return 0;
}

int mdEnterSpan(MD_SPANTYPE type, void *detail, void *userdata) {
    auto *ctx = static_cast<MarkdownContext *>(userdata);
    if (!ctx) {
        return 0;
    }
    switch (type) {
        case MD_SPAN_EM:
            ctx->out.append("<em>");
            break;
        case MD_SPAN_STRONG:
            ctx->out.append("<strong>");
            break;
        case MD_SPAN_CODE:
            ctx->out.append("<code class=\"md-inline-code\">");
            break;
        case MD_SPAN_DEL:
            ctx->out.append("<del>");
            break;
        case MD_SPAN_A: {
            auto *a = static_cast<MD_SPAN_A_DETAIL *>(detail);
            std::string href;
            if (a) {
                href.assign(a->href.text, a->href.text + a->href.size);
            }
            ctx->linkStack.push_back(href);
            ctx->out.append("<a href=\"");
            ctx->out.append(escapeRmlText(href));
            ctx->out.append("\">");
            break;
        }
        default:
            break;
    }
    return 0;
}

int mdLeaveSpan(MD_SPANTYPE type, void *, void *userdata) {
    auto *ctx = static_cast<MarkdownContext *>(userdata);
    if (!ctx) {
        return 0;
    }
    switch (type) {
        case MD_SPAN_EM:
            ctx->out.append("</em>");
            break;
        case MD_SPAN_STRONG:
            ctx->out.append("</strong>");
            break;
        case MD_SPAN_CODE:
            ctx->out.append("</code>");
            break;
        case MD_SPAN_DEL:
            ctx->out.append("</del>");
            break;
        case MD_SPAN_A:
            ctx->out.append("</a>");
            if (!ctx->linkStack.empty()) {
                ctx->linkStack.pop_back();
            }
            break;
        default:
            break;
    }
    return 0;
}

int mdText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    auto *ctx = static_cast<MarkdownContext *>(userdata);
    if (!ctx) {
        return 0;
    }
    std::string_view view(text ? text : "", static_cast<std::size_t>(size));
    switch (type) {
        case MD_TEXT_NORMAL:
        case MD_TEXT_ENTITY:
            ctx->out.append(renderTextWithTwemoji(view));
            break;
        case MD_TEXT_CODE:
            ctx->out.append(escapeRmlText(view));
            break;
        case MD_TEXT_BR:
        case MD_TEXT_SOFTBR:
            ctx->out.append("<br/>");
            break;
        default:
            break;
    }
    return 0;
}

std::string renderMarkdownToRml(const std::string &text) {
    if (text.empty()) {
        return {};
    }
    MarkdownContext ctx;
    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEURLAUTOLINKS | MD_FLAG_TABLES;
    parser.enter_block = mdEnterBlock;
    parser.leave_block = mdLeaveBlock;
    parser.enter_span = mdEnterSpan;
    parser.leave_span = mdLeaveSpan;
    parser.text = mdText;
    md_parse(text.c_str(), text.size(), &parser, &ctx);
    return ctx.out;
}
} // namespace

class RmlUiPanelCommunity::RmlUiPanelCommunityListener final : public Rml::EventListener {
public:
    enum class Action {
        Add,
        Selection,
        SelectionBlur,
        Refresh,
        Join,
        Roam,
        Quit,
        AddOnEnter
    };

    RmlUiPanelCommunityListener(RmlUiPanelCommunity *panelIn, Action actionIn)
        : panel(panelIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        switch (action) {
            case Action::Add:
                panel->handleAdd();
                break;
            case Action::Selection:
                panel->handleSelection();
                break;
            case Action::SelectionBlur:
                panel->handleSelectionBlur();
                break;
            case Action::Refresh:
                panel->handleRefresh();
                break;
            case Action::Join:
                panel->handleJoin();
                break;
            case Action::Roam:
                panel->handleRoam();
                break;
            case Action::Quit:
                panel->handleQuit();
                break;
            case Action::AddOnEnter: {
                const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
                if (keyIdentifier == Rml::Input::KI_RETURN || keyIdentifier == Rml::Input::KI_NUMPADENTER) {
                    panel->handleAdd();
                }
                break;
            }
        }
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
    Action action;
};

class RmlUiPanelCommunity::ServerRowListener final : public Rml::EventListener {
public:
    enum class Action {
        Select,
        Join
    };

    ServerRowListener(RmlUiPanelCommunity *panelIn, int indexIn, Action actionIn)
        : panel(panelIn), index(indexIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (action == Action::Join) {
            panel->handleServerClick(index);
            if (panel->hasActiveConnection()) {
                return;
            }
            auto *community = panel->consoleModel ? &panel->consoleModel->community : nullptr;
            if (community &&
                community->selectedIndex == index &&
                community->selectedIndex >= 0 &&
                community->selectedIndex < static_cast<int>(community->entries.size()) &&
                panel->isConnectedToEntry(community->entries[static_cast<std::size_t>(community->selectedIndex)])) {
                panel->handleResume();
            } else {
                panel->handleJoin();
            }
            return;
        }
        panel->handleServerClick(index);
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
    int index = -1;
    Action action = Action::Select;
};

class RmlUiPanelCommunity::WebsiteLinkListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event &event) override {
        if (auto *element = event.GetTargetElement()) {
            if (auto *hrefVar = element->GetAttribute("href")) {
                if (hrefVar->GetType() == Rml::Variant::STRING) {
                    const std::string href = hrefVar->Get<std::string>();
                    openUrlInBrowser(href);
                }
            }
        }
    }
};

class RmlUiPanelCommunity::CommunityInfoListener final : public Rml::EventListener {
public:
    explicit CommunityInfoListener(RmlUiPanelCommunity *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->handleCommunityInfoToggle();
        }
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
};

class RmlUiPanelCommunity::PasswordHintListener final : public Rml::EventListener {
public:
    explicit PasswordHintListener(RmlUiPanelCommunity *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->handlePasswordHintDismiss();
        }
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
};

class RmlUiPanelCommunity::CredentialChangeListener final : public Rml::EventListener {
public:
    enum class Field {
        Username,
        Password
    };

    CredentialChangeListener(RmlUiPanelCommunity *panelIn, Field fieldIn)
        : panel(panelIn), field(fieldIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        if (field == Field::Password) {
            panel->setPasswordHintActive(false);
            panel->persistCommunityCredentials(true);
            return;
        }
        panel->storedPasswordHash.clear();
        panel->setPasswordHintActive(false);
        panel->persistCommunityCredentials(true);
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
    Field field;
};

class RmlUiPanelCommunity::DeleteDialogListener final : public Rml::EventListener {
public:
    explicit DeleteDialogListener(RmlUiPanelCommunity *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->showDeleteDialog();
        }
    }

private:
    RmlUiPanelCommunity *panel = nullptr;
};

RmlUiPanelCommunity::RmlUiPanelCommunity()
    : RmlUiPanel("community", "client/ui/console_panel_community.rml") {}

void RmlUiPanelCommunity::setConsoleModel(ConsoleModel *model, ConsoleController *controller) {
    consoleModel = model;
    consoleController = controller;
}

void RmlUiPanelCommunity::bindCallbacks(std::function<void(int)> onSelection,
                                        std::function<void(const std::string &)> onAdd,
                                        std::function<void()> onRefresh,
                                        std::function<void(int)> onServerSelection,
                                        std::function<void(int)> onJoin,
                                        std::function<void(int)> onRoam,
                                        std::function<void()> onResume,
                                        std::function<void()> onQuit) {
    onSelectionChanged = std::move(onSelection);
    onAddRequested = std::move(onAdd);
    onRefreshRequested = std::move(onRefresh);
    onServerSelectionChanged = std::move(onServerSelection);
    onJoinRequested = std::move(onJoin);
    onRoamRequested = std::move(onRoam);
    onResumeRequested = std::move(onResume);
    onQuitRequested = std::move(onQuit);
}

void RmlUiPanelCommunity::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    selectElement = document->GetElementById("community-select");
    addButton = document->GetElementById("community-add-button");
    refreshButton = document->GetElementById("community-refresh-button");
    inputElement = document->GetElementById("community-add-input");
    usernameInput = document->GetElementById("community-username-input");
    passwordInput = document->GetElementById("community-password-input");
    passwordLabel = document->GetElementById("community-password-label");
    serverList = document->GetElementById("server-list");
    communityInfoButton = document->GetElementById("community-info-button");
    detailTitle = document->GetElementById("server-detail-title");
    joinButton = document->GetElementById("server-join-button");
    roamButton = document->GetElementById("server-roam-button");
    quitButton = document->GetElementById("server-quit-button");
    detailName = document->GetElementById("server-detail-name");
    detailWebsite = document->GetElementById("server-detail-website");
    detailOverview = document->GetElementById("server-detail-overview");
    detailDescription = document->GetElementById("server-detail-description");
    detailScreenshot = document->GetElementById("server-detail-screenshot");
    detailServerSection = document->GetElementById("detail-server-section");
    detailWebsiteSection = document->GetElementById("detail-website-section");
    detailOverviewSection = document->GetElementById("detail-overview-section");
    detailDescriptionSection = document->GetElementById("detail-description-section");
    detailScreenshotSection = document->GetElementById("detail-screenshot-section");
    detailLanInfoSection = document->GetElementById("detail-lan-info");
    detailLanInfoText = document->GetElementById("lan-info-text");
    communityDeleteButton = document->GetElementById("community-delete-button");
    confirmDialog.bind(document, "join-confirm-overlay", "join-confirm-message",
                       "join-confirm-yes", "join-confirm-no");
    errorDialog.bind(document, "error-dialog-overlay", "error-dialog-message",
                     "error-dialog-ok");
    deleteDialog.bind(document, "delete-confirm-overlay", "delete-confirm-message",
                      "delete-confirm-yes", "delete-confirm-no");

    listeners.clear();
    if (addButton) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Add);
        addButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (inputElement) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::AddOnEnter);
        inputElement->AddEventListener("keydown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (refreshButton) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Refresh);
        refreshButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (joinButton) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Join);
        joinButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (roamButton) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Roam);
        roamButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (quitButton) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Quit);
        quitButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (selectElement) {
        auto listener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::Selection);
        selectElement->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
        auto blurListener = std::make_unique<RmlUiPanelCommunityListener>(this, RmlUiPanelCommunityListener::Action::SelectionBlur);
        selectElement->AddEventListener("blur", blurListener.get());
        selectElement->AddEventListener("focusout", blurListener.get());
        listeners.emplace_back(std::move(blurListener));
    }
    if (detailWebsite) {
        auto listener = std::make_unique<WebsiteLinkListener>();
        detailWebsite->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (communityInfoButton) {
        auto listener = std::make_unique<CommunityInfoListener>(this);
        communityInfoButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (communityDeleteButton) {
        auto listener = std::make_unique<DeleteDialogListener>(this);
        communityDeleteButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (passwordInput) {
        auto listener = std::make_unique<PasswordHintListener>(this);
        passwordInput->AddEventListener("focus", listener.get());
        passwordInput->AddEventListener("click", listener.get());
        passwordInput->AddEventListener("keydown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (usernameInput) {
        auto listener = std::make_unique<CredentialChangeListener>(this, CredentialChangeListener::Field::Username);
        usernameInput->AddEventListener("change", listener.get());
        usernameInput->AddEventListener("blur", listener.get());
        usernameInput->AddEventListener("focusout", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (passwordInput) {
        auto listener = std::make_unique<CredentialChangeListener>(this, CredentialChangeListener::Field::Password);
        passwordInput->AddEventListener("change", listener.get());
        passwordInput->AddEventListener("blur", listener.get());
        passwordInput->AddEventListener("focusout", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    confirmDialog.setOnAccept([this]() { handleConfirmJoin(true); });
    confirmDialog.setOnCancel([this]() { handleConfirmJoin(false); });
    confirmDialog.installListeners(listeners);

    errorDialog.setOnAccept([this]() { handleErrorDialogClose(); });
    errorDialog.setOnCancel([this]() { handleErrorDialogClose(); });
    errorDialog.installListeners(listeners);

    deleteDialog.setOnAccept([this]() { handleDeleteConfirm(true); });
    deleteDialog.setOnCancel([this]() { handleDeleteConfirm(false); });
    deleteDialog.installListeners(listeners);

    clearAddStatus();
    if (consoleModel) {
        setListOptions(consoleModel->community.listOptions,
                       consoleModel->community.listSelectedIndex);
        setEntries(consoleModel->community.entries);
    }
}

void RmlUiPanelCommunity::setCommunityDetails(const std::string &details) {
    if (consoleModel) {
        consoleModel->community.detailsText = details;
    }
    if (showingCommunityInfo) {
        updateServerDetails();
    }
}

void RmlUiPanelCommunity::setListOptions(const std::vector<ServerListOption> &options, int selected) {
    if (!consoleModel) {
        return;
    }
    consoleModel->community.listOptions = options;
    consoleModel->community.listSelectedIndex = selected;
    auto &listOptions = consoleModel->community.listOptions;
    auto &selectedIndex = consoleModel->community.listSelectedIndex;
    if (selectedIndex < 0 && !listOptions.empty()) {
        selectedIndex = 0;
    }
    if (passwordLabel) {
        passwordLabel->SetClass("hidden", false);
    }
    if (passwordInput) {
        passwordInput->SetClass("hidden", false);
    }
    if (communityDeleteButton) {
        communityDeleteButton->SetClass("hidden", isLanSelected());
    }
    storedPasswordHash.clear();
    if (!selectElement) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(selectElement);
    if (!select) {
        return;
    }
    suppressSelectionEvents = true;
    select->RemoveAll();
    for (std::size_t i = 0; i < listOptions.size(); ++i) {
        const auto &opt = listOptions[i];
        const std::string label = opt.name.empty() ? opt.host : opt.name;
        select->Add(label, std::to_string(i));
    }
    if (selectedIndex < 0 && !listOptions.empty()) {
        selectedIndex = 0;
    }
    if (selectedIndex >= 0) {
        select->SetSelection(selectedIndex);
    }
    suppressSelectionEvents = false;
    refreshCommunityCredentials();
}

void RmlUiPanelCommunity::refreshCommunityCredentials() {
    storedPasswordHash.clear();
    if (usernameInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(usernameInput)) {
            input->SetValue("");
        }
    }
    clearPasswordValue();

    if (!consoleController) {
        return;
    }
    const auto creds = consoleController->loadCommunityCredentials(consoleModel->community.listSelectedIndex);
    if (!creds.username.empty()) {
        setUsernameValue(creds.username);
    }
    if (!creds.storedPasswordHash.empty()) {
        storedPasswordHash = creds.storedPasswordHash;
        setPasswordHintActive(true);
    } else {
        setPasswordHintActive(false);
    }
}

void RmlUiPanelCommunity::setEntries(const std::vector<CommunityBrowserEntry> &entriesIn) {
    if (!consoleModel) {
        return;
    }
    consoleModel->community.entries = entriesIn;
    auto &entries = consoleModel->community.entries;
    if (!serverList) {
        return;
    }
    serverList->SetInnerRML("");
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(), [](const std::unique_ptr<Rml::EventListener> &ptr) {
            return dynamic_cast<ServerRowListener *>(ptr.get()) != nullptr;
        }),
        listeners.end());

    std::string listMarkup;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        const std::string label = !entry.worldName.empty()
            ? entry.worldName
            : (!entry.label.empty() ? entry.label : entry.host);
        const std::string players = entry.maxPlayers > 0
            ? (std::to_string(entry.activePlayers) + "/" + std::to_string(entry.maxPlayers))
            : std::string{};
        const std::string rowId = "server-row-" + std::to_string(i);
        const std::string nameId = "server-name-" + std::to_string(i);
        const char *parityClass = (i % 2 == 0) ? "even" : "odd";
        std::string row = "<div id=\"" + rowId + "\" class=\"server-item " + std::string(parityClass) + "\">";
        row += "<span id=\"" + nameId + "\" class=\"server-name\"></span>";
        if (!players.empty()) {
            row += "<span class=\"server-players\">" + players + "</span>";
        }
        row += "</div>";
        listMarkup += row;
    }
    serverList->SetInnerRML(listMarkup);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        const std::string label = !entry.worldName.empty()
            ? entry.worldName
            : (!entry.label.empty() ? entry.label : entry.host);
        const std::string nameId = "server-name-" + std::to_string(i);
        const std::string rowId = "server-row-" + std::to_string(i);
        if (auto *nameEl = document->GetElementById(nameId)) {
            nameEl->SetInnerRML(renderTextWithTwemoji(label));
        }
        if (auto *row = document->GetElementById(rowId)) {
            auto listener = std::make_unique<ServerRowListener>(this, static_cast<int>(i), ServerRowListener::Action::Select);
            row->AddEventListener("click", listener.get());
            listeners.emplace_back(std::move(listener));
            auto dblListener = std::make_unique<ServerRowListener>(this, static_cast<int>(i), ServerRowListener::Action::Join);
            row->AddEventListener("dblclick", dblListener.get());
            listeners.emplace_back(std::move(dblListener));
        }
    }

    if (consoleModel->community.selectedIndex >= static_cast<int>(entries.size())) {
        consoleModel->community.selectedIndex = -1;
    }
    updateServerDetails();
}

void RmlUiPanelCommunity::setAddStatus(const std::string &text, bool isError) {
    (void)text;
    (void)isError;
}

void RmlUiPanelCommunity::setServerDescriptionLoading(const std::string &key, bool loading) {
    if (consoleModel) {
        consoleModel->community.serverDescriptionLoadingKey = key;
        consoleModel->community.serverDescriptionLoading = loading;
        if (!loading && key.empty()) {
            consoleModel->community.serverDescriptionLoadingKey.clear();
        }
    }
    updateServerDetails();
}

void RmlUiPanelCommunity::setServerDescriptionError(const std::string &key, const std::string &message) {
    if (consoleModel) {
        consoleModel->community.serverDescriptionErrorKey = key;
        consoleModel->community.serverDescriptionErrorText = message;
    }
    updateServerDetails();
}

void RmlUiPanelCommunity::clearAddInput() {
    if (inputElement) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(inputElement)) {
            input->SetValue("");
        }
    }
    clearAddStatus();
}

void RmlUiPanelCommunity::setConnectionState(const ConsoleInterface::ConnectionState &state) {
    if (consoleModel) {
        consoleModel->connectionState = state;
    }
    updateServerDetails();
}

void RmlUiPanelCommunity::setUserConfigPath(const std::string &path) {
    (void)path;
}

void RmlUiPanelCommunity::showErrorDialog(const std::string &message) {
    errorDialog.show(escapeRmlText(message));
}

void RmlUiPanelCommunity::setPasswordHintActive(bool active) {
    passwordHintActive = active;
    if (passwordInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(passwordInput)) {
            if (passwordHintActive) {
                input->SetValue("stored");
            } else if (input->GetValue() == "stored") {
                input->SetValue("");
            }
        }
    }
}

std::string RmlUiPanelCommunity::getUsernameValue() const {
    if (usernameInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(usernameInput)) {
            return input->GetValue();
        }
    }
    return {};
}

std::string RmlUiPanelCommunity::getPasswordValue() const {
    if (passwordInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(passwordInput)) {
            if (passwordHintActive) {
                return {};
            }
            return input->GetValue();
        }
    }
    return {};
}

std::string RmlUiPanelCommunity::getStoredPasswordHashValue() const {
    return storedPasswordHash;
}

void RmlUiPanelCommunity::setStoredPasswordHashValue(const std::string &value) {
    storedPasswordHash = value;
    setPasswordHintActive(!storedPasswordHash.empty());
}

void RmlUiPanelCommunity::clearPasswordValue() {
    if (passwordInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(passwordInput)) {
            input->SetValue("");
        }
    }
    passwordHintActive = false;
}

void RmlUiPanelCommunity::setUsernameValue(const std::string &value) {
    if (usernameInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(usernameInput)) {
            input->SetValue(value);
        }
    }
}

void RmlUiPanelCommunity::handleSelection() {
    if (!selectElement || !onSelectionChanged) {
        return;
    }
    if (!consoleModel) {
        return;
    }
    if (suppressSelectionEvents) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(selectElement);
    if (!select) {
        return;
    }
    const int selection = select->GetSelection();
    consoleModel->community.listSelectedIndex = selection;
    onSelectionChanged(selection);
    refreshCommunityCredentials();
    if (passwordLabel) {
        passwordLabel->SetClass("hidden", isLanSelected());
    }
    if (passwordInput) {
        passwordInput->SetClass("hidden", isLanSelected());
    }
    if (communityDeleteButton) {
        communityDeleteButton->SetClass("hidden", isLanSelected());
    }
    showingCommunityInfo = true;
    consoleModel->community.selectedIndex = -1;
    updateServerDetails();
}

void RmlUiPanelCommunity::handleSelectionBlur() {
    if (!selectElement) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(selectElement);
    if (!select) {
        return;
    }
    if (select->IsSelectBoxVisible()) {
        select->HideSelectBox();
    }
}

void RmlUiPanelCommunity::handleAdd() {
    if (!inputElement || !onAddRequested) {
        return;
    }
    auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(inputElement);
    if (!input) {
        return;
    }
    std::string value = input->GetValue();
    onAddRequested(value);
}

void RmlUiPanelCommunity::handleRefresh() {
    if (onRefreshRequested) {
        onRefreshRequested();
    }
}

void RmlUiPanelCommunity::handleJoin() {
    if (!consoleModel) {
        return;
    }
    if (hasActiveConnection()) {
        return;
    }
    auto &community = consoleModel->community;
    KARMA_TRACE("ui.rmlui",
                "RmlUi Community: Join clicked (selectedServerIndex={}, entries={})",
                community.selectedIndex,
                community.entries.size());
    if (community.selectedIndex < 0 ||
        community.selectedIndex >= static_cast<int>(community.entries.size())) {
        spdlog::warn("RmlUi Community: Join ignored (no valid selection)");
        return;
    }
    const auto &entry = community.entries[static_cast<std::size_t>(community.selectedIndex)];
    if (detailLanInfoSection) detailLanInfoSection->SetClass("hidden", true);
    if (detailServerSection) detailServerSection->SetClass("hidden", false);
    if (detailWebsiteSection) detailWebsiteSection->SetClass("hidden", false);
    if (detailOverviewSection) detailOverviewSection->SetClass("hidden", false);
    if (detailDescriptionSection) detailDescriptionSection->SetClass("hidden", false);
    if (detailScreenshotSection) detailScreenshotSection->SetClass("hidden", false);
    if (!onJoinRequested) {
        spdlog::warn("RmlUi Community: Join ignored (no callback bound)");
        return;
    }
    onJoinRequested(community.selectedIndex);
}

void RmlUiPanelCommunity::handleRoam() {
    if (!consoleModel) {
        return;
    }
    if (hasActiveConnection()) {
        return;
    }
    auto &community = consoleModel->community;
    if (community.selectedIndex < 0 ||
        community.selectedIndex >= static_cast<int>(community.entries.size())) {
        return;
    }
    if (!onRoamRequested) {
        return;
    }
    onRoamRequested(community.selectedIndex);
}

void RmlUiPanelCommunity::handleResume() {
    if (onResumeRequested) {
        onResumeRequested();
    }
}

void RmlUiPanelCommunity::handleQuit() {
    if (onQuitRequested) {
        onQuitRequested();
    }
}

void RmlUiPanelCommunity::handleConfirmJoin(bool accepted) {
    if (!accepted) {
        pendingJoinIndex = -1;
        return;
    }
    if (!consoleModel) {
        pendingJoinIndex = -1;
        return;
    }
    auto &community = consoleModel->community;
    const int joinIndex = pendingJoinIndex;
    pendingJoinIndex = -1;
    if (joinIndex < 0 || joinIndex >= static_cast<int>(community.entries.size())) {
        return;
    }
    if (onJoinRequested) {
        onJoinRequested(joinIndex);
    }
    if (onQuitRequested) {
        onQuitRequested();
    }
}

void RmlUiPanelCommunity::handlePasswordHintDismiss() {
    if (!passwordHintActive) {
        return;
    }
    passwordHintActive = false;
    if (passwordInput) {
        if (auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(passwordInput)) {
            if (input->GetValue() == "stored") {
                input->SetValue("");
            }
        }
    }
}

void RmlUiPanelCommunity::handleErrorDialogClose() {
    errorDialog.hide();
}

void RmlUiPanelCommunity::handleDeleteConfirm(bool accepted) {
    if (!accepted) {
        return;
    }
    if (!consoleModel || !consoleController) {
        return;
    }
    const auto &community = consoleModel->community;
    if (community.listSelectedIndex >= 0 &&
        community.listSelectedIndex < static_cast<int>(community.listOptions.size())) {
        const auto &opt = community.listOptions[static_cast<std::size_t>(community.listSelectedIndex)];
        if (!opt.host.empty()) {
            consoleController->queueDeleteListRequest(opt.host);
        }
    }
}

void RmlUiPanelCommunity::showDeleteDialog() {
    std::string label = "this community";
    if (consoleModel) {
        const auto &community = consoleModel->community;
        if (community.listSelectedIndex >= 0 &&
            community.listSelectedIndex < static_cast<int>(community.listOptions.size())) {
            const auto &opt = community.listOptions[static_cast<std::size_t>(community.listSelectedIndex)];
            label = !opt.name.empty() ? opt.name : opt.host;
        }
    }
    deleteDialog.show("Delete \"" + escapeRmlText(label) + "\"?");
}

std::string RmlUiPanelCommunity::communityKeyForIndex(int index) const {
    if (!consoleController) {
        return {};
    }
    return consoleController->communityKeyForIndex(index);
}

void RmlUiPanelCommunity::persistCommunityCredentials(bool passwordChanged) {
    if (!consoleModel || !consoleController) {
        return;
    }
    const std::string username = getUsernameValue();
    const auto result = consoleController->persistCommunityCredentials(
        consoleModel->community.listSelectedIndex,
        username,
        storedPasswordHash,
        passwordChanged);
    if (result.clearStoredPasswordHash) {
        passwordHintActive = false;
        storedPasswordHash.clear();
    }
}

void RmlUiPanelCommunity::handleCommunityInfoToggle() {
    showingCommunityInfo = true;
    if (showingCommunityInfo) {
        if (consoleModel) {
            consoleModel->community.selectedIndex = -1;
        }
    }
    updateServerDetails();

    if (serverList && consoleModel) {
        const auto &entries = consoleModel->community.entries;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const std::string rowId = "server-row-" + std::to_string(i);
            if (auto *row = document->GetElementById(rowId)) {
                row->SetClass("selected", false);
            }
        }
    }
}

void RmlUiPanelCommunity::clearAddStatus() {
    return;
}

void RmlUiPanelCommunity::handleServerClick(int index) {
    if (!consoleModel) {
        return;
    }
    const auto &entries = consoleModel->community.entries;
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return;
    }
    consoleModel->community.selectedIndex = index;
    showingCommunityInfo = false;
    if (onServerSelectionChanged) {
        onServerSelectionChanged(index);
    }
    updateServerDetails();
}

void RmlUiPanelCommunity::updateServerDetails() {
    if (!detailName || !detailWebsite || !detailOverview || !detailDescription || !detailScreenshot) {
        return;
    }
    if (!consoleModel) {
        return;
    }
    auto &community = consoleModel->community;

    if (detailTitle) {
        detailTitle->SetInnerRML(showingCommunityInfo ? "Community Info" : "Server Details");
    }
    const bool connected = hasActiveConnection();
    if (joinButton) {
        joinButton->SetClass("hidden", connected || showingCommunityInfo || community.selectedIndex < 0);
    }
    if (roamButton) {
        roamButton->SetClass("hidden", connected || showingCommunityInfo || community.selectedIndex < 0);
    }
    if (quitButton) {
        quitButton->SetClass("hidden", !connected);
    }
    if (communityDeleteButton) {
        communityDeleteButton->SetClass("hidden", !showingCommunityInfo || isLanSelected());
    }
    confirmDialog.hide();

    if (showingCommunityInfo) {
        const bool lanInfo = isLanSelected();
        if (detailServerSection) detailServerSection->SetClass("hidden", lanInfo);
        if (detailWebsiteSection) detailWebsiteSection->SetClass("hidden", lanInfo);
        if (detailOverviewSection) detailOverviewSection->SetClass("hidden", lanInfo);
        if (detailDescriptionSection) detailDescriptionSection->SetClass("hidden", lanInfo);
        if (detailScreenshotSection) detailScreenshotSection->SetClass("hidden", lanInfo);
        if (detailLanInfoSection) detailLanInfoSection->SetClass("hidden", !lanInfo);
        if (lanInfo) {
            if (detailLanInfoText) {
                detailLanInfoText->SetInnerRML(
                    "Local Area Network (LAN) shows servers running on your local network. "
                    "If you want to play with friends nearby, start a server from the Start Server panel "
                    "and it will appear here for everyone on the same LAN.");
            }
            detailName->SetInnerRML("");
            detailWebsite->SetInnerRML("");
            detailWebsite->SetAttribute("href", "");
            detailOverview->SetInnerRML("");
            detailDescription->SetInnerRML("");
            detailScreenshot->SetInnerRML("");
            return;
        } else {
            if (detailServerSection) detailServerSection->SetClass("hidden", false);
            if (detailWebsiteSection) detailWebsiteSection->SetClass("hidden", false);
            if (detailOverviewSection) detailOverviewSection->SetClass("hidden", false);
            if (detailDescriptionSection) detailDescriptionSection->SetClass("hidden", false);
            if (detailScreenshotSection) detailScreenshotSection->SetClass("hidden", false);
        }
        detailName->SetInnerRML("");
        detailWebsite->SetInnerRML("");
        detailWebsite->SetAttribute("href", "");
        detailOverview->SetInnerRML("");
        if (community.detailsText.empty()) {
            detailDescription->SetInnerRML("No community details available.");
        } else {
            const std::string rendered = renderMarkdownToRml(community.detailsText);
            detailDescription->SetInnerRML(rendered.empty()
                                               ? escapeRmlText(community.detailsText)
                                               : rendered);
        }
        detailScreenshot->SetInnerRML("");
        return;
    }

    for (std::size_t i = 0; i < community.entries.size(); ++i) {
        const std::string rowId = "server-row-" + std::to_string(i);
        if (auto *row = document->GetElementById(rowId)) {
            row->SetClass("selected", static_cast<int>(i) == community.selectedIndex);
        }
    }

    if (community.selectedIndex < 0 ||
        community.selectedIndex >= static_cast<int>(community.entries.size())) {
        detailName->SetInnerRML("Select a server");
        detailWebsite->SetInnerRML("");
        detailWebsite->SetAttribute("href", "");
        detailOverview->SetInnerRML("");
        detailDescription->SetInnerRML("");
        detailScreenshot->SetInnerRML("");
        if (detailLanInfoSection) detailLanInfoSection->SetClass("hidden", true);
        if (detailServerSection) detailServerSection->SetClass("hidden", false);
        if (detailWebsiteSection) detailWebsiteSection->SetClass("hidden", false);
        if (detailOverviewSection) detailOverviewSection->SetClass("hidden", false);
        if (detailDescriptionSection) detailDescriptionSection->SetClass("hidden", false);
        if (detailScreenshotSection) detailScreenshotSection->SetClass("hidden", false);
        return;
    }

    const auto &entry = community.entries[static_cast<std::size_t>(community.selectedIndex)];
    if (joinButton) {
        joinButton->SetInnerRML("Join");
        joinButton->SetClass("hidden", connected);
    }
    if (roamButton) {
        roamButton->SetClass("hidden", connected);
    }
    if (quitButton) {
        quitButton->SetClass("hidden", !connected);
    }
    const std::string name = !entry.worldName.empty()
        ? entry.worldName
        : (!entry.label.empty() ? entry.label : entry.host);
    const std::string overview = entry.description;
    const std::string description = entry.longDescription;
    const std::string website = buildServerWebsite(entry);
    const std::string detailsKey = makeServerDetailsKey(entry);

    detailName->SetInnerRML(renderTextWithTwemoji(name));
    detailWebsite->SetInnerRML(website);
    detailWebsite->SetAttribute("href", website);
    detailOverview->SetInnerRML(overview.empty() ? "No overview available." : renderTextWithTwemoji(overview));
    if (description.empty()) {
        if (community.serverDescriptionLoading &&
            !detailsKey.empty() &&
            detailsKey == community.serverDescriptionLoadingKey) {
            detailDescription->SetInnerRML("Fetching server description...");
        } else if (!detailsKey.empty() &&
                   detailsKey == community.serverDescriptionErrorKey &&
                   !community.serverDescriptionErrorText.empty()) {
            detailDescription->SetInnerRML(
                "Description unavailable: " + escapeRmlText(community.serverDescriptionErrorText));
        } else {
            detailDescription->SetInnerRML("No description provided.");
        }
    } else {
        const std::string rendered = renderMarkdownToRml(description);
        detailDescription->SetInnerRML(rendered.empty() ? escapeRmlText(description) : rendered);
    }
    detailScreenshot->SetInnerRML(entry.screenshotId.empty() ? "None" : entry.screenshotId);
}

std::string RmlUiPanelCommunity::makeServerDetailsKey(const CommunityBrowserEntry &entry) const {
    if (entry.sourceHost.empty()) {
        return {};
    }
    if (entry.code.empty()) {
        return {};
    }
    return entry.sourceHost + "|" + entry.code;
}

std::string RmlUiPanelCommunity::buildServerWebsite(const CommunityBrowserEntry &entry) const {
    std::string base = entry.sourceHost.empty() ? entry.host : entry.sourceHost;
    if (base.empty()) {
        return {};
    }
    if (base.rfind("http://", 0) != 0 && base.rfind("https://", 0) != 0) {
        base = "http://" + base;
    }
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    if (entry.code.empty()) {
        return {};
    }
    const std::string &name = entry.code;
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex << std::setfill('0');
    for (unsigned char ch : name) {
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
    return base + "/servers/" + encoded.str();
}

bool RmlUiPanelCommunity::hasActiveConnection() const {
    return consoleModel && consoleModel->connectionState.connected;
}

bool RmlUiPanelCommunity::isLanSelected() const {
    if (!consoleModel) {
        return false;
    }
    const auto &community = consoleModel->community;
    return community.listSelectedIndex >= 0 &&
           community.listSelectedIndex < static_cast<int>(community.listOptions.size()) &&
           community.listOptions[static_cast<std::size_t>(community.listSelectedIndex)].name == "Local Area Network";
}

bool RmlUiPanelCommunity::isConnectedToEntry(const CommunityBrowserEntry &entry) const {
    if (!consoleModel) {
        return false;
    }
    const auto &state = consoleModel->connectionState;
    return state.connected &&
           state.port == entry.port &&
           !state.host.empty() &&
           state.host == entry.host;
}

} // namespace ui

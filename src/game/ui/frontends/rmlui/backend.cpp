#include "ui/frontends/rmlui/backend.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>
#include <cmath>

#if defined(KARMA_RENDER_BACKEND_BGFX)
#include "karma_extras/ui/platform/rmlui/renderer_bgfx.hpp"
#elif defined(KARMA_RENDER_BACKEND_DILIGENT)
#include "karma_extras/ui/platform/rmlui/renderer_diligent.hpp"
#else
#error "RmlUi backend requires BGFX or Diligent renderer."
#endif
#include "ui/frontends/rmlui/console/emoji_utils.hpp"
#include "ui/frontends/rmlui/translate.hpp"
#include "karma/platform/window.hpp"
#include "karma/common/i18n.hpp"
#include "ui/frontends/rmlui/hud/hud.hpp"
#include "ui/frontends/rmlui/console/console.hpp"
#include "ui/frontends/rmlui/console/panels/panel_community.hpp"
#include "ui/frontends/rmlui/console/panels/panel_bindings.hpp"
#include "ui/frontends/rmlui/console/panels/panel_documentation.hpp"
#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"
#include "ui/frontends/rmlui/console/panels/panel_start_server.hpp"
#include "ui/console/tab_spec.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_store.hpp"
#include "spdlog/spdlog.h"
#include "karma_extras/ui/bridges/ui_render_bridge.hpp"
#include "ui/fonts/console_fonts.hpp"
#include "ui/config/input_mapping.hpp"
#include "ui/config/render_scale.hpp"

namespace ui_backend {
namespace {

std::string tabLabelForSpec(const ui::ConsoleTabSpec &spec) {
    if (spec.labelKey) {
        return karma::i18n::Get().get(spec.labelKey);
    }
    if (spec.fallbackLabel) {
        return spec.fallbackLabel;
    }
    return spec.key ? spec.key : "";
}

ui::RmlUiPanel *findPanelByKey(const std::vector<std::unique_ptr<ui::RmlUiPanel>> &panels,
                               const std::string &key) {
    for (const auto &panel : panels) {
        if (panel && panel->key() == key) {
            return panel.get();
        }
    }
    return nullptr;
}

class SystemInterface_Platform final : public Rml::SystemInterface {
public:
    void SetWindow(platform::Window *window) {
        windowRef = window;
        startTime = std::chrono::steady_clock::now();
        hasStart = true;
    }

    double GetElapsedTime() override {
        const auto now = std::chrono::steady_clock::now();
        if (!hasStart) {
            startTime = now;
            hasStart = true;
        }
        const std::chrono::duration<double> elapsed = now - startTime;
        return elapsed.count();
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String &message) override {
        switch (type) {
            case Rml::Log::Type::LT_ERROR:
                spdlog::error("RmlUi: {}", message);
                break;
            case Rml::Log::Type::LT_WARNING:
                spdlog::warn("RmlUi: {}", message);
                break;
            case Rml::Log::Type::LT_INFO:
                spdlog::trace("RmlUi: {}", message);
                break;
            case Rml::Log::Type::LT_DEBUG:
            case Rml::Log::Type::LT_ASSERT:
            default:
                spdlog::debug("RmlUi: {}", message);
                break;
        }
        return true;
    }

    void SetClipboardText(const Rml::String &text) override {
        if (windowRef) {
            windowRef->setClipboardText(text);
        }
    }

    void GetClipboardText(Rml::String &text) override {
        if (windowRef) {
            text = windowRef->getClipboardText();
        } else {
            text.clear();
        }
    }

private:
    platform::Window *windowRef = nullptr;
    std::chrono::steady_clock::time_point startTime{};
    bool hasStart = false;
};

class TabClickListener final : public Rml::EventListener {
public:
    TabClickListener(class RmlUiBackend *backendIn, std::string tabKeyIn)
        : backend(backendIn), tabKey(std::move(tabKeyIn)) {}

    void ProcessEvent(Rml::Event &) override {
        if (backend) {
            backend->setActiveTab(tabKey);
        }
    }

private:
    class RmlUiBackend *backend = nullptr;
    std::string tabKey;
};


} // namespace

std::string escapeRmlText(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}


struct RmlUiBackend::RmlUiState {
    SystemInterface_Platform systemInterface;
#if defined(KARMA_RENDER_BACKEND_BGFX)
    RenderInterface_BGFX renderInterface;
#elif defined(KARMA_RENDER_BACKEND_DILIGENT)
    RenderInterface_Diligent renderInterface;
#endif
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    Rml::Element *bodyElement = nullptr;
    int lastWidth = 0;
    int lastHeight = 0;
    float lastDpRatio = 1.0f;
    std::string activeTab;
    std::unordered_map<std::string, Rml::Element*> tabs;
    std::unordered_map<std::string, std::string> tabLabels;
    std::unordered_map<std::string, Rml::Element*> tabPanels;
    Rml::Element *contentElement = nullptr;
    std::vector<std::unique_ptr<Rml::EventListener>> tabListeners;
    std::unordered_map<std::string, std::string> emojiMarkupCache;
    std::vector<std::unique_ptr<ui::RmlUiPanel>> panels;
    std::unordered_set<std::string> loadedFontFiles;
    std::string consolePath;
    std::string hudPath;
    std::string lastFontLanguage;
    uint64_t lastConfigRevision = 0;
    bool reloadRequested = false;
    bool reloadArmed = false;
    bool hardReloadRequested = false;
    std::optional<std::string> pendingLanguage;
    std::string regularFontPath;
    std::string emojiFontPath;
    std::unique_ptr<ui::RmlUiHud> hud;
    bool outputVisible = false;
};

RmlUiBackend::RmlUiBackend(platform::Window &windowRefIn) : windowRef(&windowRefIn) {
    state = std::make_unique<RmlUiState>();
    consoleView = std::make_unique<ui::RmlUiConsole>();
    state->systemInterface.SetWindow(windowRef);

    Rml::SetSystemInterface(&state->systemInterface);
    Rml::SetRenderInterface(&state->renderInterface);
#if defined(KARMA_RENDER_BACKEND_BGFX)
    if (!state->renderInterface) {
        spdlog::error("RmlUi: failed to initialize bgfx renderer.");
        return;
    }
    spdlog::info("RmlUi: bgfx renderer initialized.");
#elif defined(KARMA_RENDER_BACKEND_DILIGENT)
    if (!state->renderInterface) {
        spdlog::error("RmlUi: failed to initialize Diligent renderer.");
        return;
    }
    spdlog::info("RmlUi: Diligent renderer initialized.");
#endif

    if (!Rml::Initialise()) {
        spdlog::error("RmlUi: initialization failed.");
        return;
    }

    int fbWidth = 0;
    int fbHeight = 0;
    if (windowRef) { windowRef->getFramebufferSize(fbWidth, fbHeight); }
    const float renderScale = ui::GetUiRenderScale();
    const int targetWidth = std::max(1, static_cast<int>(std::lround(fbWidth * renderScale)));
    const int targetHeight = std::max(1, static_cast<int>(std::lround(fbHeight * renderScale)));
    state->lastWidth = targetWidth;
    state->lastHeight = targetHeight;
    state->renderInterface.SetViewport(targetWidth, targetHeight);

    state->context = Rml::CreateContext("bz3", Rml::Vector2i(targetWidth, targetHeight));
    if (!state->context) {
        spdlog::error("RmlUi: failed to create context.");
        return;
    }

    float dpRatio = 1.0f;
    if (windowRef) { dpRatio = windowRef->getContentScale(); }
    const float scaledDpRatio = dpRatio / std::max(renderScale, 0.0001f);
    state->lastDpRatio = scaledDpRatio;
    state->context->SetDensityIndependentPixelRatio(scaledDpRatio);

    loadConfiguredFonts(karma::i18n::Get().language());

    state->consolePath = karma::data::Resolve("client/ui/console.rml").string();
    state->hudPath = karma::data::Resolve("client/ui/hud.rml").string();
    state->hud = std::make_unique<ui::RmlUiHud>();
    auto communityPanel = std::make_unique<ui::RmlUiPanelCommunity>();
    auto *communityPanelPtr = communityPanel.get();
    state->panels.emplace_back(std::move(communityPanel));
    auto settingsPanel = std::make_unique<ui::RmlUiPanelSettings>();
    auto *settingsPanelPtr = settingsPanel.get();
    state->panels.emplace_back(std::move(settingsPanel));
    this->settingsPanel = settingsPanelPtr;
    settingsPanelPtr->setLanguageCallback([this](const std::string &language) {
        if (!state) {
            return;
        }
        state->pendingLanguage = language;
        state->reloadRequested = false;
        state->reloadArmed = true;
    });
    auto bindingsPanel = std::make_unique<ui::RmlUiPanelBindings>();
    auto *bindingsPanelPtr = bindingsPanel.get();
    state->panels.emplace_back(std::move(bindingsPanel));
    state->panels.emplace_back(std::make_unique<ui::RmlUiPanelDocumentation>());
    auto startServerPanel = std::make_unique<ui::RmlUiPanelStartServer>();
    auto *startServerPanelPtr = startServerPanel.get();
    state->panels.emplace_back(std::move(startServerPanel));
    consoleView->attachCommunityPanel(communityPanelPtr);
    consoleView->attachSettingsPanel(settingsPanelPtr);
    consoleView->attachBindingsPanel(bindingsPanelPtr);
    consoleView->attachStartServerPanel(startServerPanelPtr);
    communityPanelPtr->bindCallbacks(
        [this](int index) {
            if (consoleView) {
                consoleView->onCommunitySelection(index);
            }
        },
        [this](const std::string &host) {
            if (consoleView) {
                consoleView->onCommunityAddRequested(host);
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->onRefreshRequested();
            }
        },
        [this](int index) {
            if (consoleView) {
                consoleView->onServerSelection(index);
            }
        },
        [this](int index) {
            if (consoleView) {
                consoleView->onJoinRequested(index);
            }
        },
        [this](int index) {
            if (consoleView) {
                consoleView->onRoamRequested(index);
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->hide();
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->onQuitRequested();
            }
        }
    );
    loadHudDocument();
    loadConsoleDocument();

    spdlog::info("UiSystem: RmlUi backend initialized.");
}

RmlUiBackend::~RmlUiBackend() {
    if (!state) {
        return;
    }
    if (state->document) {
        state->document->Close();
        state->document = nullptr;
    }
    if (state->hud) {
        state->hud->unload();
    }
    if (state->context) {
        Rml::RemoveContext(state->context->GetName());
        state->context = nullptr;
    }
    Rml::Shutdown();
}

ui::ConsoleInterface &RmlUiBackend::console() {
    return *consoleView;
}

const ui::ConsoleInterface &RmlUiBackend::console() const {
    return *consoleView;
}

void RmlUiBackend::handleEvents(const std::vector<platform::Event> &events) {
    if (!state || !state->context) {
        return;
    }

    const float renderScale = ui::GetUiRenderScale();
    const bool consoleVisible = consoleView && consoleView->isVisible();
    const bool hudVisible = state->hud && state->hud->isVisible();

    for (const auto &event : events) {
        switch (event.type) {
            case platform::EventType::KeyDown: {
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                if (event.key == platform::Key::R && (mods & Rml::Input::KM_CTRL)) {
                    state->reloadRequested = true;
                    state->reloadArmed = true;
                    if (mods & Rml::Input::KM_SHIFT) {
                        state->hardReloadRequested = true;
                    }
                    break;
                }
                if (!isUiInputEnabled()) {
                    break;
                }
                state->context->ProcessKeyDown(ui::input_mapping::ToRmlKey(event.key), mods);
                break;
            }
            case platform::EventType::KeyUp: {
                if (!isUiInputEnabled()) {
                    break;
                }
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                state->context->ProcessKeyUp(ui::input_mapping::ToRmlKey(event.key), mods);
                break;
            }
            case platform::EventType::TextInput: {
                if (!isUiInputEnabled()) {
                    break;
                }
                if (state->hud && state->hud->consumeSuppressNextChatChar()) {
                    break;
                }
                state->context->ProcessTextInput(static_cast<Rml::Character>(event.codepoint));
                break;
            }
            case platform::EventType::MouseButtonDown: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                state->context->ProcessMouseButtonDown(ui::input_mapping::ToRmlMouseButton(event.mouseButton), mods);
                break;
            }
            case platform::EventType::MouseButtonUp: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                state->context->ProcessMouseButtonUp(ui::input_mapping::ToRmlMouseButton(event.mouseButton), mods);
                break;
            }
            case platform::EventType::MouseMove: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                const int x = static_cast<int>(std::lround(event.x * renderScale));
                const int y = static_cast<int>(std::lround(event.y * renderScale));
                state->context->ProcessMouseMove(x, y, mods);
                break;
            }
            case platform::EventType::MouseScroll: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                const int mods = ui::input_mapping::RmlModsForEvent(event, windowRef);
                state->context->ProcessMouseWheel(-static_cast<float>(event.scrollY), mods);
                break;
            }
            case platform::EventType::WindowFocus: {
                if (!event.focused) {
                    state->context->ProcessMouseLeave();
                }
                break;
            }
            case platform::EventType::WindowResize: {
                const int targetWidth = std::max(1, static_cast<int>(std::lround(event.width * renderScale)));
                const int targetHeight = std::max(1, static_cast<int>(std::lround(event.height * renderScale)));
                state->lastWidth = targetWidth;
                state->lastHeight = targetHeight;
                state->renderInterface.SetViewport(targetWidth, targetHeight);
                state->context->SetDimensions(Rml::Vector2i(targetWidth, targetHeight));
                break;
            }
            case platform::EventType::WindowClose: {
                state->context->ProcessMouseLeave();
                break;
            }
            default:
                break;
        }
    }
}

bool RmlUiBackend::isUiInputEnabled() const {
    if (consoleView && consoleView->isVisible()) {
        return true;
    }
    return state && state->hud && (state->hud->isChatFocused() || state->hud->isQuickMenuVisible());
}

void RmlUiBackend::update() {
    if (!state || !state->context) {
        return;
    }

    const uint64_t revision = karma::config::ConfigStore::Revision();
    if (revision != state->lastConfigRevision) {
        state->lastConfigRevision = revision;
        for (const auto &panel : state->panels) {
            if (panel) {
                panel->configChanged();
            }
        }
    }

    if (rendererBridge && state->hud) {
        state->hud->setRadarTexture(rendererBridge->getRadarTexture());
    }
    if (state->hud) {
        const bool consoleVisible = consoleView && consoleView->isVisible();
        state->hud->setScoreboardEntries(hudModel.scoreboardEntries);
        const bool suppressHud = hudModel.visibility.quickMenu;
        state->hud->setDialogText(hudModel.dialog.text);
        state->hud->setDialogVisible(!suppressHud && hudModel.dialog.visible);
        state->hud->setChatLines(hudModel.chatLines);
        state->hud->setHudBackgroundColor(hudModel.hudBackgroundColor);
        state->hud->setHudTextColor(hudModel.hudTextColor);
        state->hud->setHudTextScale(hudModel.hudTextScale);
        state->hud->setScoreboardVisible(!suppressHud && hudModel.visibility.scoreboard);
        state->hud->setChatVisible(!suppressHud && hudModel.visibility.chat);
        state->hud->setRadarVisible(!suppressHud && hudModel.visibility.radar);
        state->hud->setCrosshairVisible(!suppressHud && hudModel.visibility.crosshair && !consoleVisible);
        state->hud->setFpsVisible(!suppressHud && hudModel.visibility.hud && hudModel.visibility.fps);
        state->hud->setQuickMenuVisible(hudModel.visibility.quickMenu);
        if (!suppressHud && hudModel.visibility.hud && hudModel.visibility.fps) {
            state->hud->setFpsValue(hudModel.fpsValue);
        }
    }

    int fbWidth = 0;
    int fbHeight = 0;
    if (windowRef) { windowRef->getFramebufferSize(fbWidth, fbHeight); }
    const float renderScale = ui::GetUiRenderScale();
    const int targetWidth = std::max(1, static_cast<int>(std::lround(fbWidth * renderScale)));
    const int targetHeight = std::max(1, static_cast<int>(std::lround(fbHeight * renderScale)));
    if (targetWidth != state->lastWidth || targetHeight != state->lastHeight) {
        state->lastWidth = targetWidth;
        state->lastHeight = targetHeight;
        state->renderInterface.SetViewport(targetWidth, targetHeight);
        state->context->SetDimensions(Rml::Vector2i(targetWidth, targetHeight));
    }

    float dpRatio = 1.0f;
    if (windowRef) {
        dpRatio = windowRef->getContentScale();
    }
    const float scaledDpRatio = dpRatio / std::max(renderScale, 0.0001f);
    if (scaledDpRatio != state->lastDpRatio) {
        state->lastDpRatio = scaledDpRatio;
        state->context->SetDensityIndependentPixelRatio(scaledDpRatio);
    }

    const bool consoleVisible = consoleView && consoleView->isVisible();
    if (consoleVisible) {
        if (state->document && !state->document->IsVisible()) {
            state->document->Show();
        }
        if (!state->bodyElement && state->document) {
            state->bodyElement = state->document->GetElementById("main-body");
        }
        if (state->bodyElement) {
            const bool inGame = consoleView->getConnectionState().connected;
            state->bodyElement->SetClass("in-game", inGame);
        }
    } else {
        if (state->document && state->document->IsVisible()) {
            state->document->Hide();
        }
        if (settingsPanel) {
            settingsPanel->clearRenderBrightnessDrag();
        }
    }

    if (state->hud) {
        if (hudModel.visibility.hud) {
            state->hud->show();
        } else {
            state->hud->hide();
        }
    }

    if (state->hud) {
        lastHudRenderState.hudVisible = state->hud->isVisible();
        if (lastHudRenderState.hudVisible) {
            lastHudRenderState.scoreboardVisible = state->hud->isScoreboardVisible();
            lastHudRenderState.chatVisible = state->hud->isChatVisible();
            lastHudRenderState.radarVisible = state->hud->isRadarVisible();
            lastHudRenderState.crosshairVisible = state->hud->isCrosshairVisible();
            lastHudRenderState.fpsVisible = state->hud->isFpsVisible();
            lastHudRenderState.dialogVisible = state->hud->isDialogVisible();
            lastHudRenderState.quickMenuVisible = state->hud->isQuickMenuVisible();
        } else {
            lastHudRenderState.scoreboardVisible = false;
            lastHudRenderState.chatVisible = false;
            lastHudRenderState.radarVisible = false;
            lastHudRenderState.crosshairVisible = false;
            lastHudRenderState.fpsVisible = false;
            lastHudRenderState.dialogVisible = false;
            lastHudRenderState.quickMenuVisible = false;
        }
    } else {
        lastHudRenderState = {};
    }

    const bool anyVisible = (state->document && state->document->IsVisible())
        || (state->hud && state->hud->isVisible());
    state->outputVisible = anyVisible;
    if (anyVisible && !state->reloadRequested && !state->reloadArmed) {
        if (consoleVisible) {
            for (const auto &panel : state->panels) {
                panel->update();
            }
        } else if (state->hud) {
            state->hud->update();
        }
        state->context->Update();
        state->renderInterface.BeginFrame();
        if (!std::getenv("KARMA_RMLUI_DISABLE_RENDER")) {
            state->context->Render();
        }
        state->renderInterface.EndFrame();
    }

    if (state->reloadArmed) {
        state->reloadRequested = true;
        state->reloadArmed = false;
        return;
    }
    if (state->reloadRequested) {
        state->reloadRequested = false;
        if (state->pendingLanguage) {
            karma::i18n::Get().loadLanguage(*state->pendingLanguage);
            state->pendingLanguage.reset();
        }
        loadHudDocument();
        loadConsoleDocument();
    }
}

void RmlUiBackend::reloadFonts() {
    if (!state) {
        return;
    }
    loadHudDocument();
    loadConsoleDocument();
}

void RmlUiBackend::setHudModel(const ui::HudModel &model) {
    hudModel = model;
}

void RmlUiBackend::addConsoleLine(const std::string &playerName, const std::string &line) {
    if (!state || !state->hud) {
        return;
    }
    std::string displayName = playerName;
    if (!displayName.empty() && displayName.front() != '[') {
        displayName = "[" + displayName + "]";
    }
    const std::string fullLine = displayName.empty() ? line : (displayName + " " + line);
    state->hud->addChatLine(fullLine);
}

std::string RmlUiBackend::getChatInputBuffer() const {
    if (!state || !state->hud) {
        return {};
    }
    return state->hud->getSubmittedChatInput();
}

void RmlUiBackend::clearChatInputBuffer() {
    if (!state || !state->hud) {
        return;
    }
    state->hud->clearSubmittedChatInput();
}

void RmlUiBackend::focusChatInput() {
    if (!state || !state->hud) {
        return;
    }
    state->hud->focusChatInput();
}

bool RmlUiBackend::getChatInputFocus() const {
    if (!state || !state->hud) {
        return false;
    }
    return state->hud->isChatFocused();
}

bool RmlUiBackend::consumeKeybindingsReloadRequest() {
    return consoleView && consoleView->consumeKeybindingsReloadRequest();
}

std::optional<ui::QuickMenuAction> RmlUiBackend::consumeQuickMenuAction() {
    if (!state || !state->hud) {
        return std::nullopt;
    }
    return state->hud->consumeQuickMenuAction();
}

void RmlUiBackend::setRendererBridge(const ui::RendererBridge *bridge) {
    rendererBridge = bridge;
}

ui::RenderOutput RmlUiBackend::getRenderOutput() const {
    if (!state) {
        return {};
    }
    if (!state->outputVisible) {
        return {};
    }
    const uint64_t textureId = static_cast<uint64_t>(state->renderInterface.GetOutputTextureId());
    if (textureId == 0) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("RmlUi: output texture id is 0 while outputVisible=true (size={}x{}).",
                         state->renderInterface.GetOutputWidth(),
                         state->renderInterface.GetOutputHeight());
            loggedOnce = true;
        }
        return {};
    }
    const int outputWidth = state->renderInterface.GetOutputWidth();
    const int outputHeight = state->renderInterface.GetOutputHeight();
    const uint32_t width = outputWidth > 0 ? static_cast<uint32_t>(outputWidth) : 0u;
    const uint32_t height = outputHeight > 0 ? static_cast<uint32_t>(outputHeight) : 0u;
    return ui::UiRenderBridge::MakeOutput(textureId,
                                          width,
                                          height,
                                          graphics::TextureFormat::RGBA8_UNORM,
                                          state->outputVisible);
}

float RmlUiBackend::getRenderBrightness() const {
    return settingsPanel ? settingsPanel->getRenderBrightness() : 1.0f;
}

bool RmlUiBackend::isRenderBrightnessDragActive() const {
    return settingsPanel ? settingsPanel->isRenderBrightnessDragActive() : false;
}

void RmlUiBackend::setActiveTab(const std::string &tabKey) {
    if (!state) {
        return;
    }
    auto it = state->tabs.find(tabKey);
    if (it == state->tabs.end()) {
        return;
    }

    const std::string previousTab = state->activeTab;
    state->activeTab = tabKey;
    for (const auto &entry : state->tabs) {
        entry.second->SetClass("active", entry.first == tabKey);
    }
    for (const auto &entry : state->tabPanels) {
        entry.second->SetClass("active", entry.first == tabKey);
    }

    if (state->contentElement && state->tabPanels.find(tabKey) == state->tabPanels.end()) {
        std::string label = tabKey;
        auto labelIt = state->tabLabels.find(tabKey);
        if (labelIt != state->tabLabels.end() && !labelIt->second.empty()) {
            label = labelIt->second;
        }
        const std::string &labelMarkup = cachedTwemojiMarkup(label);
        state->contentElement->SetInnerRML(
            "<div style=\"padding: 8px 0;\">" + labelMarkup + " panel</div>");
    }

    if (previousTab != tabKey) {
        if (auto *panel = findPanelByKey(state->panels, previousTab)) {
            panel->hide();
        }
        if (auto *panel = findPanelByKey(state->panels, tabKey)) {
            panel->show();
        }
    }

    if (previousTab != tabKey && tabKey == "community" && consoleView) {
        consoleView->onRefreshRequested();
    }
}

void RmlUiBackend::loadConfiguredFonts(const std::string &language) {
    if (!state) {
        return;
    }
    if (state->lastFontLanguage != language && !state->loadedFontFiles.empty()) {
        state->loadedFontFiles.clear();
    }
    state->lastFontLanguage = language;
    auto loadFont = [&](const std::filesystem::path &path, bool fallback, const char *label) {
        if (path.empty()) {
            return;
        }
        const std::string pathStr = path.string();
        if (!state->loadedFontFiles.insert(pathStr).second) {
            return;
        }
        if (!Rml::LoadFontFace(pathStr, fallback)) {
            spdlog::warn("RmlUi: failed to load font '{}' (fallback={}).", pathStr, fallback);
        }
    };

    state->regularFontPath.clear();
    state->emojiFontPath.clear();

    const ui::fonts::ConsoleFontAssets assets = ui::fonts::GetConsoleFontAssets(language, true);
    const auto defaultRegularPath = karma::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    if (!defaultRegularPath.empty()) {
        state->regularFontPath = defaultRegularPath.string();
        loadFont(defaultRegularPath, false, "hud.fonts.console.Regular.Font");
    }
    if (assets.selection.regularFontKey != "hud.fonts.console.Regular.Font") {
        const auto languageRegularPath = karma::data::ResolveConfiguredAsset(assets.selection.regularFontKey);
        loadFont(languageRegularPath, true, assets.selection.regularFontKey.c_str());
    }
    const auto titleFontPath = karma::data::ResolveConfiguredAsset(assets.titleKey);
    loadFont(titleFontPath, false, assets.titleKey.c_str());
    const auto headingFontPath = karma::data::ResolveConfiguredAsset(assets.headingKey);
    loadFont(headingFontPath, false, assets.headingKey.c_str());
    const auto buttonFontPath = karma::data::ResolveConfiguredAsset(assets.buttonKey);
    loadFont(buttonFontPath, false, assets.buttonKey.c_str());

    const auto emojiFontPath = karma::data::ResolveConfiguredAsset(assets.emojiKey);
    if (!emojiFontPath.empty()) {
        state->emojiFontPath = emojiFontPath.string();
        loadFont(emojiFontPath, true, assets.emojiKey.c_str());
    }

    for (const auto &extraPath : assets.extraPaths) {
        loadFont(extraPath, false, "assets.hud.fonts.console.Extras");
    }

    for (const auto &key : assets.selection.fallbackKeys) {
        loadFont(karma::data::ResolveConfiguredAsset(key), true, key.c_str());
    }
}

void RmlUiBackend::loadConsoleDocument() {
    if (!state || !state->context) {
        return;
    }

    const std::string previousTab = state->activeTab;
    state->reloadRequested = false;
    state->reloadArmed = false;
    if (state->document) {
        state->document->Close();
        state->document = nullptr;
        // Allow RmlUi to detach listeners and destroy elements before we clear them.
        state->context->Update();
    }

    state->tabs.clear();
    state->tabLabels.clear();
    state->tabListeners.clear();
    state->tabPanels.clear();
    state->contentElement = nullptr;
    state->bodyElement = nullptr;
    state->emojiMarkupCache.clear();

    loadConfiguredFonts(karma::i18n::Get().language());

    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();
    if (state->hardReloadRequested) {
        state->hardReloadRequested = false;
        if (!state->regularFontPath.empty()) {
            Rml::LoadFontFace(state->regularFontPath);
        }
        if (!state->emojiFontPath.empty()) {
            Rml::LoadFontFace(state->emojiFontPath, true);
        }
    }
    state->document = state->context->LoadDocument(state->consolePath);
    if (!state->document) {
        spdlog::error("RmlUi: failed to load console RML from '{}'.", state->consolePath);
        return;
    }
    ui::rmlui::ApplyTranslations(state->document, karma::i18n::Get());
    for (const auto &spec : ui::GetConsoleTabSpecs()) {
        if (!spec.key) {
            continue;
        }
        const std::string elementId = std::string("tab-") + spec.key;
        if (auto *element = state->document->GetElementById(elementId)) {
            const std::string label = tabLabelForSpec(spec);
            element->SetInnerRML(label);
        }
    }

    state->document->Show();
    state->bodyElement = state->document->GetElementById("main-body");
    state->contentElement = state->document->GetElementById("tab-content");
    for (const auto &panel : state->panels) {
        panel->load(state->document);
    }
    Rml::ElementList tabElements;
    std::string defaultTabKey;
    state->document->GetElementsByClassName(tabElements, "tab");
    for (auto *element : tabElements) {
        if (!element) {
            continue;
        }
        const std::string elementId = element->GetId();
        std::string tabKey = elementId;
        const std::string prefix = "tab-";
        if (tabKey.rfind(prefix, 0) == 0) {
            tabKey = tabKey.substr(prefix.size());
        }
        if (tabKey.empty()) {
            continue;
        }
        const std::string labelRaw = element->GetInnerRML();
        state->tabs[tabKey] = element;
        state->tabLabels[tabKey] = labelRaw;
        element->SetInnerRML(cachedTwemojiMarkup(labelRaw));
        if (defaultTabKey.empty() && element->IsClassSet("default")) {
            defaultTabKey = tabKey;
        }
        auto listener = std::make_unique<TabClickListener>(this, tabKey);
        element->AddEventListener("click", listener.get());
        state->tabListeners.emplace_back(std::move(listener));
    }
    for (const auto &entry : state->tabs) {
        const std::string panelId = "panel-" + entry.first;
        if (auto *panel = state->document->GetElementById(panelId)) {
            state->tabPanels[entry.first] = panel;
        }
    }
    if (!state->tabs.empty()) {
        if (!previousTab.empty() && state->tabs.find(previousTab) != state->tabs.end()) {
            setActiveTab(previousTab);
        } else {
            setActiveTab(defaultTabKey.empty() ? state->tabs.begin()->first : defaultTabKey);
        }
    }
}

void RmlUiBackend::loadHudDocument() {
    if (!state || !state->context) {
        return;
    }

    if (state->hud) {
        state->hud->load(state->context, state->hudPath,
            [this](const std::string &text) -> const std::string & {
                return cachedTwemojiMarkup(text);
            });
    }
}

const std::string &RmlUiBackend::cachedTwemojiMarkup(const std::string &text) {
    if (!state) {
        static const std::string empty;
        return empty;
    }
    auto it = state->emojiMarkupCache.find(text);
    if (it != state->emojiMarkupCache.end()) {
        return it->second;
    }
    const std::string markup = ui::renderTextWithTwemoji(text);
    auto [inserted, _] = state->emojiMarkupCache.emplace(text, markup);
    return inserted->second;
}

} // namespace ui

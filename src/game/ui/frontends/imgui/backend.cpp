#include "ui/frontends/imgui/backend.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/i18n.hpp"
#include "karma/platform/window.hpp"
#include "spdlog/spdlog.h"
#include "karma/ui/bridges/ui_render_bridge.hpp"
#include "ui/config/input_mapping.hpp"
#include "ui/fonts/console_fonts.hpp"
#include "ui/config/render_scale.hpp"

#include <cmath>

namespace ui_backend {
namespace {
bool hasOutputDrawData(const ImDrawData* drawData) {
    return drawData && drawData->TotalVtxCount > 0;
}

const char *GetClipboardText(void *user_data) {
    static std::string buffer;
    auto *window = static_cast<platform::Window *>(user_data);
    buffer = window ? window->getClipboardText() : std::string();
    return buffer.c_str();
}

void SetClipboardText(void *user_data, const char *text) {
    auto *window = static_cast<platform::Window *>(user_data);
    if (window) {
        window->setClipboardText(text ? text : "");
    }
}

} // namespace

ImGuiBackend::ImGuiBackend(platform::Window &windowRef) : window(&windowRef) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendPlatformName = "bz3-platform";
    io.SetClipboardTextFn = SetClipboardText;
    io.GetClipboardTextFn = GetClipboardText;
    io.ClipboardUserData = window;

    ImGui::StyleColorsDark();

    io.BackendRendererName = "bz3-imgui";

    spdlog::info("UiSystem: ImGui add default font");
    io.Fonts->AddFontDefault();

    const auto assets = ui::fonts::GetConsoleFontAssets(karma::i18n::Get().language(), true);
    const auto bigFontPath = karma::data::ResolveConfiguredAsset(assets.selection.regularFontKey);
    const std::string bigFontPathStr = bigFontPath.string();
    spdlog::info("UiSystem: ImGui add big font from {}", bigFontPathStr);
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    spdlog::info("UiSystem: ImGui console font init start");
    consoleView.initializeFonts(io);
    spdlog::info("UiSystem: ImGui console font init done");

    hud.setShowFps(hudModel.visibility.fps);

    consoleView.setLanguageCallback([this](const std::string &language) {
#if defined(KARMA_RENDER_BACKEND_BGFX)
        pendingLanguage = language;
        languageReloadArmed = true;
#else
        pendingLanguage = language;
        languageReloadArmed = true;
#endif
    });

    spdlog::info("UiSystem: ImGui font atlas build start");
    io.Fonts->Build();
    spdlog::info("UiSystem: ImGui font atlas build done");
    fontsDirty = true;
}

ImGuiBackend::~ImGuiBackend() {
    ImGui::DestroyContext();
}

void ImGuiBackend::handleEvents(const std::vector<platform::Event> &events) {
    ImGuiIO &io = ImGui::GetIO();
    for (const auto &event : events) {
        switch (event.type) {
            case platform::EventType::KeyDown:
            case platform::EventType::KeyUp: {
                const bool down = (event.type == platform::EventType::KeyDown);
                const ImGuiKey key = ui::input_mapping::ToImGuiKey(event.key);
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, down);
                }
                break;
            }
            case platform::EventType::TextInput: {
                if (event.codepoint != 0) {
                    io.AddInputCharacter(static_cast<unsigned int>(event.codepoint));
                }
                break;
            }
            case platform::EventType::MouseButtonDown:
            case platform::EventType::MouseButtonUp: {
                const bool down = (event.type == platform::EventType::MouseButtonDown);
                const int button = ui::input_mapping::ToImGuiMouseButton(event.mouseButton);
                io.AddMouseButtonEvent(button, down);
                break;
            }
            case platform::EventType::MouseMove: {
                io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
                break;
            }
            case platform::EventType::MouseScroll: {
                io.AddMouseWheelEvent(static_cast<float>(event.scrollX), static_cast<float>(event.scrollY));
                break;
            }
            case platform::EventType::WindowFocus: {
                io.AddFocusEvent(event.focused);
                break;
            }
            default:
                break;
        }
    }
}

void ImGuiBackend::update() {
    if (languageReloadArmed && pendingLanguage) {
        languageReloadArmed = false;
        karma::i18n::Get().loadLanguage(*pendingLanguage);
        pendingLanguage.reset();
        reloadFonts();
    }
    if (consoleView.consumeFontReloadRequest()) {
        reloadFonts();
    }
    if (rendererBridge) {
        hud.setRadarTexture(rendererBridge->getRadarTexture());
    }

    ImGuiIO &io = ImGui::GetIO();

    const auto now = std::chrono::steady_clock::now();
    if (lastFrameTime.time_since_epoch().count() == 0) {
        io.DeltaTime = 1.0f / 60.0f;
    } else {
        const std::chrono::duration<float> dt = now - lastFrameTime;
        io.DeltaTime = dt.count();
    }
    lastFrameTime = now;

    int fbWidth = 0;
    int fbHeight = 0;
    if (window) {
        window->getFramebufferSize(fbWidth, fbHeight);
    }
    const float renderScale = ui::GetUiRenderScale();
    const int targetWidth = std::max(1, static_cast<int>(std::lround(fbWidth * renderScale)));
    const int targetHeight = std::max(1, static_cast<int>(std::lround(fbHeight * renderScale)));
    io.DisplaySize = ImVec2(static_cast<float>(fbWidth), static_cast<float>(fbHeight));
    io.DisplayFramebufferScale = ImVec2(renderScale, renderScale);

    ui::input_mapping::UpdateImGuiModifiers(io, window);
    if (window) {
        window->setCursorVisible(!io.MouseDrawCursor);
    }
    if (uiBridge) {
        uiBridge->ensureImGuiRenderTarget(targetWidth, targetHeight);
    }

    if (uiBridge && fontsDirty) {
        uiBridge->rebuildImGuiFonts(io.Fonts);
        io.FontDefault = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
        fontsDirty = false;
    }
    io.FontGlobalScale = 1.0f;
    ImGui::NewFrame();

    hud.setScoreboardEntries(hudModel.scoreboardEntries);
    hud.setDialogText(hudModel.dialog.text);
    hud.setFpsValue(hudModel.fpsValue);
    hud.setChatLines(hudModel.chatLines);
    const auto &bg = hudModel.hudBackgroundColor;
    hud.setHudBackgroundColor(ImVec4(bg[0], bg[1], bg[2], bg[3]));
    const auto &textColor = hudModel.hudTextColor;
    hud.setHudTextColor(ImVec4(textColor[0], textColor[1], textColor[2], textColor[3]));
    hud.setHudTextScale(hudModel.hudTextScale);

    const bool consoleVisible = consoleView.isVisible();
    const bool hudVisible = hudModel.visibility.hud;
    quickMenuVisible = hudVisible && hudModel.visibility.quickMenu;
    const bool suppressHud = quickMenuVisible;
    if (hudVisible) {
        hud.setScoreboardVisible(!suppressHud && hudModel.visibility.scoreboard);
        hud.setChatVisible(!suppressHud && hudModel.visibility.chat);
        hud.setRadarVisible(!suppressHud && hudModel.visibility.radar);
        hud.setCrosshairVisible(!suppressHud && hudModel.visibility.crosshair && !consoleVisible);
        hud.setShowFps(!suppressHud && hudModel.visibility.fps);
        hud.setDialogVisible(!suppressHud && hudModel.dialog.visible);
        hud.draw(io, bigFont);
    }
    if (consoleVisible) {
        consoleView.draw(io);
    }
    if (quickMenuVisible) {
        const auto &i18n = karma::i18n::Get();
        const std::string title = i18n.get("ui.hud.quick_menu.title");
        const std::string consoleLabel = i18n.get("ui.hud.quick_menu.console");
        const std::string resumeLabel = i18n.get("ui.hud.quick_menu.resume");
        const std::string disconnectLabel = i18n.get("ui.hud.quick_menu.disconnect");
        const std::string quitLabel = i18n.get("ui.hud.quick_menu.quit");
        const std::string windowTitle = title + "###QuickMenu";
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f)
        );
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin(windowTitle.c_str(), nullptr, flags);
        if (ImGui::Button(consoleLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
            pendingQuickMenuAction = ui::QuickMenuAction::OpenConsole;
        }
        if (ImGui::Button(resumeLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
            pendingQuickMenuAction = ui::QuickMenuAction::Resume;
        }
        if (ImGui::Button(disconnectLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
            pendingQuickMenuAction = ui::QuickMenuAction::Disconnect;
        }
        if (ImGui::Button(quitLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
            pendingQuickMenuAction = ui::QuickMenuAction::Quit;
        }
        ImGui::End();
    }

    lastHudRenderState.hudVisible = hudVisible;
    if (hudVisible) {
        lastHudRenderState.scoreboardVisible = hud.isScoreboardVisible();
        lastHudRenderState.chatVisible = hud.isChatVisible();
        lastHudRenderState.radarVisible = hud.isRadarVisible();
        lastHudRenderState.crosshairVisible = hud.isCrosshairVisible();
        lastHudRenderState.fpsVisible = hud.isFpsVisible();
        lastHudRenderState.dialogVisible = hud.isDialogVisible();
        lastHudRenderState.quickMenuVisible = quickMenuVisible;
    } else {
        lastHudRenderState.scoreboardVisible = false;
        lastHudRenderState.chatVisible = false;
        lastHudRenderState.radarVisible = false;
        lastHudRenderState.crosshairVisible = false;
        lastHudRenderState.fpsVisible = false;
        lastHudRenderState.dialogVisible = false;
        lastHudRenderState.quickMenuVisible = false;
    }

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    outputVisible = (consoleVisible || hudVisible) && hasOutputDrawData(drawData);
    if (uiBridge && outputVisible) {
        uiBridge->renderImGuiToTarget(drawData);
    }
}

void ImGuiBackend::reloadFonts() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

    const auto assets = ui::fonts::GetConsoleFontAssets(karma::i18n::Get().language(), true);
    const auto bigFontPath = karma::data::ResolveConfiguredAsset(assets.selection.regularFontKey);
    const std::string bigFontPathStr = bigFontPath.string();
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    consoleView.initializeFonts(io);
    io.Fonts->Build();
    fontsDirty = true;
    if (uiBridge) {
        uiBridge->rebuildImGuiFonts(io.Fonts);
        io.FontDefault = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
        fontsDirty = false;
    }
}

void ImGuiBackend::setHudModel(const ui::HudModel &model) {
    hudModel = model;
}

void ImGuiBackend::addConsoleLine(const std::string &playerName, const std::string &line) {
    hud.addConsoleLine(playerName, line);
}

std::string ImGuiBackend::getChatInputBuffer() const {
    return hud.getChatInputBuffer();
}

void ImGuiBackend::clearChatInputBuffer() {
    hud.clearChatInputBuffer();
}

void ImGuiBackend::focusChatInput() {
    hud.focusChatInput();
}

bool ImGuiBackend::getChatInputFocus() const {
    return hud.getChatInputFocus();
}

bool ImGuiBackend::consumeKeybindingsReloadRequest() {
    return consoleView.consumeKeybindingsReloadRequest();
}

std::optional<ui::QuickMenuAction> ImGuiBackend::consumeQuickMenuAction() {
    if (!pendingQuickMenuAction) {
        return std::nullopt;
    }
    auto action = *pendingQuickMenuAction;
    pendingQuickMenuAction.reset();
    return action;
}

void ImGuiBackend::setRendererBridge(const ui::RendererBridge *bridge) {
    rendererBridge = bridge;
    uiBridge = rendererBridge ? rendererBridge->getUiRenderTargetBridge() : nullptr;
    if (uiBridge) {
        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererName = "bz3-imgui-bridge";
        fontsDirty = true;
    }
}

ui::RenderOutput ImGuiBackend::getRenderOutput() const {
    if (!uiBridge) {
        return {};
    }
    const graphics::TextureHandle texture = uiBridge->getImGuiRenderTarget();
    if (outputVisible && !texture.valid()) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("ImGui: output texture invalid while outputVisible=true (id={}, size={}x{}).",
                         static_cast<unsigned long long>(texture.id),
                         texture.width,
                         texture.height);
            loggedOnce = true;
        }
    }
    return ui::UiRenderBridge::MakeOutput(texture, outputVisible);
}

bool ImGuiBackend::isRenderBrightnessDragActive() const {
    return consoleView.isRenderBrightnessDragActive();
}

ui::ConsoleInterface &ImGuiBackend::console() {
    return consoleView;
}

const ui::ConsoleInterface &ImGuiBackend::console() const {
    return consoleView;
}

} // namespace ui_backend

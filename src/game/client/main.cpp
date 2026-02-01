#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>
#include <cstring>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#include "spdlog/spdlog.h"
#include "karma/app/engine_app.hpp"
#include "game/engine/client_engine.hpp"
#if defined(KARMA_RENDER_BACKEND_BGFX)
#include "karma/graphics/backends/bgfx/backend.hpp"
#endif
#include "client/game.hpp"
#include "client/client_cli_options.hpp"
#include "client/config_client.hpp"
#include "client/server/community_browser_controller.hpp"
#include "client/server/server_connector.hpp"
#include "game/net/messages.hpp"
#include "game/common/data_path_spec.hpp"
#include "game/ui/core/system.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/i18n.hpp"
#include "karma/graphics/ui_render_target_bridge.hpp"
#include "karma/platform/window.hpp"
#include "ui/config/ui_config.hpp"
#include "ui/bridges/renderer_bridge.hpp"

#if !defined(_WIN32)
#include <csignal>
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

constexpr TimeUtils::duration MIN_DELTA_TIME = 1.0f / 120.0f;

namespace {
struct FullscreenState {
    bool active = false;
};

struct QuickStartServer {
#if !defined(_WIN32)
    pid_t pid = -1;
#endif
    ~QuickStartServer() {
#if !defined(_WIN32)
        if (pid > 0) {
            kill(pid, SIGTERM);
        }
#endif
    }
};

struct UiSmokeTimer {
    float elapsed = 0.0f;
    int phase = 0;
};

std::string findServerBinary() {
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
            return candidate.string();
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
                return path.string();
            }
        }
    }

    return {};
}

bool launchQuickStartServer(const ClientCLIOptions &cliOptions, QuickStartServer &server) {
#if defined(_WIN32)
    (void)cliOptions;
    (void)server;
    spdlog::error("dev-quick-start: server auto-launch not supported on Windows yet.");
    return false;
#else
    const std::string serverBinary = findServerBinary();
    if (serverBinary.empty()) {
        spdlog::error("dev-quick-start: bz3-server binary not found.");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("dev-quick-start: failed to fork server process: {}", std::strerror(errno));
        return false;
    }

    if (pid == 0) {
        std::vector<std::string> args;
        args.push_back(serverBinary);
        args.push_back("-p");
        args.push_back(std::to_string(cliOptions.connectPort));
        args.push_back("-D");
        if (cliOptions.dataDirExplicit && !cliOptions.dataDir.empty()) {
            args.push_back("-d");
            args.push_back(cliOptions.dataDir);
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

    server.pid = pid;
    spdlog::info("dev-quick-start: launched bz3-server (pid {}) on port {}", pid, cliOptions.connectPort);
    return true;
#endif
}

class ClientLoopAdapter final : public karma::app::GameInterface {
public:
    ClientLoopAdapter(platform::Window &window,
                      ClientEngine &engine,
                      ServerConnector &serverConnector,
                      CommunityBrowserController &communityBrowser,
                      ClientCLIOptions cliOptions,
                      std::unique_ptr<Game> &game)
        : window_(window),
          engine_(engine),
          serverConnector_(serverConnector),
          communityBrowser_(communityBrowser),
          cliOptions_(std::move(cliOptions)),
          game_(game) {}

    void onStart() override {
        auto *ctx = context();
        if (!ctx) {
            initOk_ = false;
            return;
        }
        lastConfigRevision_ = karma::config::ConfigStore::Revision();
        lastVsyncEnabled_ = karma::config::ReadBoolConfig({"graphics.VSync"}, true);
        if (cliOptions_.ecsSmokeTest) {
            engine_.ui->console().hide();
            engine_.ui->setQuickMenuVisible(false);
        }
        if (cliOptions_.devQuickStart) {
            engine_.ui->console().show({});
            if (launchQuickStartServer(cliOptions_, quickStartServer_)) {
                quickStartPending_ = true;
                quickStartLastAttempt_ = TimeUtils::GetCurrentTime();
                quickStartInitialDelayDone_ = false;
            }
        } else if (cliOptions_.addrExplicit) {
            engine_.setRoamingModeSession(false);
            serverConnector_.connect(cliOptions_.connectAddr,
                                     cliOptions_.connectPort,
                                     cliOptions_.playerName,
                                     false,
                                     false,
                                     false);
        }
        if (!initOk_ && ctx->window) {
            ctx->window->requestClose();
        }
    }

    void onUpdate(float dt) override {
        if (!initOk_) {
            return;
        }
        const uint64_t configRevision = karma::config::ConfigStore::Revision();
        if (configRevision != lastConfigRevision_) {
            lastConfigRevision_ = configRevision;
            const bool vsyncEnabled = karma::config::ReadBoolConfig({"graphics.VSync"}, true);
            if (vsyncEnabled != lastVsyncEnabled_) {
                window_.setVsync(vsyncEnabled);
                lastVsyncEnabled_ = vsyncEnabled;
            }
        }
        lastDt_ = dt;
        const float effectiveDt = dt < MIN_DELTA_TIME ? MIN_DELTA_TIME : dt;
        if (dt < MIN_DELTA_TIME) {
            TimeUtils::sleep(MIN_DELTA_TIME - dt);
        }

        engine_.earlyUpdate(effectiveDt);

        for (const auto &joinResp : engine_.network->consumeMessages<ServerMsg_JoinResponse>()) {
            serverConnector_.handleJoinResponse(joinResp);
        }

        if (quickStartPending_ && !engine_.network->isConnected()) {
            const TimeUtils::time now = TimeUtils::GetCurrentTime();
            if (!quickStartInitialDelayDone_) {
                if (TimeUtils::GetElapsedTime(quickStartLastAttempt_, now) >= quickStartInitialDelay_) {
                    quickStartInitialDelayDone_ = true;
                    quickStartLastAttempt_ = now;
                }
            }
            if (quickStartInitialDelayDone_ &&
                TimeUtils::GetElapsedTime(quickStartLastAttempt_, now) >= quickStartRetryDelay_) {
                quickStartLastAttempt_ = now;
                ++quickStartAttempts_;
                engine_.setRoamingModeSession(false);
                if (serverConnector_.connect("localhost",
                                             cliOptions_.connectPort,
                                             cliOptions_.playerName,
                                             false,
                                             false,
                                             false)) {
                    quickStartPending_ = false;
                } else if (quickStartAttempts_ >= quickStartMaxAttempts_) {
                    spdlog::error("dev-quick-start: failed to connect after {} attempts.", quickStartAttempts_);
                    quickStartPending_ = false;
                }
            }
        }

        const bool graveDown = window_.isKeyDown(platform::Key::GraveAccent);
        if (graveDown && !prevGraveDown_) {
            if (game_) {
                auto &console = engine_.ui->console();
                if (console.isVisible()) {
                    console.hide();
                } else {
                    console.show({});
                }
            }
        }
        prevGraveDown_ = graveDown;

        if (engine_.ui->console().consumeQuitRequest()) {
            if (game_) {
                suppressDisconnectDialog_ = true;
                engine_.network->disconnect("Disconnected from server.");
            }
        }
        if (auto action = engine_.ui->consumeQuickMenuAction()) {
            switch (*action) {
            case ui::QuickMenuAction::OpenConsole:
                engine_.ui->setQuickMenuVisible(false);
                engine_.ui->console().show({});
                break;
            case ui::QuickMenuAction::Resume:
                engine_.ui->setQuickMenuVisible(false);
                break;
            case ui::QuickMenuAction::Disconnect:
                if (game_) {
                    suppressDisconnectDialog_ = true;
                    engine_.network->disconnect("Disconnected from server.");
                }
                engine_.ui->setQuickMenuVisible(false);
                break;
            case ui::QuickMenuAction::Quit:
                window_.requestClose();
                break;
            }
        }

        if (auto disconnectEvent = engine_.network->consumeDisconnectEvent()) {
            if (game_) {
                game_.reset();
            }
            communityBrowser_.handleDisconnected(disconnectEvent->reason);
            if (!suppressDisconnectDialog_
                && !serverConnector_.consumeSuppressDisconnectDialog()
                && !serverConnector_.consumeJoinRejectionDialogShown()) {
                const std::string &reason = disconnectEvent->reason;
                std::string message;
                if (reason.find("Name already in use") != std::string::npos) {
                    message = "That name is already in use. Please choose a different name.";
                } else if (reason.find("Protocol version mismatch") != std::string::npos) {
                    message = "Client/server versions don't match. Please rebuild both.";
                } else if (reason.find("Join request required") != std::string::npos) {
                    message = "Join rejected by server. Please try again.";
                } else if (reason.find("Join request mismatch") != std::string::npos) {
                    message = "Join rejected by server. Please try again.";
                } else if (reason.find("Connection lost") != std::string::npos) {
                    message = "Connection lost. Please check your network and try again.";
                } else if (reason.find("timeout") != std::string::npos) {
                    message = "Connection timed out. Please try again.";
                } else if (reason.find("Disconnected from server") != std::string::npos) {
                    message = "Connection closed by server.";
                } else if (!reason.empty()) {
                    message = "Disconnected: " + reason;
                } else {
                    message = "Disconnected from server.";
                }
                engine_.ui->console().showErrorDialog(message);
            }
            suppressDisconnectDialog_ = false;
        }

        const bool consoleVisible = engine_.ui->console().isVisible();
        if (cliOptions_.uiSmokeTest) {
            updateUiSmokeTest(dt);
        }
        if (!consoleVisible && engine_.getInputState().toggleFullscreen) {
            const bool wasFullscreen = window_.isFullscreen();
            spdlog::info("Fullscreen toggle requested (before={})", wasFullscreen);
            window_.setFullscreen(!wasFullscreen);
            const bool nowFullscreen = window_.isFullscreen();
            spdlog::info("Fullscreen toggle complete (after={})", nowFullscreen);
            if (nowFullscreen == wasFullscreen) {
                spdlog::warn("Fullscreen toggle had no effect");
            }
        }
        if (consoleVisible) {
            communityBrowser_.update();
        }
        if (game_) {
            game_->earlyUpdate(dt);
        }

        engine_.step(effectiveDt);
        if (game_) {
            game_->lateUpdate(lastDt_);
        }
        engine_.lateUpdate(lastDt_);
    }

    bool shouldQuit() const override { return window_.shouldClose(); }

private:
    platform::Window &window_;
    ClientEngine &engine_;
    ServerConnector &serverConnector_;
    CommunityBrowserController &communityBrowser_;
    ClientCLIOptions cliOptions_;
    std::unique_ptr<Game> &game_;
    QuickStartServer quickStartServer_;
    UiSmokeTimer uiSmokeTimer_;
    bool quickStartPending_ = false;
    int quickStartAttempts_ = 0;
    TimeUtils::time quickStartLastAttempt_ = TimeUtils::GetCurrentTime();
    bool prevGraveDown_ = false;
    const float quickStartRetryDelay_ = 0.5f;
    const float quickStartInitialDelay_ = 1.0f;
    const int quickStartMaxAttempts_ = 20;
    float lastDt_ = 0.0f;
    uint64_t lastConfigRevision_ = 0;
    bool lastVsyncEnabled_ = true;
    bool initOk_ = true;
    bool suppressDisconnectDialog_ = false;
    bool quickStartInitialDelayDone_ = true;
    void updateUiSmokeTest(float dt) {
        uiSmokeTimer_.elapsed += dt;
        if (uiSmokeTimer_.elapsed < 2.0f) {
            return;
        }
        uiSmokeTimer_.elapsed = 0.0f;
        uiSmokeTimer_.phase = (uiSmokeTimer_.phase + 1) % 6;
        switch (uiSmokeTimer_.phase) {
        case 0:
            ui::UiConfig::SetHudScoreboard(true);
            ui::UiConfig::SetHudChat(true);
            ui::UiConfig::SetHudRadar(true);
            ui::UiConfig::SetHudFps(false);
            ui::UiConfig::SetHudCrosshair(true);
            engine_.ui->setDialogVisible(false);
            spdlog::info("ui-smoke: baseline HUD on");
            break;
        case 1:
            ui::UiConfig::SetHudScoreboard(false);
            spdlog::info("ui-smoke: scoreboard off");
            break;
        case 2:
            ui::UiConfig::SetHudChat(false);
            spdlog::info("ui-smoke: chat off");
            break;
        case 3:
            ui::UiConfig::SetHudRadar(false);
            spdlog::info("ui-smoke: radar off");
            break;
        case 4:
            ui::UiConfig::SetHudFps(true);
            spdlog::info("ui-smoke: fps on");
            break;
        case 5:
            engine_.ui->setDialogText("UI smoke test");
            engine_.ui->setDialogVisible(true);
            spdlog::info("ui-smoke: dialog on");
            break;
        default:
            break;
        }
    }
};
}

namespace {

graphics::MeshData MakeTestCube(float halfExtent) {
    const float h = halfExtent;
    graphics::MeshData mesh;
    mesh.vertices = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h},
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}
    };
    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        0, 3, 7, 0, 7, 4
    };
    return mesh;
}

class TestUiBridge final : public ui::RendererBridge {
public:
    explicit TestUiBridge(graphics_backend::UiRenderTargetBridge* backendBridge)
        : backendBridge_(backendBridge) {
        if (backendBridge_) {
            uiBridge_ = std::make_unique<RendererUiBridge>(backendBridge_);
        }
    }

    graphics::TextureHandle getRadarTexture() const override {
        return graphics::TextureHandle{};
    }

    ui::UiRenderTargetBridge* getUiRenderTargetBridge() const override {
        return uiBridge_.get();
    }

private:
    class RendererUiBridge final : public ui::UiRenderTargetBridge {
    public:
        explicit RendererUiBridge(graphics_backend::UiRenderTargetBridge* bridge)
            : bridge_(bridge) {}

        void* toImGuiTextureId(const graphics::TextureHandle& texture) const override {
            return bridge_ ? bridge_->toImGuiTextureId(texture) : nullptr;
        }

        void rebuildImGuiFonts(ImFontAtlas* atlas) override {
            if (bridge_) {
                bridge_->rebuildImGuiFonts(atlas);
            }
        }

        void renderImGuiToTarget(ImDrawData* drawData) override {
            if (bridge_) {
                bridge_->renderImGuiToTarget(drawData);
            }
        }

        bool isImGuiReady() const override {
            return bridge_ && bridge_->isImGuiReady();
        }

        void ensureImGuiRenderTarget(int width, int height) override {
            if (bridge_) {
                bridge_->ensureImGuiRenderTarget(width, height);
            }
        }

        graphics::TextureHandle getImGuiRenderTarget() const override {
            return bridge_ ? bridge_->getImGuiRenderTarget() : graphics::TextureHandle{};
        }

    private:
        graphics_backend::UiRenderTargetBridge* bridge_ = nullptr;
    };

    graphics_backend::UiRenderTargetBridge* backendBridge_ = nullptr;
    std::unique_ptr<RendererUiBridge> uiBridge_;
};

class Test3DAdapter final : public karma::app::GameInterface {
public:
    Test3DAdapter(platform::Window &window, bool useWorld)
        : window_(window), useWorld_(useWorld) {}

    void onStart() override {
        auto *ctx = context();
        if (!ctx || !ctx->ecsWorld) {
            return;
        }
        auto *world = ctx->ecsWorld;
        const ecs::EntityId cube = world->createEntity();
        ecs::Transform cubeXform{};
        cubeXform.scale = {2.0f, 2.0f, 2.0f};
        world->set(cube, cubeXform);
        if (useWorld_) {
            ecs::MeshComponent mesh{};
            mesh.mesh_key = karma::data::Resolve("common/models/world.glb").string();
            world->set(cube, mesh);
        } else {
            ecs::ProceduralMesh proc{};
            proc.mesh = MakeTestCube(1.0f);
            proc.dirty = true;
            world->set(cube, proc);
        }

        cameraEntity_ = world->createEntity();
        ecs::Transform camXform{};
        camXform.position = {0.0f, 2.0f, 6.0f};
        const glm::vec3 target{0.0f, 0.0f, 0.0f};
        const glm::mat4 view = glm::lookAt(camXform.position, target, glm::vec3(0.0f, 1.0f, 0.0f));
        camXform.rotation = glm::quat_cast(glm::inverse(view));
        world->set(cameraEntity_, camXform);
        ecs::CameraComponent camera{};
        camera.is_primary = true;
        camera.fov_degrees = karma::config::ReadRequiredFloatConfig("graphics.Camera.FovDegrees");
        camera.near_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.NearPlane");
        camera.far_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.FarPlane");
        world->set(cameraEntity_, camera);
    }

    void onUpdate(float /*dt*/) override {}

    bool shouldQuit() const override { return window_.shouldClose(); }

private:
    platform::Window &window_;
    ecs::EntityId cameraEntity_ = ecs::kInvalidEntity;
    bool useWorld_ = false;
};

} // namespace

spdlog::level::level_enum ParseLogLevel(const std::string &level) {
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "info") {
        return spdlog::level::info;
    }
    if (level == "warn") {
        return spdlog::level::warn;
    }
    if (level == "err") {
        return spdlog::level::err;
    }
    if (level == "critical") {
        return spdlog::level::critical;
    }
    if (level == "off") {
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

void ConfigureLogging(spdlog::level::level_enum level, bool includeTimestamp) {
    if (includeTimestamp) {
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    } else {
        spdlog::set_pattern("[%^%l%$] %v");
    }
    spdlog::set_level(level);
}

void SetEnvOverride(const char *name, const std::string &value) {
    if (!name || value.empty()) {
        return;
    }
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
    spdlog::info("Env override set: {}={}", name, value);
}

int main(int argc, char *argv[]) {
    ConfigureLogging(spdlog::level::info, false);

    game_common::ConfigureDataPathSpec();

    const karma::data::DataDirOverrideResult dataDirResult = karma::data::ApplyDataDirOverrideFromArgs(argc, argv);

    const auto clientUserConfigPathFs = dataDirResult.userConfigPath;
    const std::vector<karma::config::ConfigFileSpec> clientConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"client/config.json", "data/client/config.json", spdlog::level::err, true, true}
    };
    karma::config::ConfigStore::Initialize(clientConfigSpecs, clientUserConfigPathFs);
    const ClientCLIOptions cliOptions = ParseClientCLIOptions(argc, argv);
    const auto configIssues = karma::config::ValidateRequiredKeys(karma::config::ClientRequiredKeys());
    if (!configIssues.empty()) {
        if (cliOptions.strictConfig) {
            spdlog::error("Config validation failed:");
        } else {
            spdlog::warn("Config validation reported issues:");
        }
        for (const auto &issue : configIssues) {
            if (cliOptions.strictConfig) {
                spdlog::error("  {}: {}", issue.path, issue.message);
            } else {
                spdlog::warn("  {}: {}", issue.path, issue.message);
            }
        }
        if (cliOptions.strictConfig) {
            return 1;
        }
    }
    karma::i18n::Get().loadFromConfig();

    const uint16_t configWidth = karma::config::ReadUInt16Config({"graphics.resolution.Width"}, 1280);
    const uint16_t configHeight = karma::config::ReadUInt16Config({"graphics.resolution.Height"}, 720);
    const bool fullscreenEnabled = karma::config::ReadBoolConfig({"graphics.Fullscreen"}, false);
    const bool vsyncEnabled = karma::config::ReadBoolConfig({"graphics.VSync"}, true);
    const std::string windowTitle = karma::config::ReadStringConfig({"platform.WindowTitle"}, "BZFlag v3");

    if (cliOptions.languageExplicit && !cliOptions.language.empty()) {
        karma::i18n::Get().loadLanguage(cliOptions.language);
    }
    if (cliOptions.themeExplicit && !cliOptions.theme.empty()) {
        SetEnvOverride("KARMA_BGFX_THEME", cliOptions.theme);
    }
    const spdlog::level::level_enum logLevel = cliOptions.logLevelExplicit
        ? ParseLogLevel(cliOptions.logLevel)
        : (cliOptions.verbose >= 2 ? spdlog::level::trace
           : cliOptions.verbose == 1 ? spdlog::level::debug
           : spdlog::level::info);
    ConfigureLogging(logLevel, cliOptions.timestampLogging);

    const std::string clientUserConfigPath = clientUserConfigPathFs.string();
    ClientConfig clientConfig = ClientConfig::Load("");

    const std::string initialWorldDir = (cliOptions.worldExplicit && !cliOptions.worldDir.empty())
        ? cliOptions.worldDir
        : karma::data::Resolve("client-test").string();

    platform::WindowConfig windowConfig;
    windowConfig.width = configWidth;
    windowConfig.height = configHeight;
    windowConfig.title = windowTitle;
    windowConfig.preferredVideoDriver = karma::config::ReadStringConfig({"platform.SdlVideoDriver"}, "");
    auto window = platform::CreateWindow(windowConfig);
    if (!window || !window->nativeHandle()) {
        spdlog::error("Window failed to create");
        return 1;
    }
    window->setVsync(vsyncEnabled);

    if (cliOptions.test3d) {
        engine::renderer::RendererCore rendererCore(*window);
        karma::app::EngineApp app;
        app.context().window = window.get();
        app.context().rendererCore = &rendererCore;
        std::unique_ptr<UiSystem> testUi;
        std::unique_ptr<TestUiBridge> testUiBridge;
        if (cliOptions.test3dUi) {
            testUi = std::make_unique<UiSystem>(*window);
            testUiBridge = std::make_unique<TestUiBridge>(rendererCore.device().getUiRenderTargetBridge());
            testUi->setRendererBridge(testUiBridge.get());
            app.context().overlay = testUi.get();
        }
        {
            auto &config = app.config();
            config.enable_ecs_render_sync = true;
            config.enable_ecs_camera_sync = true;
            config.enable_ecs_audio_sync = false;
            config.enable_ecs_physics_sync = false;
        }
        Test3DAdapter adapter(*window, cliOptions.test3dWorld);
        if (!app.start(adapter, app.config())) {
            return 1;
        }
        while (app.isRunning()) {
            app.tick();
        }
        return 0;
    }

    ClientEngine engine(*window);
    spdlog::trace("ClientEngine initialized successfully");

    if (fullscreenEnabled) {
        window->setFullscreen(true);
    }

    std::unique_ptr<Game> game;
    ServerConnector serverConnector(engine, cliOptions.playerName, initialWorldDir, game);
    CommunityBrowserController communityBrowser(
        engine,
        clientConfig,
        clientUserConfigPath,
        serverConnector);

    spdlog::trace("Starting main loop");

    ClientLoopAdapter adapter(*window, engine, serverConnector, communityBrowser, cliOptions, game);
    karma::app::EngineApp app;
    app.context().window = window.get();
    app.context().input = engine.input;
    app.context().audio = engine.audio;
    app.context().physics = engine.physics;
    app.context().overlay = engine.ui;
    engine.ecsWorld = app.context().ecsWorld;
    engine.render->setEcsWorld(app.context().ecsWorld);
    app.context().rendererCore = engine.render->getRendererCore();
    if (!cliOptions.ecsSmokeTest) {
        auto *ecsWorld = app.context().ecsWorld;
        if (ecsWorld) {
            engine.cameraEntity = ecsWorld->createEntity();
            ecs::Transform camXform{};
            camXform.position = {0.0f, 2.0f, 6.0f};
            ecsWorld->set(engine.cameraEntity, camXform);
            ecs::CameraComponent camera{};
            camera.is_primary = true;
            camera.fov_degrees = karma::config::ReadRequiredFloatConfig("graphics.Camera.FovDegrees");
            camera.near_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.NearPlane");
            camera.far_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.FarPlane");
            ecsWorld->set(engine.cameraEntity, camera);
            ecsWorld->set(engine.cameraEntity, ecs::AudioListenerComponent{});
        }
    }
    {
        auto &config = app.config();
        config.enable_ecs_render_sync = true;
        config.enable_ecs_camera_sync = true;
        config.enable_ecs_audio_sync = true;
        spdlog::info("ECS render/camera/audio sync enabled (default)");
    }
    if (cliOptions.ecsSmokeTest) {
        auto &config = app.config();
        config.enable_ecs_physics_sync = false;
        config.enable_ecs_audio_sync = false;
        spdlog::info("ECS smoke test enabled (render + camera sync)");

        auto *ecsWorld = app.context().ecsWorld;
        if (ecsWorld) {
            const auto worldEntity = ecsWorld->createEntity();
            ecs::Transform worldXform{};
            worldXform.scale = {2.0f, 2.0f, 2.0f};
            ecsWorld->set(worldEntity, worldXform);
            ecs::MeshComponent worldMesh{};
            worldMesh.mesh_key = karma::data::Resolve("common/models/tank_final.glb").string();
            ecsWorld->set(worldEntity, worldMesh);

            const auto cameraEntity = ecsWorld->createEntity();
            ecs::Transform camXform{};
            camXform.position = {0.0f, 8.0f, 22.0f};
            const glm::vec3 target{0.0f, 0.0f, 0.0f};
            const glm::mat4 view = glm::lookAt(camXform.position, target, glm::vec3(0.0f, 1.0f, 0.0f));
            camXform.rotation = glm::quat_cast(glm::inverse(view));
            ecsWorld->set(cameraEntity, camXform);
            ecs::CameraComponent camera{};
            camera.is_primary = true;
            camera.fov_degrees = karma::config::ReadRequiredFloatConfig("graphics.Camera.FovDegrees");
            camera.near_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.NearPlane");
            camera.far_plane = karma::config::ReadRequiredFloatConfig("graphics.Camera.FarPlane");
            ecsWorld->set(cameraEntity, camera);
            ecsWorld->set(cameraEntity, ecs::AudioListenerComponent{});
        }
    }
    spdlog::info("EngineApp loop enabled (start/tick)");
    if (!app.start(adapter, app.config())) {
        return 1;
    }
    while (app.isRunning()) {
        app.tick();
    }
    return 0;
}

#include "server/runtime/internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <atomic>
#include <csignal>
#include <string>

namespace bz3::server::runtime_detail {

namespace {

std::atomic<bool> g_running{true};
std::string g_app_name = "server";

void OnSignal(int signum) {
    KARMA_TRACE("engine.server", "{}: received signal {}, requesting stop", g_app_name, signum);
    g_running.store(false);
}

} // namespace

void InstallSignalHandlers(std::string app_name) {
    g_app_name = app_name.empty() ? std::string("server") : std::move(app_name);
    g_running.store(true);
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
}

bool ShouldKeepRunning() {
    return g_running.load();
}

} // namespace bz3::server::runtime_detail

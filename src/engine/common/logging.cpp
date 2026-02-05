#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>
#include <cctype>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <unordered_map>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace karma::logging {

namespace {

std::mutex trace_mutex;
std::unordered_set<std::string> enabled_trace_channels;
std::atomic<bool> trace_enabled{false};
std::mutex pattern_mutex;
std::string default_pattern;
std::string named_pattern;
std::once_flag trace_color_once;
bool trace_color_enabled = false;

std::string trimCopy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start == value.size()) {
        return {};
    }
    size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

} // namespace

void ConfigureLogPatterns(bool timestamp_logging) {
    const char* base = "[%^%l%$] %v";
    const char* named = "[%^%l%$][%n] %v";
    const char* base_ts = "%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v";
    const char* named_ts = "%Y-%m-%d %H:%M:%S.%e [%^%l%$][%n] %v";

    std::lock_guard<std::mutex> lock(pattern_mutex);
    default_pattern = timestamp_logging ? base_ts : base;
    named_pattern = timestamp_logging ? named_ts : named;
    spdlog::set_pattern(default_pattern);
    if (auto base_logger = spdlog::default_logger()) {
        base_logger->set_pattern(default_pattern);
    }
}

const char* TraceCategoryColor(const std::string& category) {
    std::call_once(trace_color_once, []() {
#ifndef _WIN32
        trace_color_enabled = (isatty(fileno(stdout)) != 0);
#else
        trace_color_enabled = false;
#endif
    });
    if (!trace_color_enabled) {
        return "";
    }
    const auto dot = category.find('.');
    const std::string top = dot == std::string::npos ? category : category.substr(0, dot);
    static const std::unordered_map<std::string, const char*> colors = {
        {"ui", "\x1b[95m"},
        {"render", "\x1b[91m"},
        {"console", "\x1b[93m"},
        {"config", "\x1b[96m"},
        {"net", "\x1b[92m"}
    };
    auto it = colors.find(top);
    return it == colors.end() ? "" : it->second;
}

void EnableTraceChannels(const std::string& list) {
    if (list.empty()) {
        return;
    }
    trace_enabled.store(true, std::memory_order_relaxed);
    auto base = spdlog::default_logger();
    if (!base) {
        return;
    }
    const auto& sinks = base->sinks();

    {
        std::lock_guard<std::mutex> lock(trace_mutex);
        enabled_trace_channels.clear();
    }

    std::stringstream ss(list);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const std::string name = trimCopy(token);
        if (name.empty()) {
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(trace_mutex);
            enabled_trace_channels.insert(name);
        }
        auto logger = spdlog::get(name);
        if (!logger) {
            logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
            spdlog::register_logger(logger);
        }
        logger->set_level(spdlog::level::trace);
    }
}

bool ShouldTraceChannel(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (!trace_enabled.load(std::memory_order_relaxed)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(trace_mutex);
    return enabled_trace_channels.find(name) != enabled_trace_channels.end();
}

spdlog::logger* GetTraceLogger(const std::string& name) {
    if (name.empty()) {
        return nullptr;
    }
    auto logger = spdlog::get(name);
    if (logger) {
        return logger.get();
    }
    auto base = spdlog::default_logger();
    if (!base) {
        return nullptr;
    }
    const auto& sinks = base->sinks();
    auto created = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    created->set_level(spdlog::level::off);
    spdlog::register_logger(created);
    return created.get();
}

const char* GetDefaultTraceChannelsHelp() {
    return "    platform.sdl\n"
           "    platform.sdl.resize\n"
           "    engine.app\n"
           "    engine.client\n"
           "    engine.server\n"
           "    ui\n"
           "    ui.rmlui\n"
           "    ui.rmlui.bgfx\n"
           "    ui.rmlui.fonts\n"
           "    ui.rmlui.resize\n"
           "    ui.imgui\n"
           "    audio\n"
           "    config\n"
           "    world\n"
           "    physics.jolt\n"
           "    render\n"
           "    render.frame\n"
           "    render.bgfx\n"
           "    render.bgfx.internal\n"
           "    render.bgfx.ui\n"
           "    render.mesh\n"
           "    render.diligent\n"
           "    render.diligent.radar\n"
           "    render.diligent.vulkan\n"
           "    render.diligent.ui\n"
           "    game.client\n"
           "    game.radar\n"
           "    net.client\n"
           "    net.server\n\n";
}

} // namespace karma::logging

#include "ui/core/validation.hpp"

#include "spdlog/spdlog.h"

namespace {

uint64_t hashState(const ui::HudRenderState &expected,
                   const ui::HudRenderState &actual) {
    auto pack = [](const ui::HudRenderState &state) {
        return (state.hudVisible ? 1u : 0u)
            | (state.scoreboardVisible ? 1u << 1u : 0u)
            | (state.chatVisible ? 1u << 2u : 0u)
            | (state.radarVisible ? 1u << 3u : 0u)
            | (state.crosshairVisible ? 1u << 4u : 0u)
            | (state.fpsVisible ? 1u << 5u : 0u)
            | (state.dialogVisible ? 1u << 6u : 0u)
            | (state.quickMenuVisible ? 1u << 7u : 0u);
    };

    const uint64_t expectedBits = pack(expected);
    const uint64_t actualBits = pack(actual);
    return (expectedBits << 32u) | actualBits;
}

void logMismatch(std::string_view backendName,
                 const char *label,
                 bool expected,
                 bool actual,
                 ui::HudValidationResult &result) {
    if (expected == actual) {
        return;
    }
    result.matches = false;
    result.mismatchCount++;
    spdlog::warn("UiValidation [{}]: {} expected={} actual={}", backendName, label, expected, actual);
}

} // namespace

namespace ui {

HudRenderState BuildExpectedHudState(const HudModel &model, bool consoleVisible) {
    HudRenderState expected;
    expected.hudVisible = model.visibility.hud;
    if (!expected.hudVisible) {
        return expected;
    }

    if (model.visibility.quickMenu) {
        expected.quickMenuVisible = true;
        return expected;
    }

    expected.scoreboardVisible = model.visibility.scoreboard;
    expected.chatVisible = model.visibility.chat;
    expected.radarVisible = model.visibility.radar;
    expected.crosshairVisible = model.visibility.crosshair && !consoleVisible;
    expected.fpsVisible = model.visibility.fps;
    expected.dialogVisible = model.dialog.visible;
    expected.quickMenuVisible = model.visibility.quickMenu;
    return expected;
}

HudValidationResult ValidateHudState(const HudRenderState &expected,
                                     const HudRenderState &actual,
                                     std::string_view backendName) {
    HudValidationResult result{};
    logMismatch(backendName, "hudVisible", expected.hudVisible, actual.hudVisible, result);
    logMismatch(backendName, "scoreboardVisible", expected.scoreboardVisible, actual.scoreboardVisible, result);
    logMismatch(backendName, "chatVisible", expected.chatVisible, actual.chatVisible, result);
    logMismatch(backendName, "radarVisible", expected.radarVisible, actual.radarVisible, result);
    logMismatch(backendName, "crosshairVisible", expected.crosshairVisible, actual.crosshairVisible, result);
    logMismatch(backendName, "fpsVisible", expected.fpsVisible, actual.fpsVisible, result);
    logMismatch(backendName, "dialogVisible", expected.dialogVisible, actual.dialogVisible, result);
    logMismatch(backendName, "quickMenuVisible", expected.quickMenuVisible, actual.quickMenuVisible, result);
    return result;
}

void HudValidator::validate(const HudRenderState &expected,
                            const HudRenderState &actual,
                            std::string_view backendName) {
    const uint64_t nextHash = hashState(expected, actual);
    if (nextHash == lastHash) {
        return;
    }
    lastHash = nextHash;
    ValidateHudState(expected, actual, backendName);
}

} // namespace ui

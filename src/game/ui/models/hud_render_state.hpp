#pragma once

namespace ui {

struct HudRenderState {
    bool hudVisible = false;
    bool scoreboardVisible = false;
    bool chatVisible = false;
    bool radarVisible = false;
    bool crosshairVisible = false;
    bool fpsVisible = false;
    bool dialogVisible = false;
    bool quickMenuVisible = false;
};

} // namespace ui

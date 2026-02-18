#pragma once

#include "karma/window/events.hpp"

namespace window {
class Window;
}

#if defined(KARMA_UI_BACKEND_IMGUI)
#include <imgui.h>
#endif

#if defined(KARMA_UI_BACKEND_RMLUI)
#include <RmlUi/Core/Input.h>
#endif

namespace ui::input::mapping {

#if defined(KARMA_UI_BACKEND_IMGUI)
ImGuiKey ToImGuiKey(window::Key key);
int ToImGuiMouseButton(window::MouseButton button);
void UpdateImGuiModifiers(ImGuiIO &io, window::Window *window);
#endif

#if defined(KARMA_UI_BACKEND_RMLUI)
Rml::Input::KeyIdentifier ToRmlKey(window::Key key);
int ToRmlMouseButton(window::MouseButton button);
int ToRmlMods(const window::Modifiers &mods);
int CurrentRmlMods(window::Window *window);
int RmlModsForEvent(const window::Event &event, window::Window *window);
#endif

} // namespace ui::input::mapping

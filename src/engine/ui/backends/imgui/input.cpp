#include "ui/backends/imgui/internal.hpp"

#if defined(KARMA_HAS_IMGUI)

namespace karma::ui::imgui {

ImGuiKey MapKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return ImGuiKey_A;
        case platform::Key::B: return ImGuiKey_B;
        case platform::Key::C: return ImGuiKey_C;
        case platform::Key::D: return ImGuiKey_D;
        case platform::Key::E: return ImGuiKey_E;
        case platform::Key::F: return ImGuiKey_F;
        case platform::Key::G: return ImGuiKey_G;
        case platform::Key::H: return ImGuiKey_H;
        case platform::Key::I: return ImGuiKey_I;
        case platform::Key::J: return ImGuiKey_J;
        case platform::Key::K: return ImGuiKey_K;
        case platform::Key::L: return ImGuiKey_L;
        case platform::Key::M: return ImGuiKey_M;
        case platform::Key::N: return ImGuiKey_N;
        case platform::Key::O: return ImGuiKey_O;
        case platform::Key::P: return ImGuiKey_P;
        case platform::Key::Q: return ImGuiKey_Q;
        case platform::Key::R: return ImGuiKey_R;
        case platform::Key::S: return ImGuiKey_S;
        case platform::Key::T: return ImGuiKey_T;
        case platform::Key::U: return ImGuiKey_U;
        case platform::Key::V: return ImGuiKey_V;
        case platform::Key::W: return ImGuiKey_W;
        case platform::Key::X: return ImGuiKey_X;
        case platform::Key::Y: return ImGuiKey_Y;
        case platform::Key::Z: return ImGuiKey_Z;
        case platform::Key::Num0: return ImGuiKey_0;
        case platform::Key::Num1: return ImGuiKey_1;
        case platform::Key::Num2: return ImGuiKey_2;
        case platform::Key::Num3: return ImGuiKey_3;
        case platform::Key::Num4: return ImGuiKey_4;
        case platform::Key::Num5: return ImGuiKey_5;
        case platform::Key::Num6: return ImGuiKey_6;
        case platform::Key::Num7: return ImGuiKey_7;
        case platform::Key::Num8: return ImGuiKey_8;
        case platform::Key::Num9: return ImGuiKey_9;
        case platform::Key::Minus: return ImGuiKey_Minus;
        case platform::Key::Equals: return ImGuiKey_Equal;
        case platform::Key::LeftBracket: return ImGuiKey_LeftBracket;
        case platform::Key::RightBracket: return ImGuiKey_RightBracket;
        case platform::Key::Backslash: return ImGuiKey_Backslash;
        case platform::Key::Semicolon: return ImGuiKey_Semicolon;
        case platform::Key::Apostrophe: return ImGuiKey_Apostrophe;
        case platform::Key::Comma: return ImGuiKey_Comma;
        case platform::Key::Slash: return ImGuiKey_Slash;
        case platform::Key::Grave: return ImGuiKey_GraveAccent;
        case platform::Key::LeftControl: return ImGuiKey_LeftCtrl;
        case platform::Key::RightControl: return ImGuiKey_RightCtrl;
        case platform::Key::LeftAlt: return ImGuiKey_LeftAlt;
        case platform::Key::RightAlt: return ImGuiKey_RightAlt;
        case platform::Key::LeftSuper: return ImGuiKey_LeftSuper;
        case platform::Key::RightSuper: return ImGuiKey_RightSuper;
        case platform::Key::Menu: return ImGuiKey_Menu;
        case platform::Key::Home: return ImGuiKey_Home;
        case platform::Key::End: return ImGuiKey_End;
        case platform::Key::PageUp: return ImGuiKey_PageUp;
        case platform::Key::PageDown: return ImGuiKey_PageDown;
        case platform::Key::Insert: return ImGuiKey_Insert;
        case platform::Key::Delete: return ImGuiKey_Delete;
        case platform::Key::CapsLock: return ImGuiKey_CapsLock;
        case platform::Key::NumLock: return ImGuiKey_NumLock;
        case platform::Key::ScrollLock: return ImGuiKey_ScrollLock;
        case platform::Key::Left: return ImGuiKey_LeftArrow;
        case platform::Key::Right: return ImGuiKey_RightArrow;
        case platform::Key::Up: return ImGuiKey_UpArrow;
        case platform::Key::Down: return ImGuiKey_DownArrow;
        case platform::Key::LeftShift: return ImGuiKey_LeftShift;
        case platform::Key::RightShift: return ImGuiKey_RightShift;
        case platform::Key::F1: return ImGuiKey_F1;
        case platform::Key::F2: return ImGuiKey_F2;
        case platform::Key::F3: return ImGuiKey_F3;
        case platform::Key::F4: return ImGuiKey_F4;
        case platform::Key::F5: return ImGuiKey_F5;
        case platform::Key::F6: return ImGuiKey_F6;
        case platform::Key::F7: return ImGuiKey_F7;
        case platform::Key::F8: return ImGuiKey_F8;
        case platform::Key::F9: return ImGuiKey_F9;
        case platform::Key::F10: return ImGuiKey_F10;
        case platform::Key::F11: return ImGuiKey_F11;
        case platform::Key::F12: return ImGuiKey_F12;
        case platform::Key::Enter: return ImGuiKey_Enter;
        case platform::Key::Space: return ImGuiKey_Space;
        case platform::Key::Tab: return ImGuiKey_Tab;
        case platform::Key::Period: return ImGuiKey_Period;
        case platform::Key::Backspace: return ImGuiKey_Backspace;
        case platform::Key::Escape: return ImGuiKey_Escape;
        case platform::Key::Unknown:
        default: return ImGuiKey_None;
    }
}

int MapMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return ImGuiMouseButton_Left;
        case platform::MouseButton::Right: return ImGuiMouseButton_Right;
        case platform::MouseButton::Middle: return ImGuiMouseButton_Middle;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        default: return -1;
    }
}

void PushModifiers(ImGuiIO& io, const platform::Modifiers& mods) {
    io.AddKeyEvent(ImGuiMod_Shift, mods.shift);
    io.AddKeyEvent(ImGuiMod_Ctrl, mods.ctrl);
    io.AddKeyEvent(ImGuiMod_Alt, mods.alt);
    io.AddKeyEvent(ImGuiMod_Super, mods.super);
}

} // namespace karma::ui::imgui

#endif // defined(KARMA_HAS_IMGUI)

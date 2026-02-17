#include "ui/backends/rmlui/internal.hpp"

#if defined(KARMA_HAS_RMLUI)

namespace karma::ui::rmlui {

int MapModifiers(const platform::Modifiers& mods) {
    int flags = 0;
    if (mods.ctrl) {
        flags |= Rml::Input::KM_CTRL;
    }
    if (mods.shift) {
        flags |= Rml::Input::KM_SHIFT;
    }
    if (mods.alt) {
        flags |= Rml::Input::KM_ALT;
    }
    if (mods.super) {
        flags |= Rml::Input::KM_META;
    }
    return flags;
}

Rml::Input::KeyIdentifier MapKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return Rml::Input::KI_A;
        case platform::Key::B: return Rml::Input::KI_B;
        case platform::Key::C: return Rml::Input::KI_C;
        case platform::Key::D: return Rml::Input::KI_D;
        case platform::Key::E: return Rml::Input::KI_E;
        case platform::Key::F: return Rml::Input::KI_F;
        case platform::Key::G: return Rml::Input::KI_G;
        case platform::Key::H: return Rml::Input::KI_H;
        case platform::Key::I: return Rml::Input::KI_I;
        case platform::Key::J: return Rml::Input::KI_J;
        case platform::Key::K: return Rml::Input::KI_K;
        case platform::Key::L: return Rml::Input::KI_L;
        case platform::Key::M: return Rml::Input::KI_M;
        case platform::Key::N: return Rml::Input::KI_N;
        case platform::Key::O: return Rml::Input::KI_O;
        case platform::Key::P: return Rml::Input::KI_P;
        case platform::Key::Q: return Rml::Input::KI_Q;
        case platform::Key::R: return Rml::Input::KI_R;
        case platform::Key::S: return Rml::Input::KI_S;
        case platform::Key::T: return Rml::Input::KI_T;
        case platform::Key::U: return Rml::Input::KI_U;
        case platform::Key::V: return Rml::Input::KI_V;
        case platform::Key::W: return Rml::Input::KI_W;
        case platform::Key::X: return Rml::Input::KI_X;
        case platform::Key::Y: return Rml::Input::KI_Y;
        case platform::Key::Z: return Rml::Input::KI_Z;
        case platform::Key::Num0: return Rml::Input::KI_0;
        case platform::Key::Num1: return Rml::Input::KI_1;
        case platform::Key::Num2: return Rml::Input::KI_2;
        case platform::Key::Num3: return Rml::Input::KI_3;
        case platform::Key::Num4: return Rml::Input::KI_4;
        case platform::Key::Num5: return Rml::Input::KI_5;
        case platform::Key::Num6: return Rml::Input::KI_6;
        case platform::Key::Num7: return Rml::Input::KI_7;
        case platform::Key::Num8: return Rml::Input::KI_8;
        case platform::Key::Num9: return Rml::Input::KI_9;
        case platform::Key::Minus: return Rml::Input::KI_OEM_MINUS;
        case platform::Key::Equals: return Rml::Input::KI_OEM_PLUS;
        case platform::Key::LeftBracket: return Rml::Input::KI_OEM_4;
        case platform::Key::RightBracket: return Rml::Input::KI_OEM_6;
        case platform::Key::Backslash: return Rml::Input::KI_OEM_5;
        case platform::Key::Semicolon: return Rml::Input::KI_OEM_1;
        case platform::Key::Apostrophe: return Rml::Input::KI_OEM_7;
        case platform::Key::Comma: return Rml::Input::KI_OEM_COMMA;
        case platform::Key::Slash: return Rml::Input::KI_OEM_2;
        case platform::Key::Grave: return Rml::Input::KI_OEM_3;
        case platform::Key::LeftControl: return Rml::Input::KI_LCONTROL;
        case platform::Key::RightControl: return Rml::Input::KI_RCONTROL;
        case platform::Key::LeftAlt: return Rml::Input::KI_LMENU;
        case platform::Key::RightAlt: return Rml::Input::KI_RMENU;
        case platform::Key::LeftSuper: return Rml::Input::KI_LMETA;
        case platform::Key::RightSuper: return Rml::Input::KI_RMETA;
        case platform::Key::Menu: return Rml::Input::KI_APPS;
        case platform::Key::Home: return Rml::Input::KI_HOME;
        case platform::Key::End: return Rml::Input::KI_END;
        case platform::Key::PageUp: return Rml::Input::KI_PRIOR;
        case platform::Key::PageDown: return Rml::Input::KI_NEXT;
        case platform::Key::Insert: return Rml::Input::KI_INSERT;
        case platform::Key::Delete: return Rml::Input::KI_DELETE;
        case platform::Key::CapsLock: return Rml::Input::KI_CAPITAL;
        case platform::Key::NumLock: return Rml::Input::KI_NUMLOCK;
        case platform::Key::ScrollLock: return Rml::Input::KI_SCROLL;
        case platform::Key::Left: return Rml::Input::KI_LEFT;
        case platform::Key::Right: return Rml::Input::KI_RIGHT;
        case platform::Key::Up: return Rml::Input::KI_UP;
        case platform::Key::Down: return Rml::Input::KI_DOWN;
        case platform::Key::LeftShift: return Rml::Input::KI_LSHIFT;
        case platform::Key::RightShift: return Rml::Input::KI_RSHIFT;
        case platform::Key::F1: return Rml::Input::KI_F1;
        case platform::Key::F2: return Rml::Input::KI_F2;
        case platform::Key::F3: return Rml::Input::KI_F3;
        case platform::Key::F4: return Rml::Input::KI_F4;
        case platform::Key::F5: return Rml::Input::KI_F5;
        case platform::Key::F6: return Rml::Input::KI_F6;
        case platform::Key::F7: return Rml::Input::KI_F7;
        case platform::Key::F8: return Rml::Input::KI_F8;
        case platform::Key::F9: return Rml::Input::KI_F9;
        case platform::Key::F10: return Rml::Input::KI_F10;
        case platform::Key::F11: return Rml::Input::KI_F11;
        case platform::Key::F12: return Rml::Input::KI_F12;
        case platform::Key::Enter: return Rml::Input::KI_RETURN;
        case platform::Key::Space: return Rml::Input::KI_SPACE;
        case platform::Key::Tab: return Rml::Input::KI_TAB;
        case platform::Key::Period: return Rml::Input::KI_OEM_PERIOD;
        case platform::Key::Backspace: return Rml::Input::KI_BACK;
        case platform::Key::Escape: return Rml::Input::KI_ESCAPE;
        case platform::Key::Unknown:
        default: return Rml::Input::KI_UNKNOWN;
    }
}

int MapMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)

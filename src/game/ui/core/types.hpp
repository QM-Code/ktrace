#pragma once

#include <string>

#include "karma_extras/ui/types.hpp"

struct ScoreboardEntry {
    std::string name;
    int score;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};

namespace ui {

enum class QuickMenuAction {
    OpenConsole,
    Resume,
    Disconnect,
    Quit
};

} // namespace ui

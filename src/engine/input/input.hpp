#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "platform/events.hpp"
#include "input/mapping/mapper.hpp"

namespace platform {
class Window;
}

class Input {
public:
    Input(platform::Window &window, input::InputMap::DefaultBindingsMap defaultBindings);
    ~Input() = default;

    void pumpEvents(const std::vector<platform::Event> &events);
    const std::vector<platform::Event>& events() const { return lastEvents_; }
    bool actionTriggered(std::string_view actionId) const;
    bool actionDown(std::string_view actionId) const;
    void reloadKeyBindings();
    std::string bindingListDisplay(std::string_view actionId) const;

private:
    void loadKeyBindings();
    void update(const std::vector<platform::Event> &events);

    input::InputMapper mapper_;
    input::InputMap::DefaultBindingsMap defaultBindings_{};
    platform::Window *window = nullptr;
    std::vector<platform::Event> lastEvents_;
};

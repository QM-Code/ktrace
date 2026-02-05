#pragma once

#include <cstdint>

namespace karma::ecs {

struct Entity {
    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;

    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    constexpr bool isValid() const {
        return index != kInvalidIndex;
    }

    friend constexpr bool operator==(const Entity& a, const Entity& b) {
        return a.index == b.index && a.generation == b.generation;
    }

    friend constexpr bool operator!=(const Entity& a, const Entity& b) {
        return !(a == b);
    }
};

} // namespace karma::ecs

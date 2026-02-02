#pragma once

namespace game::renderer {

struct RadarRenderable {
    bool enabled = true;
};

struct RadarCircle {
    float radius = 1.0f;
    bool enabled = true;
};

} // namespace game::renderer

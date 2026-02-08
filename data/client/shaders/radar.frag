uniform float playerY;
uniform float jumpHeight;   
varying vec3 vWorldPos;
const float MAX_DY = 10.0;
const float BELOW_SOLID_DY = 10.0;
const float BLUE_SOLID_DY = 1.0;

void main() {
    float dy = vWorldPos.y - playerY;

    // Exactly at player height should still be visible on radar (player/shot blips).
    // Use a small epsilon to avoid precision artifacts.
    if (abs(dy) < 1e-4) {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 0.85);
        return;
    }

    float alpha = 0.0;
    vec3 color = vec3(0.0);

    if (dy > 0.0) {
        // 0..BLUE_SOLID_DY: fade-in blue (alpha 0->1)
        if (dy < BLUE_SOLID_DY) {
            float t = clamp(dy / BLUE_SOLID_DY, 0.0, 1.0);
            color = vec3(0.0, 0.0, 1.0);
            alpha = t;
        } else {
            // >= BLUE_SOLID_DY: solid (no transparency). At 5 units up: fully blue.
            // Higher than that: shift towards red.
            float t = clamp((dy - BLUE_SOLID_DY) / max(1e-3, (MAX_DY - BLUE_SOLID_DY)), 0.0, 1.0);
            color = mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), t);
            alpha = 1.0;
        }
    } else {
        // 0..-BELOW_SOLID_DY: fade-in red (alpha 0->1), then stay fully opaque red.
        float t = clamp((-dy) / BELOW_SOLID_DY, 0.0, 1.0);
        color = vec3(1.0, 0.0, 0.0);
        alpha = t;
    }

    gl_FragColor = vec4(color, alpha);
}

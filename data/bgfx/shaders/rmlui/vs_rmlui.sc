$input a_position, a_color0, a_texcoord0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform mat4 u_transform;
uniform vec4 u_translate;

void main() {
    vec2 translated = a_position + u_translate.xy;
    gl_Position = mul(u_transform, vec4(translated, 0.0, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}

$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_scaleBias;

void main() {
    gl_Position = vec4(a_position.xy * u_scaleBias.xy + u_scaleBias.zw, 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
}

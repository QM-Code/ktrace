$input a_position
$output v_dir

#include <bgfx_shader.sh>

void main() {
    v_dir = a_position;
    gl_Position = vec4(a_position, 1.0);
}

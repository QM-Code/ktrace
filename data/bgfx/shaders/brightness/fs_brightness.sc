$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);
uniform vec4 u_brightness;

void main() {
    vec4 texColor = texture2D(s_tex, v_texcoord0);
    gl_FragColor = vec4(texColor.rgb * u_brightness.x, 1.0);
}

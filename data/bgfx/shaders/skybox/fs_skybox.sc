$input v_dir

#include <bgfx_shader.sh>

SAMPLERCUBE(s_skybox, 0);

void main() {
    gl_FragColor = textureCube(s_skybox, normalize(v_dir));
}

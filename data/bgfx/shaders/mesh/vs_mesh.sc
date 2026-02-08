$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_normal

#include <bgfx_shader.sh>

void main() {
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_texcoord0 = a_texcoord0;
#if BGFX_SHADER_LANGUAGE_GLSL
    v_normal = mul(mat3(u_modelView), a_normal);
#else
    v_normal = mul((float3x3)u_modelView, a_normal);
#endif
}

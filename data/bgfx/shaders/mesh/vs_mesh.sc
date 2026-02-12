$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_normal, v_worldPos

#include <bgfx_shader.sh>

void main() {
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_texcoord0 = a_texcoord0;
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    v_worldPos = worldPos.xyz;
#if BGFX_SHADER_LANGUAGE_GLSL
    v_normal = mul(mat3(u_model[0]), a_normal);
#else
    v_normal = mul((float3x3)u_model[0], a_normal);
#endif
}

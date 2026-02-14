$input a_position
$output v_depth

#include <bgfx_shader.sh>

uniform vec4 u_shadowParams1; // extent, center_x, center_y, min_depth
uniform vec4 u_shadowParams2; // inv_depth_range, ..., clip_depth_scale, clip_depth_bias
uniform vec4 u_shadowAxisRight;
uniform vec4 u_shadowAxisUp;
uniform vec4 u_shadowAxisForward;

void main() {
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    float extent = max(u_shadowParams1.x, 0.001);
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float lx = dot(worldPos.xyz, u_shadowAxisRight.xyz);
    float ly = dot(worldPos.xyz, u_shadowAxisUp.xyz);
    float lz = dot(worldPos.xyz, u_shadowAxisForward.xyz);
    float depth = clamp((lz - u_shadowParams1.w) * invDepthRange, 0.0, 1.0);

    float ndcX = (lx - u_shadowParams1.y) / extent;
    float ndcY = ((ly - u_shadowParams1.z) / extent) * u_shadowAxisUp.w;
    gl_Position = vec4(ndcX, ndcY, (depth * u_shadowParams2.z) + u_shadowParams2.w, 1.0);
    v_depth = depth;
}

$input a_position, a_normal
#include <bgfx_shader.sh>

uniform vec4 u_shadowParams0; // enabled, strength, base_bias, inv_map_size
uniform vec4 u_shadowParams1; // extent, center_x, center_y, min_depth
uniform vec4 u_shadowParams2; // inv_depth_range, ..., clip_depth_scale, clip_depth_bias
uniform vec4 u_shadowBiasParams; // receiver_scale, normal_scale, raster_depth_bias, raster_slope_bias
uniform vec4 u_shadowAxisRight;
uniform vec4 u_shadowAxisUp;
uniform vec4 u_shadowAxisForward;

void main() {
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
#if BGFX_SHADER_LANGUAGE_GLSL
    vec3 worldNormal = normalize(mul(mat3(u_model[0]), a_normal));
#else
    vec3 worldNormal = normalize(mul((float3x3)u_model[0], a_normal));
#endif
    float extent = max(u_shadowParams1.x, 0.001);
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float invMapSize = max(u_shadowParams0.w, 0.0);
    float lx = dot(worldPos.xyz, u_shadowAxisRight.xyz);
    float ly = dot(worldPos.xyz, u_shadowAxisUp.xyz);
    float lz = dot(worldPos.xyz, u_shadowAxisForward.xyz);
    float depth = clamp((lz - u_shadowParams1.w) * invDepthRange, 0.0, 1.0);
    float slope = clamp(1.0 - abs(dot(worldNormal, -u_shadowAxisForward.xyz)), 0.0, 1.0);
    depth = clamp(
        depth + u_shadowBiasParams.z + (invMapSize * u_shadowBiasParams.w * slope),
        0.0,
        1.0);

    float ndcX = (lx - u_shadowParams1.y) / extent;
    float ndcY = ((ly - u_shadowParams1.z) / extent) * u_shadowAxisUp.w;
    gl_Position = vec4(ndcX, ndcY, (depth * u_shadowParams2.z) + u_shadowParams2.w, 1.0);
}

$input v_texcoord0, v_normal, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_color;
uniform vec4 u_lightDir;
uniform vec4 u_lightColor;
uniform vec4 u_ambientColor;
uniform vec4 u_unlit;
uniform vec4 u_textureMode;
uniform vec4 u_shadowParams0;
uniform vec4 u_shadowParams1;
uniform vec4 u_shadowParams2;
uniform vec4 u_shadowAxisRight;
uniform vec4 u_shadowAxisUp;
uniform vec4 u_shadowAxisForward;
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_occlusion, 2);
SAMPLER2D(s_shadow, 3);

float sampleShadowVisibility(vec3 worldPos, float ndotl) {
    if (u_shadowParams0.x < 0.5) {
        return 1.0;
    }

    float extent = max(u_shadowParams1.x, 0.001);
    float centerX = u_shadowParams1.y;
    float centerY = u_shadowParams1.z;
    float minDepth = u_shadowParams1.w;
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float radius = clamp(u_shadowParams2.y, 0.0, 4.0);
    float invMapSize = max(u_shadowParams0.w, 0.0);
    // Add slope-aware bias to reduce self-shadow checkerboarding on grazing angles.
    float slope = clamp(1.0 - ndotl, 0.0, 1.0);
    float bias = clamp(
        u_shadowParams0.z + (u_shadowParams0.w * (0.08 + (0.35 * slope))),
        0.0,
        0.02);

    float lx = dot(worldPos, u_shadowAxisRight.xyz);
    float ly = dot(worldPos, u_shadowAxisUp.xyz);
    float lz = dot(worldPos, u_shadowAxisForward.xyz);
    float u = 0.5 + ((lx - centerX) / (2.0 * extent));
    float v = 0.5 + ((ly - centerY) / (2.0 * extent));
    float depth = (lz - minDepth) * invDepthRange;

    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 || depth < 0.0 || depth > 1.0) {
        return 1.0;
    }

    float lit = 0.0;
    float count = 0.0;
    for (int oy = -4; oy <= 4; ++oy) {
        for (int ox = -4; ox <= 4; ++ox) {
            if (abs(float(ox)) > radius || abs(float(oy)) > radius) {
                continue;
            }
            vec2 uv = vec2(u, v) + vec2(float(ox), float(oy)) * invMapSize;
            float mapDepth = texture2D(s_shadow, uv).r;
            float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
            lit += step(depth - bias, mapDepth) * w;
            count += w;
        }
    }
    if (count < 0.5) {
        return 1.0;
    }
    return lit / count;
}

void main() {
    vec4 texColor = texture2D(s_tex, v_texcoord0);
    float normalModulation = 1.0;
    if (u_textureMode.x > 0.5) {
        vec2 encodedNormal = texture2D(s_normal, v_texcoord0).rg * 2.0 - vec2(1.0, 1.0);
        float normalResponse = clamp(length(encodedNormal), 0.0, 1.0);
        normalModulation = clamp(1.0 + (0.16 * max(0.0, normalResponse - 0.18)), 1.0, 1.18);
    }

    float occlusionModulation = 1.0;
    if (u_textureMode.y > 0.5) {
        float ao = clamp(texture2D(s_occlusion, v_texcoord0).r, 0.0, 1.0);
        occlusionModulation = clamp(0.25 + (0.75 * ao), 0.25, 1.0);
    }

    float combinedModulation = clamp(normalModulation * occlusionModulation, 0.20, 1.20);
    vec4 shadedTexColor = vec4(clamp(texColor.rgb * combinedModulation, vec3(0.0), vec3(1.0)),
                               clamp(texColor.a, 0.0, 1.0));
    vec4 baseColor = u_color * shadedTexColor;
    if (u_unlit.x > 0.5) {
        gl_FragColor = baseColor;
        return;
    }
    vec3 n = normalize(v_normal);
    float ndotl = max(dot(n, normalize(u_lightDir.xyz)), 0.0);
    float shadowVis = sampleShadowVisibility(v_worldPos, ndotl);
    float shadowFactor = clamp(1.0 - (u_shadowParams0.y * (1.0 - shadowVis)), 0.0, 1.0);
    vec3 lit = baseColor.rgb * (u_ambientColor.rgb + (u_lightColor.rgb * ndotl * shadowFactor));
    gl_FragColor = vec4(lit, baseColor.a);
}

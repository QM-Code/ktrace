$input v_texcoord0, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_color;
uniform vec4 u_lightDir;
uniform vec4 u_lightColor;
uniform vec4 u_ambientColor;
uniform vec4 u_unlit;
uniform vec4 u_textureMode;
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_occlusion, 2);

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
    vec3 lit = baseColor.rgb * (u_ambientColor.rgb + u_lightColor.rgb * ndotl);
    gl_FragColor = vec4(lit, baseColor.a);
}

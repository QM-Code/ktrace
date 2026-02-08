$input v_texcoord0, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_color;
uniform vec4 u_lightDir;
uniform vec4 u_lightColor;
uniform vec4 u_ambientColor;
uniform vec4 u_unlit;
SAMPLER2D(s_tex, 0);

void main() {
    vec4 texColor = texture2D(s_tex, v_texcoord0);
    vec4 baseColor = u_color * texColor;
    if (u_unlit.x > 0.5) {
        gl_FragColor = baseColor;
        return;
    }
    vec3 n = normalize(v_normal);
    float ndotl = max(dot(n, normalize(u_lightDir.xyz)), 0.0);
    vec3 lit = baseColor.rgb * (u_ambientColor.rgb + u_lightColor.rgb * ndotl);
    gl_FragColor = vec4(lit, baseColor.a);
}

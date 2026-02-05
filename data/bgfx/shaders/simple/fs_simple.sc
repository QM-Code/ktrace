$input v_normal

#include <bgfx_shader.sh>

uniform vec4 u_color;

void main()
{
    vec3 n = normalize(v_normal);
    float light = max(dot(n, normalize(vec3(0.3, 0.8, 0.5))), 0.2);
    gl_FragColor = vec4(u_color.rgb * light, u_color.a);
}

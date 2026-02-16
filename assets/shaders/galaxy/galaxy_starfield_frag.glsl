#version 450

in vec3 v_color;

out vec4 f_color;

void main()
{
    // Soft circular point
    vec2 center = (gl_PointCoord - 0.5) * 2.0;
    float dist = length(center);
    float alpha = smoothstep(1.0, 0.0, dist) * 0.8;

    if (alpha < 0.001) discard;

    f_color = vec4(v_color, alpha);
}

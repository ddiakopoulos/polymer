#version 450

in float v_density;

uniform vec4 u_dense_color;  // rgb, w unused
uniform vec4 u_sparse_color; // rgb, w unused
uniform float u_brightness;

out vec4 f_color;

void main()
{
    // Smooth circular star shape from gl_PointCoord
    vec2 center = (gl_PointCoord - 0.5) * 2.0;
    float dist = length(center);
    float alpha = smoothstep(1.0, 0.0, dist) * smoothstep(1.0, 0.3, dist);

    if (alpha < 0.001) discard;

    // Color based on density: dense_color for arm centers, sparse_color for edges
    vec3 star_color = mix(u_dense_color.rgb, u_sparse_color.rgb, v_density) * u_brightness;

    f_color = vec4(star_color, alpha);
}

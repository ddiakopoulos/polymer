#version 450

uniform sampler2D s_source;
uniform vec2 u_direction;  // (1/width, 0) for horizontal, (0, 1/height) for vertical
uniform float u_radius;

in vec2 v_texcoord;
out vec4 f_color;

void main()
{
    // 13-tap Gaussian blur with configurable radius
    // Weights for sigma ~= 4.0
    const float weights[7] = float[7](
        0.1964825501511404,
        0.2969069646728344,
        0.2969069646728344 * 0.7,
        0.2969069646728344 * 0.4,
        0.2969069646728344 * 0.2,
        0.2969069646728344 * 0.08,
        0.2969069646728344 * 0.02
    );

    vec3 result = texture(s_source, v_texcoord).rgb * weights[0];

    for (int i = 1; i < 7; ++i)
    {
        vec2 offset = u_direction * float(i) * u_radius;
        result += texture(s_source, v_texcoord + offset).rgb * weights[i];
        result += texture(s_source, v_texcoord - offset).rgb * weights[i];
    }

    // Normalize (sum of weights)
    float total_weight = weights[0];
    for (int i = 1; i < 7; ++i)
    {
        total_weight += weights[i] * 2.0;
    }

    f_color = vec4(result / total_weight, 1.0);
}

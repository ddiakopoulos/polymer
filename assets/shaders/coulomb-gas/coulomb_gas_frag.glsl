#version 450

layout(location = 0) out vec4 frag_color;

flat in float v_particle_type;

uniform float u_alpha;
uniform float u_is_dark;
uniform int u_two_species;

void main()
{
    if (u_two_species != 0)
    {
        vec3 pos_dark  = vec3(1.0, 0.4, 0.3);
        vec3 neg_dark  = vec3(0.3, 0.6, 1.0);
        vec3 pos_light = vec3(0.85, 0.2, 0.15);
        vec3 neg_light = vec3(0.1, 0.3, 0.8);

        bool is_positive = v_particle_type > 0.0;
        vec3 col = (u_is_dark > 0.5)
            ? (is_positive ? pos_dark : neg_dark)
            : (is_positive ? pos_light : neg_light);
        frag_color = vec4(col, u_alpha);
    }
    else
    {
        float c = u_is_dark > 0.5 ? 1.0 : 0.0;
        frag_color = vec4(c, c, c, u_alpha);
    }
}

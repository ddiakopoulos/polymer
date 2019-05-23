#version 450

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

out vec4 f_color;

uniform float u_alpha;
uniform vec4 u_fade_ctrl = vec4(0.0, 0.0, 0.80, 1.0); // far, far, near, near
uniform vec3 u_color;

void main()
{   
    float near = clamp((v_texcoord.y - u_fade_ctrl[0]) / (u_fade_ctrl[1] - u_fade_ctrl[0]), 0.0, 1.0);
    float far = clamp(1.0 - ((v_texcoord.y - u_fade_ctrl[2]) / (u_fade_ctrl[3] - u_fade_ctrl[2])), 0.0, 1.0);
    float fade_factor = near * far;
    f_color = vec4(u_color, u_alpha * fade_factor); 
}

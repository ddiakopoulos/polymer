#include "renderer_common.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

uniform float u_opacity = 1.0f;

out vec4 f_color;

float integrate_montecarlo(vec2 start, vec2 end, vec2 pt) 
{
    if (length(pt - (start*0.5 + end*0.5)) > 0.5) 
    {
        return 0.0;   
    }

    float sum = 0.f;
    for(int i  = 0; i <= 4; i++) 
    {
        float alpha = float(i) / 4.0;
        float d = length((alpha * start + (1.0 - alpha) * end) - pt);
        sum += 1.0 / (pow(d, 1.44));
    }

    return 0.0025 * sum;
}    

// compute the light contribution from each fluorescent segment
float compute_light_from_square(vec2 outerMin, vec2 outerMax, vec2 pos) 
{
    float d = 0.0;
    d += integrate_montecarlo(outerMin, vec2(outerMin.x, outerMax.y), pos);
    d += integrate_montecarlo(vec2(outerMax.x, outerMin.y), outerMin, pos);
    d += integrate_montecarlo(vec2(outerMin.x, outerMax.y), outerMax, pos);
    d += integrate_montecarlo(vec2(outerMax.x, outerMin.y), outerMax, pos);
    return d;
}    

float the_random(vec2 seed2) 
{
    vec4 seed4 = vec4(seed2.x, seed2.y, seed2.y, 1.0);
    float dot_product = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
    return fract(sin(dot_product) * 43758.5453);
}

void main()
{
    vec2 y_pos = mod(v_world_position.yy * 2, vec2(1));
    vec2 z_pos = mod(v_world_position.zz * 2, vec2(1));
    vec2 x_pos = mod(v_world_position.xx * 2, vec2(1));

    float rnd = the_random(vec2(x_pos.x, z_pos.x) * 0.5);
    //x_pos *= rnd;

    float box_padding_x = 0.15;
    float box_padding_y = 0.25;
    float box_padding = 0.1;

    int num_boxes = 4;

    float box_width = (1.0-2.0*box_padding_x-0.5*float(num_boxes-1) * box_padding) / float(num_boxes);
    float box_height = (1.0-2.0*box_padding_x-0.5*float(num_boxes-1) * box_padding) / float(num_boxes);

    float d = 0.0;
    for(int i = 0; i < num_boxes; i++) 
    {
        for(int j = 0; j < num_boxes; j++) 
        {
            vec2 outerMin = vec2(float(i) * box_width + box_padding_x, float(j) * box_height + box_padding_y);
            vec2 outerMax = outerMin + vec2(box_width, box_height);
            outerMin += vec2(box_padding, box_padding);
            outerMax -= vec2(box_padding, box_padding);
            d += compute_light_from_square(outerMin, outerMax, y_pos);
            d += compute_light_from_square(outerMin, outerMax, z_pos);
            d += compute_light_from_square(outerMin, outerMax, x_pos);
        }
    }

    vec4 mixColor =  vec4(19.0/255.0, 98.0/255.0, 128.0/255.0, 1.0);
    f_color = mix(0.00028 * vec4(pow(d, 1.77 + 0.43)), mixColor, 0.40);
    f_color.a *= (u_opacity);
}
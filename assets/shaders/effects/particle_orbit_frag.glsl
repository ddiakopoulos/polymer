#version 330

const float num_particles = 64.0;
const float particle_res = 32.0;

const float pixSize = 1.0 / particle_res;
const float pixSizeInv = 1.0 - pixSize;
const float pixSizeHalf = pixSize * 0.5;

uniform float u_time;
uniform vec4 u_emissive_color = vec4(1, 1, 1, 1);

in vec3 v_position, v_normal;
in vec2 v_texcoord;

out vec4 f_color;

float sdf(vec2 p, vec2 uv) { return length(uv-p); }
float circle(vec2 p, vec2 uv, float r) { return step(sdf(p,uv), r); }

float draw_circle(vec2 p, vec2 uv, float i)
{     
    float modifier = (sin(i * 5.0) * .5 + .5) / particle_res / 2.0; 
    vec4 r1;
    r1 += circle(p, uv, pixSize - modifier);
    return r1.x;
}

float draw_square(vec2 p, vec2 uv, float i)
{
    float modifier = (sin(i * 5.0) * .5 + .5) / particle_res / 2.0; 
    vec4 r1 = vec4(step(p, uv), 1.0 - step(p + pixSize - modifier, uv));
    return (r1.x * r1.y * r1.z * r1.w);
}

void main()
{
    vec2 uv = (v_texcoord.xy + vec2(-0.5, -0.5)) * 2;  
    float sphere_idx = 1.0 + pixSizeHalf - length(uv);
    
    float particle_color = 0.0;
    for (float i = num_particles; i > 0.0; i -= 1.0)
    {
        float dist = i / num_particles;
        
        // Test for particles closer to center
        if (sphere_idx > (pixSizeInv - dist))
        {  
            vec2 angleMod = vec2(cos(i), sin(i));

            vec2 pPos = vec2(0.0, 0.0);
            pPos.x += cos(i + u_time / dist) * dist * angleMod.x;
            pPos.y += sin(i + u_time / dist) * dist * angleMod.y;

            float rot = radians(i);
            mat2 m = mat2(cos(rot), -sin(rot), sin(rot), cos(rot));
            pPos  = m * pPos;

            particle_color += draw_circle(pPos, uv, i);

        }
        else break;
    }
    f_color = vec4(vec3(particle_color), 1) * u_emissive_color * sphere_idx;
}

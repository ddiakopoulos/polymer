#version 330

const float num_particles = 64.0;
const float particle_res = 32.0;
const float pixSize = 1.0 / particle_res;

const float direction = 0.0;
float seed = 0.75;

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

void main()
{
    vec2 uv = (v_texcoord.xy + vec2(-0.5, -0.5)) * 2;  
    float sphere_idx = 1.0 - length(uv);

    float particle_color = 0.0;
    if(sphere_idx > 0.0)
    {
    
        for(float i = 0.0; i < num_particles; i += 1.0)
        {
            seed += i + tan(seed);
            vec2 tPos = vec2(cos(seed), sin(seed));
            vec2 pPos = vec2(0.0, 0.0);

            float speed = i / num_particles + 0.4713 * (cos(seed) + 1.5) / 1.5;
            float timeOffset = u_time * speed + speed;
            float timecycle = timeOffset-floor(timeOffset); 
        
            pPos = mix(tPos, pPos,1.0 + direction - timecycle);
        
            particle_color += draw_circle(pPos, uv, i) * speed;
        }
    }
    f_color = vec4(vec3(particle_color), 1) * u_emissive_color * sphere_idx;
}

#include "renderer_common.glsl"

// based on https://thebookofshaders.com/edit.php#10/ikeda-03.frag

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

out vec4 f_color;

uniform vec4 u_resolution = vec4(10, 100, 1, 1);
uniform vec2 u_grid = vec2(200, 200);
uniform float u_density = 0.5;
uniform float u_speed = 0.5;
uniform float u_margin = 0.0;
uniform float u_band_offset = 0.04;

float random(in float x) { return fract(sin(x) * 1e4); }
float random(in vec2 st) { return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123); }
float pattern(in vec2 st, in vec2 v, float t)
{
    vec2 p = floor(st + v);
    return step(t, random(u_grid.x + p * .000001) + random(p.x) * 0.5);
}

void main()
{
    float time = u_time * 0.01;

    vec2 coord = vec2(v_world_position.z, v_world_position.y) * 10;
    vec2 st = coord / u_resolution.xy;
    st.x *= coord.x / u_resolution.y;
    st *= u_grid;
    vec2 fpos = fract(st);

/*
    float timeCalc = u_speed * 2.0 * max(u_grid.x, u_grid.y);
    vec2 vel = vec2(timeCalc, timeCalc);
    vel *= vec2(1, 1) * random(v_world_position.y * 0.000001 * u_grid.y);
    vel *= (time / u_grid.y);

    vec2 offset = vec2(u_band_offset, 0);
    vec3 color = vec3(0);
    color.r = pattern(st + offset, vel, 0.5 + u_density / u_resolution.x);
    color.g = pattern(st, vel, 0.5 + u_density / u_resolution.x);
    color.b = pattern(st - offset, vel, 0.5 + u_density / u_resolution.x);
    color *= step(u_margin, fpos.y);

    float mask1 = dot(vec3(1, 0, 0), v_normal); // v_normal should be in world space
    float mask2 = dot(vec3(-1, 0, 0), v_normal);
*/

    vec2 ipos = floor(st);  // integer

    vec2 vel = vec2(time *2.0 * max(u_grid.x, u_grid.y)); // time
    vel *= vec2(1., -2.0) * random(1.0 + ipos.y); // direction

    // Assign a random value base on the integer coord
    vec2 offset = vec2(0.05, 0);

    vec3 color = vec3(0.);
    color.r = pattern(st+offset,vel,0.5 + u_density/u_resolution.x);
    color.g = pattern(st,vel,0.5 + u_density/u_resolution.x);
    color.b = pattern(st-offset,vel,0.5 + u_density/u_resolution.x);

    // Margins
    color *= step(0.5, fpos.y);

    f_color = vec4(vec3(color), 1);
    //f_color = vec4(v_world_position, 1);
    //f_color = vec4(max(vec3(0), mix(vec3(0, 0, 0), color, mask1)) + max(vec3(0), mix(vec3(0, 0, 0), color, mask2)), 1.0);
}
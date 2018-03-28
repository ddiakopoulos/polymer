#version 330

const float speed = 2; // spread effect speed 
const float effectRadius = 0.75; // maximum radius of effect

uniform float u_time;
uniform vec3 u_eye;
uniform float u_triangleScale;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

float r(float n) { return fract(abs(sin(n*55.753) * 367.34)); }
float r(vec2 n) { return r(dot(n, vec2(2.46, -1.21))); }

vec3 expanding_ring(in vec3 pos, in vec3 centerPosition)
{
    float border = mod(u_time * speed, 5.0);
    float d = (length(pos.xz - centerPosition.xz) + pos.y - centerPosition.y) / effectRadius;
    vec3 c = vec3(1.0, 1.0, 1.0) * smoothstep(border - 0.2, border, d); // rim
    c *= smoothstep(border, border - 0.05, d); // front cut
    c *= smoothstep(border - 3.0, border - 0.5, d); // cut back
    c *= smoothstep(5.0, 4.0, border); // fade 
    return c;
}

vec3 small_triangles_color(vec3 worldPosition)
{
    const float a = (radians(72.0));
    const float zoom = 0.125;

    vec2 c = (worldPosition.xy + vec2(0.0, worldPosition.z)) * vec2(sin(a), 1.0) / u_triangleScale;
    c = ((c + vec2(c.y, 0.0) * cos(a)) / zoom) + vec2(floor((c.x - c.y * cos(a)) / zoom * 4.0) / 4.0, 0.0);

    float type = (r(floor(c * 4.0)) * 0.2 + r(floor(c*2.0)) * 0.3 + r(floor(c)) * 0.5);
    type += 0.3 * sin(u_time * 5.0*type);

    float l = min(min((1.0 - (2.0 * abs(fract((c.x - c.y) * 4.0) - 0.5))),
                      (1.0 - (2.0 * abs(fract(c.y * 4.0) - 0.5)))),
                      (1.0 - (2.0 * abs(fract(c.x * 4.0) - 0.5))));

    l = smoothstep(0.06, 0.04, l);

    return vec3(mix(type, l, 0.3));
}

vec3 largest_triangles_color(vec3 worldPosition)
{
    const float a = (radians(72.0));
    const float zoom = 0.5;

    vec2 c = (worldPosition.xy + vec2(0.0, worldPosition.z)) * vec2(sin(a), 1.0) / u_triangleScale;
    c = ((c + vec2(c.y, 0.0) * cos(a)) / zoom) + vec2(floor((c.x - c.y*cos(a)) / zoom * 4.0) / 4.0, 0.0);

    float l = min(min((1.0 - (2.0 * abs(fract((c.x - c.y) * 4.0) - 0.5))),
                      (1.0 - (2.0 * abs(fract(c.y * 4.0) - 0.5)))),
                      (1.0 - (2.0 * abs(fract(c.x * 4.0) - 0.5))));

    l = smoothstep(0.03, 0.02, l);

    return vec3(mix(0.01, l, 1.0));
}

vec4 holo_scan_effect(in vec3 pos, in vec3 centerPosition)
{
    float border = mod(u_time * speed, 5.0);

    float d = length(pos.xz - centerPosition.xz) + pos.y - centerPosition.y;
    d /= effectRadius;

    vec3 c1 = largest_triangles_color(pos);
    vec3 c2 = small_triangles_color(pos);

    vec3 c = vec3(1, 1, 1) * smoothstep(border - 0.2, border, d); // rim
    c += c1;
    c += c2 * smoothstep(border - 0.5, border - 0.88, d); // Small triangle after front
    c *= smoothstep(border, border - 0.05, d); // front cut
    c *= smoothstep(border - 3.0, border - 0.5, d); // cut back
    c *= smoothstep(5.0, 4.0, border); // fade 

    return vec4(c, 1) * vec4(0, 0.5, 1, 1.0);
}

void main()
{
    vec4 light = holo_scan_effect(v_world_position, vec3(0, 0, 0));
    f_color = vec4(light);
}
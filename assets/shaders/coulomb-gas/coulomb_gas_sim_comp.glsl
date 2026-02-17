#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer PosIn  { vec2 pos_in[]; };
layout(std430, binding = 1) readonly buffer VelIn  { vec2 vel_in[]; };
layout(std430, binding = 2) writeonly buffer PosOut { vec2 pos_out[]; };
layout(std430, binding = 3) writeonly buffer VelOut { vec2 vel_out[]; };
layout(std430, binding = 4) readonly buffer InsertedParticles { vec4 inserted_particles[]; };

uniform int u_n;
uniform int u_pot;
uniform int u_inserted_count;
uniform float u_dt;
uniform float u_damping;
uniform float u_lemniscate_t;
uniform float u_lem_interpol;

shared vec2 tile_pos[256];

vec2 grad_logs(float x, float y)
{
    float eps = 1e-6;
    float gx = 0.0;
    float gy = 0.0;
    for (int i = 0; i < u_inserted_count; ++i)
    {
        vec4 p = inserted_particles[i];
        float dx = x - p.x;
        float dy = y - p.y;
        float d = dx * dx + dy * dy + eps;
        gx -= p.z * dx / d;
        gy -= p.z * dy / d;
    }
    return vec2(gx, gy);
}

vec2 grad_f2_interp(float x, float y, float p)
{
    float r2 = x * x + y * y + 1e-12;
    float r = sqrt(r2);
    float th = atan(y, x);
    float c = cos(p * th);
    float s = sin(p * th);
    float sc = p * pow(r, p - 2.0);
    return vec2(sc * (x * c + y * s), sc * (y * c - x * s));
}

vec2 grad_conf(float x, float y)
{
    float r2 = x * x + y * y;
    float r4 = r2 * r2;
    float gx = 0.0;
    float gy = 0.0;

    switch (u_pot)
    {
        case 0:
        {
            gx = 2.0 * x;
            gy = 2.0 * y;
            break;
        }
        case 1:
        {
            gx = 4.0 * r2 * x;
            gy = 4.0 * r2 * y;
            break;
        }
        case 2:
        {
            float s = 20.0 * pow(r2, 9.0);
            gx = s * x;
            gy = s * y;
            break;
        }
        case 3:
        {
            float c = u_lemniscate_t * 2.0 / sqrt(2.0);
            gx = 4.0 * r2 * x - 2.0 * c * x;
            gy = 4.0 * r2 * y + 2.0 * c * y;
            break;
        }
        case 4:
        {
            float c = u_lemniscate_t * 2.0 / sqrt(3.0);
            float x2 = x * x;
            float y2 = y * y;
            gx = 6.0 * r4 * x - 3.0 * c * (x2 - y2);
            gy = 6.0 * r4 * y + 6.0 * c * x * y;
            break;
        }
        case 5:
        {
            float c = u_lemniscate_t * 2.0 / sqrt(5.0);
            float s = 10.0 * pow(r2, 4.0);
            float gxr = s * x;
            float gyr = s * y;
            float x2 = x * x;
            float y2 = y * y;
            float x3 = x2 * x;
            float y3 = y2 * y;
            float x4 = x2 * x2;
            float y4 = y2 * y2;
            float dPx = 5.0 * x4 - 30.0 * x2 * y2 + 5.0 * y4;
            float dPy = -20.0 * x3 * y + 20.0 * x * y3;
            gx = gxr - c * dPx;
            gy = gyr - c * dPy;
            break;
        }
        default:
        {
            float p = u_lem_interpol;
            float s = 2.0 * p * pow(r2, p - 1.0);
            float gx1 = s * x;
            float gy1 = s * y;
            float alpha = u_lemniscate_t * 2.0 / sqrt(max(p, 1e-6));
            vec2 g2 = grad_f2_interp(x, y, p);
            gx = gx1 - alpha * g2.x;
            gy = gy1 - alpha * g2.y;
            break;
        }
    }

    vec2 g_ins = grad_logs(x, y);
    return vec2(gx + g_ins.x, gy + g_ins.y);
}

void main()
{
    uint i = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    uint N = uint(u_n);

    float px = 0.0;
    float py = 0.0;
    float vx = 0.0;
    float vy = 0.0;

    if (i < N)
    {
        px = pos_in[i].x;
        py = pos_in[i].y;
        vx = vel_in[i].x;
        vy = vel_in[i].y;
    }

    vec2 g = grad_conf(px, py);
    float accx = -float(N) * g.x;
    float accy = -float(N) * g.y;

    uint tile_start = 0u;
    while (tile_start < N)
    {
        uint j = tile_start + lid;
        if (j < N)
        {
            tile_pos[lid] = pos_in[j];
        }
        barrier();

        uint count = min(256u, N - tile_start);
        for (uint k = 0u; k < count; ++k)
        {
            float dx = tile_pos[k].x - px;
            float dy = tile_pos[k].y - py;
            float inv = 1.0 / (dx * dx + dy * dy + 1e-6);
            float scale = 2.0 * inv;
            if (tile_start + k != i)
            {
                accx -= dx * scale;
                accy -= dy * scale;
            }
        }
        barrier();

        tile_start += 256u;
    }

    if (i < N)
    {
        vx += accx * u_dt;
        vy += accy * u_dt;

        px += vx * u_dt;
        py += vy * u_dt;

        vx *= u_damping;
        vy *= u_damping;

        pos_out[i] = vec2(px, py);
        vel_out[i] = vec2(vx, vy);
    }
}

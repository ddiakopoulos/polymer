#pragma once

#ifndef renderable_grid_h
#define renderable_grid_h

#include "polymer-gfx-gl/gl-api.hpp"

constexpr const char gl_grid_vert[] = R"(#version 450

uniform vec4 u_near_origin;
uniform vec4 u_near_x;
uniform vec4 u_near_y;
uniform vec4 u_far_origin;
uniform vec4 u_far_x;
uniform vec4 u_far_y;

out vec3 world_far;
out vec3 world_near;

void main()
{
    // Full-screen triangle: 3 vertices cover the entire screen
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 vertex_position = positions[gl_VertexID];
    gl_Position = vec4(vertex_position, 0.0, 1.0);

    // Map from NDC [-1, 1] to [0, 1]
    vec2 p = vertex_position * 0.5 + 0.5;

    // Calculate world space positions on near and far planes
    world_near = u_near_origin.xyz + u_near_x.xyz * p.x + u_near_y.xyz * p.y;
    world_far = u_far_origin.xyz + u_far_x.xyz * p.x + u_far_y.xyz * p.y;
}
)";

constexpr const char gl_grid_frag[] = R"(#version 450

uniform vec4 u_view_position;
uniform mat4 u_view_projection;
uniform int u_plane;

in vec3 world_far;
in vec3 world_near;

out vec4 frag_color;

// Plane definitions
const vec4 planes[3] = vec4[3](
    vec4(1.0, 0.0, 0.0, 0.0),
    vec4(0.0, 1.0, 0.0, 0.0),
    vec4(0.0, 0.0, 1.0, 0.0)
);

const vec3 colors[3] = vec3[3](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.2),
    vec3(0.2, 0.2, 1.0)
);

const int axis0[3] = int[3](1, 0, 0);
const int axis1[3] = int[3](2, 2, 1);

bool intersect_plane(inout float t, vec3 pos, vec3 dir, vec4 plane)
{
    float d = dot(dir, plane.xyz);
    if (abs(d) < 1e-06) return false;

    float n = -(dot(pos, plane.xyz) + plane.w) / d;
    if (n < 0.0) return false;

    t = n;
    return true;
}

// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8#1e7c
float pristine_grid(in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 line_width)
{
    vec2 uv_deriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invert_line = bvec2(line_width.x > 0.5, line_width.y > 0.5);
    vec2 target_width = vec2(
        invert_line.x ? 1.0 - line_width.x : line_width.x,
        invert_line.y ? 1.0 - line_width.y : line_width.y
    );
    vec2 draw_width = clamp(target_width, uv_deriv, vec2(0.5));
    vec2 line_aa = uv_deriv * 1.5;
    vec2 grid_uv = abs(fract(uv) * 2.0 - 1.0);
    grid_uv.x = invert_line.x ? grid_uv.x : 1.0 - grid_uv.x;
    grid_uv.y = invert_line.y ? grid_uv.y : 1.0 - grid_uv.y;
    vec2 grid2 = smoothstep(draw_width + line_aa, draw_width - line_aa, grid_uv);

    grid2 *= clamp(target_width / draw_width, 0.0, 1.0);
    grid2 = mix(grid2, target_width, clamp(uv_deriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invert_line.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invert_line.y ? 1.0 - grid2.y : grid2.y;

    return mix(grid2.x, 1.0, grid2.y);
}

float calc_depth(vec3 p)
{
    vec4 v = u_view_projection * vec4(p, 1.0);
    return v.z / v.w;
}

void main()
{
    vec3 p = world_near;
    vec3 v = normalize(world_far - world_near);

    // Intersect ray with plane
    float t;
    if (!intersect_plane(t, p, v, planes[u_plane]))
    {
        discard;
    }

    // Calculate grid intersection
    vec3 world_pos = p + v * t;
    vec2 pos = u_plane == 0 ? world_pos.yz : (u_plane == 1 ? world_pos.xz : world_pos.xy);
    vec2 ddx = dFdx(pos);
    vec2 ddy = dFdy(pos);

    float epsilon = 1.0 / 255.0;

    // Calculate fade based on 3D distance from camera
    float fade = (1.0 - smoothstep(400.0, 1000.0, length(world_pos - u_view_position.xyz))) * u_view_position.w;
    if (fade < epsilon)
    {
        discard;
    }

    vec2 level_pos;
    float level_size;
    float level_alpha;

    // 10m grid with colored main axes
    level_pos = pos * 0.1;
    level_size = 2.0 / 1000.0;
    level_alpha = pristine_grid(level_pos, ddx * 0.1, ddy * 0.1, vec2(level_size)) * fade;
    if (level_alpha > epsilon)
    {
        vec3 color;
        vec2 loc = abs(level_pos);

        // Anti-aliased axis line detection using screen-space derivatives
        vec2 axis_deriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y))) * 0.1;
        float axis_width = level_size * 1.5;
        float axis_x = 1.0 - smoothstep(axis_width - axis_deriv.x, axis_width + axis_deriv.x, loc.x);
        float axis_y = 1.0 - smoothstep(axis_width - axis_deriv.y, axis_width + axis_deriv.y, loc.y);

        bool is_axis_x = axis_x > 0.01;
        bool is_axis_y = axis_y > 0.01;
        bool is_axis = is_axis_x || is_axis_y;

        if (is_axis_x && is_axis_y)
        {
            color = vec3(1.0);  // Origin: white
        }
        else if (is_axis_x)
        {
            color = colors[axis1[u_plane]];  // Vertical axis
        }
        else if (is_axis_y)
        {
            color = colors[axis0[u_plane]];  // Horizontal axis
        }
        else
        {
            color = vec3(0.4);  // Grid lines
        }

        // Smooth alpha for axes, grid alpha for others
        float axis_alpha = max(axis_x, axis_y);
        float final_alpha = is_axis ? axis_alpha * fade : level_alpha;
        frag_color = vec4(color, final_alpha);
        gl_FragDepth = calc_depth(world_pos);
        return;
    }

    // 1m grid
    level_pos = pos;
    level_size = 1.0 / 100.0;
    level_alpha = pristine_grid(level_pos, ddx, ddy, vec2(level_size)) * fade;
    if (level_alpha > epsilon)
    {
        frag_color = vec4(vec3(0.3), level_alpha);
        gl_FragDepth = calc_depth(world_pos);
        return;
    }

    // 0.1m grid
    level_pos = pos * 10.0;
    level_size = 1.0 / 100.0;
    level_alpha = pristine_grid(level_pos, ddx * 10.0, ddy * 10.0, vec2(level_size)) * fade;
    if (level_alpha > epsilon)
    {
        frag_color = vec4(vec3(0.3), level_alpha);
        gl_FragDepth = calc_depth(world_pos);
        return;
    }

    discard;
}
)";

namespace polymer
{

    enum class grid_plane : uint32_t
    {
        yz = 0,
        xz = 1,
        xy = 2
    };

    inline float3 unproject_ndc(const float4x4 & inv_view_proj, float x, float y, float z)
    {
        float4 clip = float4(x, y, z, 1.0f);
        float4 world = inv_view_proj * clip;
        if (std::abs(world.w) > 1e-7f) world /= world.w;
        return float3(world.x, world.y, world.z);
    }

    class gl_renderable_grid
    {
        gl_shader grid_shader;
        GLuint dummy_vao{ 0 };

    public:

        gl_renderable_grid()
        {
            grid_shader = gl_shader(gl_grid_vert, gl_grid_frag);
            glGenVertexArrays(1, &dummy_vao);
        }

        ~gl_renderable_grid()
        {
            if (dummy_vao) glDeleteVertexArrays(1, &dummy_vao);
        }

        gl_renderable_grid(const gl_renderable_grid &) = delete;
        gl_renderable_grid & operator=(const gl_renderable_grid &) = delete;

        gl_renderable_grid(gl_renderable_grid && other) noexcept : grid_shader(std::move(other.grid_shader)), dummy_vao(other.dummy_vao)
        {
            other.dummy_vao = 0;
        }

        gl_renderable_grid & operator=(gl_renderable_grid && other) noexcept
        {
            if (this != &other)
            {
                if (dummy_vao) glDeleteVertexArrays(1, &dummy_vao);
                grid_shader = std::move(other.grid_shader);
                dummy_vao = other.dummy_vao;
                other.dummy_vao = 0;
            }
            return *this;
        }

        void draw(const float4x4 & view_matrix, const float4x4 & projection_matrix, const float3 & camera_position, grid_plane plane = grid_plane::xz, float opacity = 1.0f)
        {
            float4x4 view_proj = projection_matrix * view_matrix;
            float4x4 inv_view_proj = inverse(view_proj);

            float3 near_bl = unproject_ndc(inv_view_proj, -1.0f, -1.0f, -1.0f);
            float3 near_br = unproject_ndc(inv_view_proj, 1.0f, -1.0f, -1.0f);
            float3 near_tl = unproject_ndc(inv_view_proj, -1.0f, 1.0f, -1.0f);

            float3 far_bl = unproject_ndc(inv_view_proj, -1.0f, -1.0f, 1.0f);
            float3 far_br = unproject_ndc(inv_view_proj, 1.0f, -1.0f, 1.0f);
            float3 far_tl = unproject_ndc(inv_view_proj, -1.0f, 1.0f, 1.0f);

            float3 near_origin = near_bl;
            float3 near_x = near_br - near_bl;
            float3 near_y = near_tl - near_bl;

            float3 far_origin = far_bl;
            float3 far_x = far_br - far_bl;
            float3 far_y = far_tl - far_bl;

            grid_shader.bind();

            grid_shader.uniform("u_near_origin", float4(near_origin, 0.0f));
            grid_shader.uniform("u_near_x", float4(near_x, 0.0f));
            grid_shader.uniform("u_near_y", float4(near_y, 0.0f));
            grid_shader.uniform("u_far_origin", float4(far_origin, 0.0f));
            grid_shader.uniform("u_far_x", float4(far_x, 0.0f));
            grid_shader.uniform("u_far_y", float4(far_y, 0.0f));
            grid_shader.uniform("u_view_position", float4(camera_position, opacity));
            grid_shader.uniform("u_view_projection", view_proj);
            grid_shader.uniform("u_plane", static_cast<int32_t>(plane));

            glBindVertexArray(dummy_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            grid_shader.unbind();
        }

    };

} // end namespace polymer

#endif // renderable_grid_h

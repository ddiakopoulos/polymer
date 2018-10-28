#pragma once

#ifndef polymer_renderer_debug_hpp
#define polymer_renderer_debug_hpp

#include "math-core.hpp"
#include "gl-api.hpp"
#include "procedural_mesh.hpp"

namespace polymer
{
    class renderer_debug
    {
        struct Vertex { float3 position; float3 color; };
        std::vector<Vertex> vertices;

        gl_mesh payload;
        gl_shader shader;

        constexpr static const char debugVertexShader[] = R"(#version 330 
            layout(location = 0) in vec3 v; 
            layout(location = 1) in vec3 c; 
            uniform mat4 u_mvp; 
            out vec3 color; 
            void main() { gl_Position = u_mvp * vec4(v.xyz, 1); color = c; }
        )";

        constexpr static const char debugFragmentShader[] = R"(#version 330 
            in vec3 color; 
            out vec4 f_color; 
            void main() { f_color = vec4(color.rgb, 1); }
        )";

    public:

        renderer_debug() { shader = gl_shader(debugVertexShader, debugFragmentShader); }

        void clear() { vertices.clear(); }

        // World-space
        void draw_line(const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
        {
            vertices.push_back({ from, color });
            vertices.push_back({ to, color });
        }

        void draw_line(const transform & pose, const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
        {
            vertices.push_back({ pose.transform_coord(from), color });
            vertices.push_back({ pose.transform_coord(to), color });
        }

        void draw_box(const transform & pose, const aabb_3d & local_bounds, const float3 color = float3(1, 1, 1))
        {
            auto unit_cube = make_cube();

            for (auto & v : unit_cube.vertices)
            {
                v *= local_bounds.size() / 2.f; // scale
                auto tV = pose.transform_coord(v); // translate
                vertices.push_back({ tV, color });
            }
        }

        void draw_sphere(const transform & pose, const float radius = 1.f, const float3 color = float3(1, 1, 1))
        {
            auto unit_sphere = make_sphere(1.f);

            for (auto & v : unit_sphere.vertices)
            {
                v *= radius;
                auto tV = pose.transform_coord(v);
                vertices.push_back({ tV, color });
            }
        }

        void draw_axis(const transform & pose, const float3 color = float3(1, 1, 1))
        {
            auto axis = make_axis();

            for (int i = 0; i < axis.vertices.size(); ++i)
            {
                auto v = axis.vertices[i];
                v = pose.transform_coord(v);
                vertices.push_back({ v, axis.colors[i].xyz });
            }
        }

        void draw(const float4x4 & viewProjectionMatrix)
        {
            payload.set_vertices(vertices.size(), vertices.data(), GL_STREAM_DRAW);
            payload.set_attribute(0, &Vertex::position);
            payload.set_attribute(1, &Vertex::color);
            payload.set_non_indexed(GL_LINES);

            shader.bind();
            shader.uniform("u_mvp", viewProjectionMatrix * Identity4x4);
            payload.draw_elements();
            shader.unbind();
        }

    };

} // end namespace polymer

#endif // end polymer_renderer_debug_hpp

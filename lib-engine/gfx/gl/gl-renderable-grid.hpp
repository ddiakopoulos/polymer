#pragma once

#ifndef renderable_grid_h
#define renderable_grid_h

#include "gl-api.hpp"

constexpr const char gl_grid_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main() { gl_Position = u_mvp * vec4(vertex.xyz, 1); }
)";

constexpr const char gl_grid_frag[] = R"(#version 330
    uniform vec4 u_color;
    out vec4 f_color;
    void main() { f_color = u_color; }
)";

namespace polymer
{

    class gl_renderable_grid
    {
        gl_shader gridShader;
        gl_mesh gridMesh;

    public:

        gl_renderable_grid(float density = 1.0f, uint32_t qx = 32, uint32_t qy = 32)
        {
            gridShader = gl_shader(gl_grid_vert, gl_grid_frag);

            const uint32_t gridSize = (qx + qy + 2) * 2;
            const float width = density * qx / 2;
            const float height = density * qy / 2;

            struct Vertex { float3 position; };
            Vertex * gridVerts = new Vertex[gridSize]; 

            uint32_t counter = 0; 
            for (uint32_t i = 0; i < (qy + 1) * 2; i += 2)
            {
                gridVerts[i] = { { -width, 0.0f, -height + counter * density } };
                gridVerts[i+1] = { { width, 0.0f, -height + counter * density } };
                ++counter;
            }

            counter = 0;
            for (uint32_t i = (qy + 1) * 2; i < gridSize; i += 2)
            {
                gridVerts[i] = { { -width + counter * density, 0.0f, -height} };
                gridVerts[i+1] = { { -width + counter * density, 0.0f, height } };
                ++counter;
            }

            gridMesh.set_vertices(gridSize, gridVerts, GL_STATIC_DRAW);
            gridMesh.set_attribute(0, &Vertex::position);
            gridMesh.set_non_indexed(GL_LINES);
        }

        void draw(const float4x4 & viewProjectionMatrix, const float4x4 & modelMatrix = Identity4x4, const float4 color = { 1, 1, 1, 1 })
        {
            gridShader.bind();
            gridShader.uniform("u_color", color);
            gridShader.uniform("u_mvp", mul(viewProjectionMatrix, modelMatrix));
            gridMesh.draw_elements();
            gridShader.unbind();
        }

    }; 
    
} // end namespace polymer

#endif // renderable_grid_h

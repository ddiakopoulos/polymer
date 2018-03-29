#pragma once

#ifndef renderable_grid_h
#define renderable_grid_h

#include "gl-api.hpp"

constexpr const char gridVertexShader[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    uniform vec3 u_color;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = u_color;
    }
)";

constexpr const char gridFragmentShader[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    void main()
    {
        f_color = vec4(color.rgb, 1);
    }
)";

namespace polymer
{

class RenderableGrid
{
    GlShader gridShader;
    GlMesh gridMesh;
    float3 origin = { 0.f, 0.f, 0.f };
    int gridSz = 0;
    int qx, qy;

public:

    RenderableGrid(float density = 1.0f, int qx = 32, int qy = 32) : qx(qx), qy(qy)
    {
        gridShader = GlShader(gridVertexShader, gridFragmentShader);

        const int gridSize = gridSz = (qx + qy + 2) * 2;
        float width = density * qx / 2;
        float height = density * qy / 2;

        struct Vertex { float3 position; };
        Vertex * gridVerts = new Vertex[gridSize]; 

        int counter = 0; 
        for (int i = 0; i < (qy + 1) * 2; i += 2)
        {
            gridVerts[i] = { { -width, 0.0f, -height + counter * density } };
            gridVerts[i+1] = { { width, 0.0f, -height + counter * density } };
            ++counter;
        }

        counter = 0;
        for (int i = (qy + 1) * 2; i < gridSize; i += 2)
        {
            gridVerts[i] = { { -width + counter * density, 0.0f, -height} };
            gridVerts[i+1] = { { -width + counter * density, 0.0f, height } };
            ++counter;
        }

        gridMesh.set_vertices(gridSize, gridVerts, GL_STATIC_DRAW);
        gridMesh.set_attribute(0, &Vertex::position);
        gridMesh.set_non_indexed(GL_LINES);
    }

    void set_origin(float3 newOrigin)
    {
        origin = newOrigin; 
    }

    void draw(const float4x4 & viewProj)    
    {
        auto model = make_translation_matrix(origin);
        auto modelViewProjectionMatrix = mul(viewProj, model);
        gridShader.bind();
        gridShader.uniform("u_color", float3(1, 1, 1));
        gridShader.uniform("u_mvp", modelViewProjectionMatrix);
        gridMesh.draw_elements();
        gridShader.unbind();
    }

};
    
}

#endif // renderable_grid_h

// See COPYING file for attribution information

#pragma once

#ifndef meshline_h
#define meshline_h

#include "gl-api.hpp"

namespace avl
{

class GlRenderableMeshline
{
    GlShader shader;
    GlMesh mesh;
    
    std::vector<float3> previous;
    std::vector<float3> next;
    std::vector<float> side;
    std::vector<float> width;
    std::vector<float2> uvs;
    std::vector<uint3> indices;
    
    GlMesh make_line_mesh(const std::vector<float3> & curve)
    {
        GlMesh m;
        
        int components = 3 + 3 + 3 + 1 + 1 + 2;
        std::vector<float> buffer;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            buffer.push_back(curve[i].x);
            buffer.push_back(curve[i].y);
            buffer.push_back(curve[i].z);
            buffer.push_back(previous[i].x);
            buffer.push_back(previous[i].y);
            buffer.push_back(previous[i].z);
            buffer.push_back(next[i].x);
            buffer.push_back(next[i].y);
            buffer.push_back(next[i].z);
            buffer.push_back(side[i]);
            buffer.push_back(width[i]);
            buffer.push_back(uvs[i].x);
            buffer.push_back(uvs[i].y);
        }
        
        m.set_vertex_data(buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);
        m.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 0);  // Positions
        m.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 3);  // Previous
        m.set_attribute(2, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 6);  // Next
        m.set_attribute(3, 1, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 9);  // Side
        m.set_attribute(4, 1, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 10); // Width
        m.set_attribute(5, 2, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 11); // Uvs
        if (indices.size() > 0) m.set_elements(indices, GL_STATIC_DRAW);
        
        return m;
    }

public:

    GlRenderableMeshline()
    {
        shader = GlShader(read_file_text("../assets/shaders/prototype/meshline_vert.glsl"), read_file_text("../assets/shaders/prototype/meshline_frag.glsl"));
    }
    
    void set_vertices(const std::vector<float3> & vertices)
    {
        const auto l = vertices.size();
        
        side.clear();
        width.clear();
        uvs.clear();
        previous.clear();
        next.clear();
        indices.clear();
        
        for (uint32_t i = 0; i < l; i += 2)
        {
            side.push_back(1.f);
            side.push_back(-1.f);
        }
        
        for (uint32_t i = 0; i < l; i += 2)
        {
            width.push_back(1.f);
            width.push_back(1.f);
        }
        
        for (uint32_t i = 0; i < l; i += 2)
        {
            uvs.push_back(float2(float(i) / (l - 1), 0.f));
            uvs.push_back(float2(float(i) / (l - 1), 1.f));
        }
        
        float3 v;
        if (vertices[0] == vertices[l - 1]) v = vertices[l - 2];
        else v = vertices[0];
        previous.push_back(v);
        previous.push_back(v);
        
        for (uint32_t i = 0; i < l - 1; i += 2)
        {
            v = vertices[i];
            previous.push_back(v);
            previous.push_back(v);
        }
        
        for (uint32_t i = 2; i < l; i += 2)
        {
            v = vertices[i];
            next.push_back(v);
            next.push_back(v);
        }
        
        if (vertices[l - 1] == vertices[0]) v = vertices[1];
        else v = vertices[l - 1];
        next.push_back(v);
        next.push_back(v);

        for (uint32_t i = 0; i < l - 1; i ++)
        {
            uint32_t n = i * 2;
            indices.push_back(uint3(n + 0, n + 1, n + 2));
            indices.push_back(uint3(n + 2, n + 1, n + 3));
        }
        
        mesh = make_line_mesh(vertices);
    }
    
    void render(const GlCamera & camera, const float4x4 model, const float2 screenDims, const float3 color, const float lineWidth = 24.f)
    {
        shader.bind();

        auto projMat = camera.get_projection_matrix(screenDims.x / screenDims.y);
        auto viewMat = camera.get_view_matrix();
        
        shader.uniform("u_projMat", projMat);
        shader.uniform("u_modelViewMat", mul(viewMat, model));
        
        shader.uniform("resolution", screenDims);
        shader.uniform("lineWidth", lineWidth);
        shader.uniform("color", color);
        shader.uniform("opacity", 1.0f);
        shader.uniform("near", camera.nearclip);
        shader.uniform("far", camera.farclip);
        shader.uniform("sizeAttenuation", 0.0f);
        shader.uniform("useMap", 0.0f);
        
        mesh.draw_elements();
        
        shader.unbind();
    }

};
    
}

#endif // GlRenderableMeshline

#ifndef gl_mesh_hpp
#define gl_mesh_hpp

#include "math-core.hpp"
#include "string_utils.hpp"
#include "gl-api.hpp"
#include "file_io.hpp"
#include "gl-loaders.hpp"
#include "geometry.hpp"

#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <assert.h>

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif

namespace polymer
{
    inline GlMesh make_mesh_from_geometry(const Geometry & geometry, const GLenum usage = GL_STATIC_DRAW)
    {
        assert(geometry.vertices.size() > 0);

        GlMesh m;

        int vertexOffset = 0;
        int normalOffset = 0;
        int colorOffset = 0;
        int texOffset = 0;
        int tanOffset = 0;
        int bitanOffset = 0;

        int components = 3;

        if (geometry.normals.size() != 0)
        {
            normalOffset = components; components += 3;
        }

        if (geometry.colors.size() != 0)
        {
            colorOffset = components; components += 3;
        }

        if (geometry.texcoord0.size() != 0)
        {
            texOffset = components; components += 2;
        }

        if (geometry.tangents.size() != 0)
        {
            tanOffset = components; components += 3;
        }

        if (geometry.bitangents.size() != 0)
        {
            bitanOffset = components; components += 3;
        }

        std::vector<float> buffer;
        buffer.reserve(geometry.vertices.size() * components);

        for (size_t i = 0; i < geometry.vertices.size(); ++i)
        {
            buffer.push_back(geometry.vertices[i].x);
            buffer.push_back(geometry.vertices[i].y);
            buffer.push_back(geometry.vertices[i].z);

            if (normalOffset)
            {
                buffer.push_back(geometry.normals[i].x);
                buffer.push_back(geometry.normals[i].y);
                buffer.push_back(geometry.normals[i].z);
            }

            if (colorOffset)
            {
                buffer.push_back(geometry.colors[i].x);
                buffer.push_back(geometry.colors[i].y);
                buffer.push_back(geometry.colors[i].z);
            }

            if (texOffset)
            {
                buffer.push_back(geometry.texcoord0[i].x);
                buffer.push_back(geometry.texcoord0[i].y);
            }

            if (tanOffset)
            {
                buffer.push_back(geometry.tangents[i].x);
                buffer.push_back(geometry.tangents[i].y);
                buffer.push_back(geometry.tangents[i].z);
            }

            if (bitanOffset)
            {
                buffer.push_back(geometry.bitangents[i].x);
                buffer.push_back(geometry.bitangents[i].y);
                buffer.push_back(geometry.bitangents[i].z);
            }
        }

        m.set_vertex_data(buffer.size() * sizeof(float), buffer.data(), usage);
        m.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + vertexOffset);
        if (normalOffset) m.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + normalOffset);
        if (colorOffset) m.set_attribute(2, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + colorOffset);
        if (texOffset) m.set_attribute(3, 2, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + texOffset);
        if (tanOffset) m.set_attribute(4, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + tanOffset);
        if (bitanOffset) m.set_attribute(5, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + bitanOffset);

        if (geometry.faces.size() > 0)
        {
            m.set_elements(geometry.faces, usage);
        }

        return m;
    }

}

#pragma warning(pop)

#endif // gl_mesh_hpp

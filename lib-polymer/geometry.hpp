#pragma once

#ifndef polymer_geometry_hpp
#define polymer_geometry_hpp

#include "math-core.hpp"

namespace polymer
{

    struct runtime_mesh
    {
        std::vector<float3> vertices;
        std::vector<float3> normals;
        std::vector<float4> colors;
        std::vector<float2> texcoord0;
        std::vector<float2> texcoord1;
        std::vector<float3> tangents;
        std::vector<float3> bitangents;
        std::vector<uint3> faces;
        std::vector<uint32_t> material;
    };

    struct runtime_mesh_quads : public runtime_mesh
    {
        std::vector<uint4> quads;
    };

    inline runtime_mesh quadmesh_to_trimesh(runtime_mesh_quads & quadmesh)
    {
        runtime_mesh trimesh = quadmesh;

        for(auto & q : quadmesh.quads)
        {
            trimesh.faces.push_back({q.x,q.y,q.z});
            trimesh.faces.push_back({q.x,q.z,q.w});
        }

        return trimesh;
    }

    typedef runtime_mesh geometry;

    inline aabb_3d compute_bounds(const geometry & g)
    {
        aabb_3d bounds;

        bounds._min = float3(std::numeric_limits<float>::infinity());
        bounds._max = -bounds.min();

        for (const auto & vertex : g.vertices)
        {
            bounds._min = linalg::min(bounds.min(), vertex);
            bounds._max = linalg::max(bounds.max(), vertex);
        }
        return bounds;
    }

    // Lengyel, Eric. "Computing Tangent Space Basis Vectors for an Arbitrary Mesh".
    // Terathon Software 3D Graphics Library, 2001.
    inline void compute_tangents(geometry & g)
    {
        if (!g.texcoord0.size() || !g.normals.size()) return;

        g.tangents.resize(g.vertices.size());
        g.bitangents.resize(g.vertices.size());

        // Each face
        for (size_t i = 0; i < g.faces.size(); ++i)
        {
            uint3 face = g.faces[i];

            const float3 & v0 = g.vertices[face.x];
            const float3 & v1 = g.vertices[face.y];
            const float3 & v2 = g.vertices[face.z];

            const float2 & w0 = g.texcoord0[face.x];
            const float2 & w1 = g.texcoord0[face.y];
            const float2 & w2 = g.texcoord0[face.z];

            float x1 = v1.x - v0.x;
            float x2 = v2.x - v0.x;
            float y1 = v1.y - v0.y;
            float y2 = v2.y - v0.y;
            float z1 = v1.z - v0.z;
            float z2 = v2.z - v0.z;

            float s1 = w1.x - w0.x;
            float s2 = w2.x - w0.x;
            float t1 = w1.y - w0.y;
            float t2 = w2.y - w0.y;

            float r = (s1 * t2 - s2 * t1);

            if (r != 0.f) r = 1.f / r;

            // Tangent in the S direction
            const float3 tangent((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);

            // Accumulate
            g.tangents[face.x] += tangent;
            g.tangents[face.y] += tangent;
            g.tangents[face.z] += tangent;
        }

        // Tangents
        for (size_t i = 0; i < g.vertices.size(); ++i)
        {
            const float3 normal = g.normals[i];
            const float3 tangent = g.tangents[i];

            // Gram-Schmidt orthogonalize
            g.tangents[i] = (tangent - normal * dot(normal, tangent));

            const float len = length(g.tangents[i]);

            if (len > 0.f) g.tangents[i] = g.tangents[i] / (float)std::sqrt(len);
        }

        // Bitangents 
        for (size_t i = 0; i < g.vertices.size(); ++i)
        {
            g.bitangents[i] = safe_normalize(cross(g.normals[i], g.tangents[i]));
        }
    }

    inline void compute_normals(geometry & g, bool smooth = true)
    {
        constexpr double NORMAL_EPSILON = 0.0001;

        g.normals.resize(g.vertices.size());

        for (auto & n : g.normals) n = float3(0, 0, 0);

        std::vector<uint32_t> uniqueVertIndices(g.vertices.size(), 0);
        if (smooth)
        {
            for (uint32_t i = 0; i < uniqueVertIndices.size(); ++i)
            {
                if (uniqueVertIndices[i] == 0)
                {
                    uniqueVertIndices[i] = i + 1;
                    const float3 v0 = g.vertices[i];
                    for (auto j = i + 1; j < g.vertices.size(); ++j)
                    {
                        const float3 v1 = g.vertices[j];
                        if (length2(v1 - v0) < NORMAL_EPSILON)
                        {
                            uniqueVertIndices[j] = uniqueVertIndices[i];
                        }
                    }
                }
            }
        }

        uint32_t idx0, idx1, idx2;
        for (size_t i = 0; i < g.faces.size(); ++i)
        {
            const auto & f = g.faces[i];

            idx0 = (smooth) ? uniqueVertIndices[f.x] - 1 : f.x;
            idx1 = (smooth) ? uniqueVertIndices[f.y] - 1 : f.y;
            idx2 = (smooth) ? uniqueVertIndices[f.z] - 1 : f.z;

            const float3 v0 = g.vertices[idx0];
            const float3 v1 = g.vertices[idx1];
            const float3 v2 = g.vertices[idx2];

            float3 e0 = v1 - v0;
            float3 e1 = v2 - v0;
            float3 e2 = v2 - v1;

            if (length2(e0) < NORMAL_EPSILON) continue;
            if (length2(e1) < NORMAL_EPSILON) continue;
            if (length2(e2) < NORMAL_EPSILON) continue;

            const float3 n = safe_normalize(cross(e0, e1));

            g.normals[f.x] += n;
            g.normals[f.y] += n;
            g.normals[f.z] += n;

            // Copy normals for non-unique verts
            if (smooth)
            {
                for (uint32_t i = 0; i < g.vertices.size(); ++i)
                {
                    g.normals[i] = g.normals[uniqueVertIndices[i] - 1];
                }
            }
        }

        for (auto & n : g.normals) n = safe_normalize(n);
    }

    inline void rescale_geometry(runtime_mesh & g, float radius = 1.0f)
    {
        auto bounds = compute_bounds(g);

        float3 r = bounds.size() * 0.5f;
        float3 center = bounds.center();

        float oldRadius = std::max(r.x(), std::max(r.y(), r.z()));
        float scale = radius / oldRadius;

        for (auto & v : g.vertices)
        {
            float3 fixed = scale * (v - center);
            v = fixed;
        }
    }

    inline void recenter_geometry(runtime_mesh & m)
    {
        float3 average_position = float3(0.f);

        for (auto & v : m.vertices)
        {
            average_position += v;
        }

        average_position /= float(m.vertices.size());

        auto average_relative_pose = transform({ 0, 0, 0, 1 }, average_position);

        for (auto & v : m.vertices)
        {
            v = average_relative_pose.detransform_coord(v);
        }
    }

    // warning: only accounts for vertices, faces, normals, and texcoords
    inline geometry concatenate_geometry(const geometry & a, const geometry & b)
    {
        geometry s;

        s.vertices.insert(s.vertices.end(), a.vertices.begin(), a.vertices.end());
        s.vertices.insert(s.vertices.end(), b.vertices.begin(), b.vertices.end());

        s.normals.insert(s.normals.end(), a.normals.begin(), a.normals.end());
        s.normals.insert(s.normals.end(), b.normals.begin(), b.normals.end());

        s.texcoord0.insert(s.texcoord0.end(), a.texcoord0.begin(), a.texcoord0.end());
        s.texcoord0.insert(s.texcoord0.end(), b.texcoord0.begin(), b.texcoord0.end());

        s.faces.insert(s.faces.end(), a.faces.begin(), a.faces.end());
        for (auto & f : b.faces) s.faces.push_back({ (int)a.vertices.size() + f.x, (int)a.vertices.size() + f.y, (int)a.vertices.size() + f.z });
        return s;
    }

    inline bool intersect_ray_mesh(const ray & ray,
        const geometry & mesh,
        float * outRayT = nullptr,
        float3 * outFaceNormal = nullptr,
        float2 * outTexcoord = nullptr,
        aabb_3d * bounds = nullptr)
    {
        float best_t = std::numeric_limits<float>::infinity(), t;
        uint3 best_face = { 0, 0, 0 };
        float2 outUv;

        for (int f = 0; f < mesh.faces.size(); ++f)
        {
            auto & tri = mesh.faces[f];
            if (intersect_ray_triangle(ray, mesh.vertices[tri.x], mesh.vertices[tri.y], mesh.vertices[tri.z], &t, &outUv) && t < best_t)
            {
                best_t = t;
                best_face = mesh.faces[f];
            }
        }

        if (best_t == std::numeric_limits<float>::infinity()) return false;

        if (outRayT)
        {
            *outRayT = best_t;
        }

        if (outTexcoord && mesh.texcoord0.size())
        {
            float u = outUv.x;
            float v = outUv.y;
            float w = 1.f - u - v;

            float3 weight = { std::max(0.f, w), std::max(0.f, u), std::max(0.f, v) };

            const float fDiv = 1.f / (weight.x + weight.y + weight.z);
            weight *= fDiv;

            const float2 tc_0 = mesh.texcoord0[best_face.x];
            const float2 tc_1 = mesh.texcoord0[best_face.y];
            const float2 tc_2 = mesh.texcoord0[best_face.z];

            *outTexcoord = tc_0 * weight.x + tc_1 * weight.y + tc_2 * weight.z;
        }

        if (outFaceNormal)
        {
            const auto v0 = mesh.vertices[best_face.x];
            const auto v1 = mesh.vertices[best_face.y];
            const auto v2 = mesh.vertices[best_face.z];
            *outFaceNormal = safe_normalize(cross(v1 - v0, v2 - v0));
        }

        return true;
    }

} // end namespace polymer

#endif // end polymer_geometry_hpp

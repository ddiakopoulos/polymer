#ifndef geometry_hpp
#define geometry_hpp

#include "math-core.hpp"
#include "../lib-model-io/model-io.hpp"

using namespace avl;

typedef runtime_mesh Geometry;

inline Bounds3D compute_bounds(const Geometry & g)
{
    Bounds3D bounds;

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
inline void compute_tangents(Geometry & g)
{
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

        if (r != 0.0f) r = 1.0f / r;

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

        if (len > 0.0f) g.tangents[i] = g.tangents[i] / (float)sqrt(len);
    }

    // Bitangents 
    for (size_t i = 0; i < g.vertices.size(); ++i)
    {
        g.bitangents[i] = safe_normalize(cross(g.normals[i], g.tangents[i]));
    }
}

inline void compute_normals(Geometry & g, bool smooth = true)
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

inline void rescale_geometry(Geometry & g, float radius = 1.0f)
{
    auto bounds = compute_bounds(g);

    float3 r = bounds.size() * 0.5f;
    float3 center = bounds.center();

    float oldRadius = std::max(r.x, std::max(r.y, r.z));
    float scale = radius / oldRadius;

    for (auto & v : g.vertices)
    {
        float3 fixed = scale * (v - center);
        v = fixed;
    }
}

inline Geometry concatenate_geometry(const Geometry & a, const Geometry & b)
{
    Geometry s;
    s.vertices.insert(s.vertices.end(), a.vertices.begin(), a.vertices.end());
    s.vertices.insert(s.vertices.end(), b.vertices.begin(), b.vertices.end());
    s.faces.insert(s.faces.end(), a.faces.begin(), a.faces.end());
    for (auto & f : b.faces) s.faces.push_back({ (int)a.vertices.size() + f.x, (int)a.vertices.size() + f.y, (int)a.vertices.size() + f.z });
    return s;
}

inline bool intersect_ray_mesh(const Ray & ray, const Geometry & mesh, float * outRayT = nullptr, float3 * outFaceNormal = nullptr, Bounds3D * bounds = nullptr)
{
    float bestT = std::numeric_limits<float>::infinity(), t;
    uint3 bestFace = { 0, 0, 0 };
    float2 outUv;

    Bounds3D meshBounds = (bounds) ? *bounds : compute_bounds(mesh);
    if (!meshBounds.contains(ray.origin) && intersect_ray_box(ray, meshBounds.min(), meshBounds.max()))
    {
        for (int f = 0; f < mesh.faces.size(); ++f)
        {
            auto & tri = mesh.faces[f];
            if (intersect_ray_triangle(ray, mesh.vertices[tri.x], mesh.vertices[tri.y], mesh.vertices[tri.z], &t, &outUv) && t < bestT)
            {
                bestT = t;
                bestFace = mesh.faces[f];
            }
        }
    }

    if (bestT == std::numeric_limits<float>::infinity()) return false;

    if (outRayT) *outRayT = bestT;

    if (outFaceNormal)
    {
        auto v0 = mesh.vertices[bestFace.x];
        auto v1 = mesh.vertices[bestFace.y];
        auto v2 = mesh.vertices[bestFace.z];
        float3 n = safe_normalize(cross(v1 - v0, v2 - v0));
        *outFaceNormal = n;
    }

    return true;
}

#endif // end geometry_hpp
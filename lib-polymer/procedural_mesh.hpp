#pragma once

#ifndef procedural_mesh_h
#define procedural_mesh_h

#include "math-core.hpp"
#include "splines.hpp"
#include "geometry.hpp"
#include "algo_misc.hpp"
#include <assert.h>

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#endif

namespace polymer
{
    
    inline geometry make_cube()
    {
        geometry cube;
        
        const struct CubeVertex { float3 position; float3 normal; float2 texCoord; } verts[] =
        {
            // -X
            { { -0.5f, -0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 } },
            { { -0.5f, -0.5f, +0.5f }, { -1, 0, 0 }, { 1, 0 } },
            { { -0.5f, +0.5f, +0.5f }, { -1, 0, 0 }, { 1, 1 } },
            { { -0.5f, +0.5f, -0.5f }, { -1, 0, 0 }, { 0, 1 } },

            // +X
            { { +0.5f, -0.5f, +0.5f }, { +1, 0, 0 }, { 0, 0 } },
            { { +0.5f, -0.5f, -0.5f }, { +1, 0, 0 }, { 1, 0 } },
            { { +0.5f, +0.5f, -0.5f }, { +1, 0, 0 }, { 1, 1 } },
            { { +0.5f, +0.5f, +0.5f }, { +1, 0, 0 }, { 0, 1 } },

            // -Y
            { { -0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 } },
            { { +0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 1, 0 } },
            { { +0.5f, -0.5f, +0.5f }, { 0, -1, 0 }, { 1, 1 } },
            { { -0.5f, -0.5f, +0.5f }, { 0, -1, 0 }, { 0, 1 } },

            // +Y
            { { +0.5f, +0.5f, -0.5f }, { 0, +1, 0 }, { 0, 0 } },
            { { -0.5f, +0.5f, -0.5f }, { 0, +1, 0 }, { 1, 0 } },
            { { -0.5f, +0.5f, +0.5f }, { 0, +1, 0 }, { 1, 1 } },
            { { +0.5f, +0.5f, +0.5f }, { 0, +1, 0 }, { 0, 1 } },

            // -Z
            { { -0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 } },
            { { -0.5f, +0.5f, -0.5f }, { 0, 0, -1 }, { 1, 0 } },
            { { +0.5f, +0.5f, -0.5f }, { 0, 0, -1 }, { 1, 1 } },
            { { +0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 1 } },

            // +Z
            { { -0.5f, +0.5f, +0.5f }, { 0, 0, +1 }, { 0, 0 } },
            { { -0.5f, -0.5f, +0.5f }, { 0, 0, +1 }, { 1, 0 } },
            { { +0.5f, -0.5f, +0.5f }, { 0, 0, +1 }, { 1, 1 } },
            { { +0.5f, +0.5f, +0.5f }, { 0, 0, +1 }, { 0, 1 } },
        };
        
        std::vector<uint4> quads = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 11 }, { 12, 13, 14, 15 }, { 16, 17, 18, 19 }, { 20, 21, 22, 23 } };
        
        for(auto & q : quads)
        {
            cube.faces.push_back({q.x,q.y,q.z});
            cube.faces.push_back({q.x,q.z,q.w});
        }
        
        for (int i = 0; i < 24; ++i)
        {
            cube.vertices.push_back(verts[i].position);
            cube.normals.push_back(verts[i].normal);
            cube.texcoord0.push_back(verts[i].texCoord);
        }
        
        compute_bounds(cube);
        compute_normals(cube, false);
        compute_tangents(cube);
        
        return cube;
    }

    inline geometry make_cube(const float3 s)
    {
        geometry cube;
        
        const struct CubeVertex { float3 position; float3 normal; float2 texCoord; } verts[] =
        {
            // -X
            { { -s.x, -s.y, -s.z }, { -1, 0, 0 }, { 0, 0 } },
            { { -s.x, -s.y, +s.z }, { -1, 0, 0 }, { 1, 0 } },
            { { -s.x, +s.y, +s.z }, { -1, 0, 0 }, { 1, 1 } },
            { { -s.x, +s.y, -s.z }, { -1, 0, 0 }, { 0, 1 } },

            // +X            
            { { +s.x, -s.y, +s.z }, { +1, 0, 0 }, { 0, 0 } },
            { { +s.x, -s.y, -s.z }, { +1, 0, 0 }, { 1, 0 } },
            { { +s.x, +s.y, -s.z }, { +1, 0, 0 }, { 1, 1 } },
            { { +s.x, +s.y, +s.z }, { +1, 0, 0 }, { 0, 1 } },

            // -Y            
            { { -s.x, -s.y, -s.z }, { 0, -1, 0 }, { 0, 0 } },
            { { +s.x, -s.y, -s.z }, { 0, -1, 0 }, { 1, 0 } },
            { { +s.x, -s.y, +s.z }, { 0, -1, 0 }, { 1, 1 } },
            { { -s.x, -s.y, +s.z }, { 0, -1, 0 }, { 0, 1 } },

            // +Y            
            { { +s.x, +s.y, -s.z }, { 0, +1, 0 }, { 0, 0 } },
            { { -s.x, +s.y, -s.z }, { 0, +1, 0 }, { 1, 0 } },
            { { -s.x, +s.y, +s.z }, { 0, +1, 0 }, { 1, 1 } },
            { { +s.x, +s.y, +s.z }, { 0, +1, 0 }, { 0, 1 } },

            // -Z            
            { { -s.x, -s.y, -s.z }, { 0, 0, -1 }, { 0, 0 } },
            { { -s.x, +s.y, -s.z }, { 0, 0, -1 }, { 1, 0 } },
            { { +s.x, +s.y, -s.z }, { 0, 0, -1 }, { 1, 1 } },
            { { +s.x, -s.y, -s.z }, { 0, 0, -1 }, { 0, 1 } },

            // +Z            
            { { -s.x, +s.y, +s.z }, { 0, 0, +1 }, { 0, 0 } },
            { { -s.x, -s.y, +s.z }, { 0, 0, +1 }, { 1, 0 } },
            { { +s.x, -s.y, +s.z }, { 0, 0, +1 }, { 1, 1 } },
            { { +s.x, +s.y, +s.z }, { 0, 0, +1 }, { 0, 1 } },
        };
        
        std::vector<uint4> quads = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 11 }, { 12, 13, 14, 15 }, { 16, 17, 18, 19 }, { 20, 21, 22, 23 } };
        
        for(auto & q : quads)
        {
            cube.faces.push_back({q.x,q.y,q.z});
            cube.faces.push_back({q.x,q.z,q.w});
        }
        
        for (int i = 0; i < 24; ++i)
        {
            cube.vertices.push_back(verts[i].position);
            cube.normals.push_back(verts[i].normal);
            cube.texcoord0.push_back(verts[i].texCoord);
        }
        
        compute_bounds(cube);
        compute_normals(cube, false);
        compute_tangents(cube);
        
        return cube;
    }
    
    inline geometry make_sphere(float radius)
    {
        geometry sphereGeom;
        
        uint32_t U = 32, V = 32;
        
        for (uint32_t ui = 0; ui < U; ++ui)
        {
            const float u = float(ui) / (U - 1) * float(POLYMER_PI);

            for (uint32_t vi = 0; vi < V; ++vi)
            {
                const float v = float(vi) / (V - 1) * 2 * float(POLYMER_PI);
                const float3 normal = cartsesian_coord(u, v);
                sphereGeom.vertices.push_back({normal * radius});
                sphereGeom.normals.push_back(normalize(normal));
                sphereGeom.texcoord0.push_back({ 1.f - (float(vi) / (V - 1)), 1.f - (float(ui) / (U - 1)) });
            }
        }
        
        for (uint32_t ui = 0; ui < U; ++ui)
        {
            const uint32_t un = (ui + 1) % U;
            for (uint32_t vi = 0; vi < V; ++vi)
            {
                const uint32_t vn = (vi + 1) % V;
                sphereGeom.faces.push_back({ui * V + vi, un * V + vi, un * V + vn});
                sphereGeom.faces.push_back({ui * V + vi, un * V + vn, ui * V + vn});
            }
        }

        compute_bounds(sphereGeom);

        return sphereGeom;
    }

    inline geometry make_cylinder(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded = false)
    {
        geometry cylinderGeom;
        
        float heightHalf = height / 2.f;
        
        std::vector<std::vector<uint32_t>> vertexRowArray;
        
        for (int y = 0; y <= heightSegments; y++)
        {
            std::vector<uint32_t> newRow;
            
            float v = (float) y / heightSegments;
            float radius = v * (radiusBottom - radiusTop) + radiusTop;
            
            for (int x = 0; x <= radialSegments; x++)
            {
                float u = (float) x / (float) radialSegments;
                
                float3 vertex;
                
                vertex.x = radius * sin(u * float(POLYMER_TAU));
                vertex.y = -v * height + heightHalf;
                vertex.z = radius * cos(u * float(POLYMER_TAU));
                
                cylinderGeom.vertices.push_back(vertex);
                newRow.push_back((int) cylinderGeom.vertices.size() - 1);
            }
            
            vertexRowArray.push_back(newRow);
        }
        
        float tanTheta = (radiusBottom - radiusTop) / height;
        
        float3 na, nb;
        
        for (int x = 0; x < radialSegments; x++)
        {
            if (radiusTop != 0)
            {
                na = cylinderGeom.vertices[vertexRowArray[0][x]];
                nb = cylinderGeom.vertices[vertexRowArray[0][x + 1]];
            }
            else
            {
                na = cylinderGeom.vertices[vertexRowArray[1][x]];
                nb = cylinderGeom.vertices[vertexRowArray[1][x + 1]];
            }
            
            na.y = sqrt(na.x * na.x + na.z * na.z) * tanTheta;
            nb.y = sqrt(nb.x * nb.x + nb.z * nb.z) * tanTheta;
            
            na = safe_normalize(na);
            nb = safe_normalize(nb);
            
            for (int y = 0; y < heightSegments; y++)
            {
                uint32_t v1 = vertexRowArray[y][x];
                uint32_t v2 = vertexRowArray[y + 1][x];
                uint32_t v3 = vertexRowArray[y + 1][x + 1];
                uint32_t v4 = vertexRowArray[y][x + 1];
                
                float3 n1 = na;
                float3 n2 = na;
                float3 n3 = nb;
                float3 n4 = nb;
                
                cylinderGeom.faces.emplace_back(v1, v2, v4);
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
                
                
                cylinderGeom.faces.emplace_back(v2, v3, v4);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
                cylinderGeom.normals.push_back(n4);
            }
        }
        
        // Top
        if (!openEnded && radiusTop > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, heightHalf, 0));
            
            for (int x = 0; x < radialSegments; x++)
            {
                
                uint32_t v1 = vertexRowArray[0][x];
                uint32_t v2 = vertexRowArray[0][x + 1];
                uint32_t v3 = (int) cylinderGeom.vertices.size() - 1;
                
                float3 n1 = float3(0, 1, 0);
                float3 n2 = float3(0, 1, 0);
                float3 n3 = float3(0, 1, 0);
                
                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
            }
            
        }
        
        // Bottom
        if (!openEnded && radiusBottom > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, -heightHalf, 0));
            
            for (int x = 0; x < radialSegments; x++)
            {
                uint32_t v1 = vertexRowArray[heightSegments][x + 1];
                uint32_t v2 = vertexRowArray[heightSegments][x];
                uint32_t v3 = (int) cylinderGeom.vertices.size() - 1;
                
                float3 n1 = float3(0, -1, 0);
                float3 n2 = float3(0, -1, 0);
                float3 n3 = float3(0, -1, 0);
                
                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
            }
            
        }
        
        //cylinderGeom.compute_normals();
        
        return cylinderGeom;
    }
    
    inline geometry make_ring(float innerRadius = 2.0f, float outerRadius = 2.5f)
    {
        geometry ringGeom;
        
        uint32_t thetaSegments = 8;
        uint32_t phiSegments = 2;
        
        float thetaStart = 0.0;
        float thetaLength = POLYMER_TAU;
        
        float radius = innerRadius;
        float radiusStep = ((outerRadius - innerRadius) / (float) phiSegments);
        
        //std::vector<float2> uvs;
        
        // Number of circles inside ring
        for (uint32_t i = 0; i <= phiSegments; i++)
        {
            float3 vertex;
            
            // Segments per cicle
            for (uint32_t o = 0; o <= thetaSegments; o++)
            {
                auto segment = thetaStart + (float) o / (float) thetaSegments * thetaLength;
                
                vertex.x = radius * cos(segment);
                vertex.y = radius * sin(segment);
                vertex.z = 0;
                
                ringGeom.vertices.push_back(vertex);
                ringGeom.texcoord0.emplace_back((vertex.x / outerRadius + 1.0f) / 2.0f, (vertex.y / outerRadius + 1.0f) / 2.0f);
                
            }
            radius += radiusStep;
        }
        
        for (uint32_t i = 0; i < phiSegments; i++)
        {
            uint32_t thetaSegment = i * thetaSegments;
            
            for (uint32_t o = 0; o <= thetaSegments; o++)
            {
                uint32_t segment = o + thetaSegment;
                
                uint32_t v1 = segment + i;
                uint32_t v2 = segment + thetaSegments + i;
                uint32_t v3 = segment + thetaSegments + 1 + i;
                
                ringGeom.faces.emplace_back(v1, v2, v3); // front
                //ringGeom.texCoords.push_back(uvs[v1], uvs[v2], uvs[v3]);
                
                v1 = segment + i;
                v2 = segment + thetaSegments + 1 + i;
                v3 = segment + 1 + i;
                
                ringGeom.faces.emplace_back(v1, v2, v3);
                //ringGeom.texCoords.push_back(uvs[v1], uvs[v2], uvs[v3]);
            }
            
        }
        
        compute_normals(ringGeom);
        compute_tangents(ringGeom);
        
        return ringGeom;
    }
    
    inline geometry make_3d_ring(float innerRadius = 1.0f, float outerRadius = 2.0f, float length = 1.0f)
    {
        geometry ringGeom;
        
        uint32_t rs = 24; // radial segments
        uint32_t rs2 = rs * 2;
        
        // Inner Ring
        for (uint32_t i = 0; i < rs2; i++)
        {
            float angle = i * POLYMER_TAU / rs;
            float x = innerRadius * cos(angle);
            float y = innerRadius * sin(angle);
            float z = (i < rs) ? -(length * 0.5f) : (length * 0.5f);
            ringGeom.vertices.emplace_back(x, y, z);
        }
        
        for (uint32_t i = 0; i < rs; i++)
        {
            uint4 q = uint4(i, i + rs, (i + 1) % rs + rs, (i + 1) % rs);
            ringGeom.faces.push_back({q.x,q.y,q.z}); // faces point in
            ringGeom.faces.push_back({q.x,q.z,q.w});
        }
        
        // Outer Ring
        for (uint32_t i = 0; i < rs2; i++)
        {
            float angle = i * POLYMER_TAU / rs;
            float x = outerRadius * cos(angle);
            float y = outerRadius * sin(angle);
            float z = (i < rs)?  -(length * 0.5f) : (length * 0.5f);
            ringGeom.vertices.emplace_back(x, y, z);
        }
        
        for (uint32_t i = 0; i < rs; i++)
        {
            uint32_t b = (uint32_t) ringGeom.vertices.size() / 2;
            uint4 q = uint4(b + i, (b + i) + rs, ((b + i) + 1) % rs + (3 * rs), ((b + i) + 1) % rs + (rs * 2));
            ringGeom.faces.push_back({q.w,q.z,q.x}); // faces point out
            ringGeom.faces.push_back({q.z,q.y,q.x});
        }
        
        // Top + Bottom
        for (uint32_t i = 0; i < rs; i++)
        {
            uint32_t x = i + rs;
            uint4 q = uint4(i, (i) % rs + (2 * rs), (i + 1) % rs + (2 * rs), (i + 1) % rs); // -Z end
            uint4 q2 = uint4(x, (x) % (2 * rs) + (2 * rs), (i + 1) % rs + (3 * rs), ((i + 1) % rs) + rs); // +Z end
            ringGeom.faces.push_back({q.w,q.z,q.x});
            ringGeom.faces.push_back({q.z,q.y,q.x});
            ringGeom.faces.push_back({q2.x,q2.y,q2.z});
            ringGeom.faces.push_back({q2.x,q2.z,q2.w});
        }
        
        compute_normals(ringGeom);
        
        return ringGeom;
    }
    
    inline geometry make_frustum(float aspectRatio = 1.33333f)
    {
        geometry frustum;
        
        float h = 1 / aspectRatio;
        
        frustum.vertices = {
            {+0, +0,   +0}, {-1, +h, -1.0}, 
            {+0, +0,   +0}, {+1, +h, -1.0}, 
            {+0, +0,   +0}, {-1, -h, -1.0},
            {+0, +0,   +0}, {+1, -h, -1.0}, 
            {-1, +h, -1.0}, {+1, +h, -1.0}, 
            {+1, +h, -1.0}, {+1, -h, -1.0}, 
            {+1, -h, -1.0}, {-1, -h, -1.0}, 
            {-1, -h, -1.0}, {-1, +h, -1.0}
        };
        return frustum;
    }
    
    inline geometry make_torus(uint32_t radial_segments = 24)
    {
        geometry torus;
        
        for (uint32_t i = 0; i <= radial_segments; ++i)
        {
            auto a = make_rotation_quat_axis_angle({0,1,0}, (i % radial_segments) * POLYMER_TAU / radial_segments);
            for (uint32_t j = 0; j <= radial_segments; ++j)
            {
                auto b = make_rotation_quat_axis_angle({0,0,1}, (j % radial_segments) * POLYMER_TAU / radial_segments);
                torus.vertices.push_back(qrot(a, qrot(b, float3(1,0,0)) + float3(3,0,0)));
                torus.texcoord0.push_back({i * 8.0f / radial_segments, j * 4.0f / radial_segments});
                if (i > 0 && j > 0)
                {
                    float4 q = {
                        float((i-1)* (radial_segments + 1)+(j-1)),
                        float(i*(radial_segments + 1) + (j-1)),
                        float(i*(radial_segments + 1) + j),
                        float((i-1)*(radial_segments+ 1) +j)
                    };
                    torus.faces.emplace_back(float3(q.x, q.y, q.z));
                    torus.faces.emplace_back(float3(q.x, q.z, q.w));
                }
            }
        }

        compute_normals(torus);
        compute_tangents(torus);

        return torus;
    }

    inline geometry make_capsule(int segments, float radius, float length)
    {
        geometry capsule;
        
        // Ensure odd
        segments = (segments + 1) &~1;
        
        int doubleSegments = segments * 2;
        float halfLength = length / 2;
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            float ty = halfLength + radius;
            capsule.vertices.emplace_back(0.f, ty, 0.f);
            capsule.normals.emplace_back(0.f, 1.f, 0.f);
            capsule.texcoord0.emplace_back((j+1) / float(segments), 0.f);
        }
        
        for (int i = 1; i < segments; ++i)
        {
            float r = sinf(i * POLYMER_PI / segments) * radius;
            float y = cosf(i * POLYMER_PI / segments);
            float ty = y * radius;
            
            if (i < segments/2)
                ty += halfLength;
            else
                ty -= halfLength;
            
            capsule.vertices.emplace_back(0.f, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0.f, y, -1.f)));
            capsule.texcoord0.emplace_back(0.f, i / float(segments));
            
            for (int j = 1; j < doubleSegments; ++j)
            {
                float x = sinf(j * POLYMER_TAU / doubleSegments);
                float z = -cosf(j * POLYMER_TAU / doubleSegments);
                float ty = y * radius;
                
                if (i < segments/2)
                    ty += halfLength;
                else
                    ty -= halfLength;
                
                capsule.vertices.emplace_back(x * r, ty , z * r);
                capsule.normals.push_back(safe_normalize(float3(x, y, z)));
                capsule.texcoord0.emplace_back(j / float(segments), i / float(segments));
            }
            
            capsule.vertices.emplace_back(0.f, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0.f, y, -1.f)));
            capsule.texcoord0.emplace_back(2.f, i / float(segments));
        }
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            float ty = -halfLength - radius;
            capsule.vertices.emplace_back(0.f, ty, 0.f);
            capsule.normals.push_back(safe_normalize(float3(0.f, -1.f, 0.f)));
            capsule.texcoord0.emplace_back((j+1) / float(segments), 1.f);
        }
        
        int v = 0;
        for (int j = 0; j < doubleSegments; ++j)
        {
            capsule.faces.emplace_back(v, v + doubleSegments + 1, v + doubleSegments);
            ++v;
        }
        
        for (int i = 1; i < segments - 1; ++i)
        {
            for(int j=0; j < doubleSegments; ++j)
            {
                capsule.faces.emplace_back(v, v + 1, v + doubleSegments + 2);
                capsule.faces.emplace_back(v, v + doubleSegments + 2,v + doubleSegments + 1);
                ++v;
            }
            ++v;
        }
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            capsule.faces.emplace_back(v, v + 1, v + doubleSegments + 1);
            ++v;
        }
        return capsule;
    }
    
    // A render_component quad with a configurable subvision. Constructed on the XY axis with normals facing -Z for front-faces.
    inline geometry make_plane(float width, float height, uint32_t widthVertices, uint32_t heightVertices, bool doubleSided = false)
    {
        geometry plane;
        uint32_t indexOffset = 0;
        
        float rw = 1.f / width;
        float rh = 1.f / height;
        float ow = width / widthVertices;
        float oh = height / heightVertices;
        
        float ou = ow * rw;
        float ov = oh * rh;

        for (float w = -width / 2.0; w < width / 2.0; w += ow)
        {
            for (float h = -height / 2.0; h < height / 2.0; h += oh)
            {
                float u = (w + width / 2.0) * rw;
                float v = (h + height / 2.0) * rh;
                
                plane.vertices.emplace_back(w, h + oh, 0.f);
                plane.vertices.emplace_back(w, h, 0.f);
                plane.vertices.emplace_back(w + ow, h, 0.f);
                plane.vertices.emplace_back(w + ow, h + oh, 0.f);
                
                plane.texcoord0.emplace_back(u, v + ov);
                plane.texcoord0.emplace_back(u, v);
                plane.texcoord0.emplace_back(u + ou, v);
                plane.texcoord0.emplace_back(u + ou, v + ov);

                plane.normals.emplace_back(0.f, 0.f, -1.f);
                plane.normals.emplace_back(0.f, 0.f, -1.f);
                plane.normals.emplace_back(0.f, 0.f, -1.f);
                plane.normals.emplace_back(0.f, 0.f, -1.f);

                plane.faces.push_back({ indexOffset + 2, indexOffset + 1, indexOffset + 0 });
                plane.faces.push_back({ indexOffset + 3, indexOffset + 2, indexOffset + 0 });

                indexOffset += 4;
            }
        }

        compute_tangents(plane);
        
        return plane;
    }
    
    inline geometry make_curved_plane()
    {
        geometry plane;
        
        auto curve = bezier_spline(float3(0.0f, 0.0f, 0.0f), float3(0.667f, 0.25f, 0.0f), float3(1.33f, 0.25f, 0.0f), float3(2.0f, 0.0f, 0.0f));
        
        const int numSegments = static_cast<int>(curve.num_steps);
        const int numSlices = numSegments + 1;
        const int numVerts = 2 * numSlices;
        
        plane.vertices.resize(numVerts);
        plane.normals.resize(numVerts);
        plane.texcoord0.resize(numVerts);
        
        for (int i = 0; i <= numSegments; i++)
        {
            float t = (float)i / (float)numSegments;
            float3 point = curve.evaluate(t);
           
            float3 tang = normalize(curve.derivative(t));
            float3 norm = float3(0.0f, 1.0f, 0.0f);
            float3 biNorm = cross(tang, norm);

            int index = i * 2; // Slice idx
            
            plane.vertices[index] = point + float3(0, 0, 1); //biNorm;
            plane.vertices[index + 1] = point - float3(0, 0, 1);// biNorm;
            
            plane.normals[index] = norm;
            plane.normals[index + 1] = norm;
            
            plane.texcoord0[index] = float2(t, 0.0f);
            plane.texcoord0[index + 1] = float2(t, 1.0f);
        }
        
        // Set up indices
        for (int i = 0; i < numSegments; i++)
        {
            int vIndex = i * 2; // Starting vertex index of this segment
            
            auto f1 = uint3(vIndex + 0, vIndex + 1, vIndex + 2);
            auto f2 = uint3(vIndex + 1, vIndex + 3, vIndex + 2);
            
            plane.faces.push_back(f1);
            plane.faces.push_back(f2);
        }
        
        compute_tangents(plane);
        
        return plane;
    }
    
    inline geometry make_axis()
    {
        geometry axis;
        
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(1.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 1.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 1.f);

        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);
        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);
        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);

        return axis;
    }

    inline geometry make_axis(const float3 & xAxis, const float3 & yAxis, const float3 & zAxis)
    {
        geometry axis;

        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(xAxis);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(yAxis);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(zAxis);

        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);
        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);
        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);

        return axis;
    }

    inline geometry make_spiral(float resolution = 512.0f, float freq = 128.f)
    {
        geometry spiral;
        float off = 1.0 / resolution;
        for (float i = 0.0; i < 1.0 + off; i += off)
        {
            float s = cos(i * 2.0 * POLYMER_PI + POLYMER_PI) * 0.5f + 0.5f;
            spiral.vertices.push_back({cosf(i * POLYMER_PI * freq) * s, i, sinf(i * POLYMER_PI * freq) * s});
        }
        return spiral;
    }

    inline geometry make_icosahedron()
    {
        geometry icosahedron;
        const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
        
        icosahedron.vertices = {
            {-1.0, t, 0.0}, {1.0, t, 0.0}, {-1.0, -t, 0.0}, {1.0, -t, 0.0},
            {0.0, -1.0, t}, {0.0, 1.0, t}, {0.0, -1.0, -t}, {0.0, 1.0, -t},
            {t, 0.0, -1.0}, {t, 0.0, 1.0}, {-t, 0.0, -1.0}, {-t, 0.0, 1.0}
        };
        
        icosahedron.faces = {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
        };
        
        compute_normals(icosahedron);

        return icosahedron;
    }
    
    inline geometry make_octohedron()
    {
        geometry octohedron;

        octohedron.vertices = {
            {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
            {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}
        };

        octohedron.faces = {{0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2}, {1, 2, 5}, {1, 5, 3}, {1, 3, 4}, {1, 4, 2}};

        compute_normals(octohedron);

        return octohedron;
    }
    
    inline geometry make_tetrahedron()
    {
        geometry tetrahedron;

        tetrahedron.vertices = {
            {1.0, 1.0, 1.0}, {-1.0, -1.0, 1.0},
            {-1.0, 1.0, -1.0}, {1.0, -1.0, -1.0}
        };

        tetrahedron.faces = {{2, 1, 0},{0, 3, 2},{1, 3, 0},{2, 3, 1}};

        compute_normals(tetrahedron);

        return tetrahedron;
    }

    inline geometry make_supershape_3d(const int segments, const float m, const float n1, const float n2, const float n3, const float a = 1.0, const float b = 1.0)
    {
        geometry shape;

        super_formula f1(m, n1, n2, n3, a, b);
        super_formula f2(m, n1, n2, n3, a, b);

        float theta = -POLYMER_PI;
        float lon_inc = POLYMER_TAU / segments;
        float lat_inc = POLYMER_PI / segments;

        // Longitude
        for (int i = 0; i < segments + 1; ++i)
        {
            const float r1 = f1(theta);
            float phi = -POLYMER_PI / 2.0f; // reset phi 

            // Latitude
            for (int j = 0; j < segments + 1; ++j)
            {
                const float r2 = f2(phi);
                const float radius = r1 * r2; // spherical product
                const float x = radius * cos(theta) * cos(phi);
                const float y = radius * sin(theta) * cos(phi);
                const float z = r2 * sin(phi);
                shape.vertices.emplace_back(x, y, z);
                phi += lat_inc;
            }

            theta += lon_inc;
        }

        std::vector<uint4> quads;
        int latIdx = 0;
        for (int i = 0; i < segments * (segments + 1); ++i)
        {
            if (latIdx < segments)
            {
                uint32_t a = i;
                uint32_t b = i + 1;
                uint32_t c = i + segments + 1 + 1;
                uint32_t d = i + segments + 1;
                quads.push_back(uint4(a, b, c, d));
                latIdx++;
            }
            else latIdx = 0;
        }

        for (auto & q : quads)
        {
            shape.faces.push_back({ q.w,q.z,q.x });
            shape.faces.push_back({ q.z,q.y,q.x });
        }

        compute_normals(shape, true);

        return shape;
    }

    inline geometry make_icosasphere(uint32_t subdivisions = 1)
    {
        geometry ico = make_icosahedron();

        for (uint32_t j = 0; j < subdivisions; ++j)
        {
            ico.vertices.reserve(ico.vertices.size() + ico.faces.size());
            ico.normals.reserve(ico.normals.size() + ico.faces.size());
            ico.faces.reserve(ico.faces.size() * 3);

            // For each triangle
            const size_t numTriangles = ico.faces.size();
            for (uint32_t i = 0; i < numTriangles; ++i)
            {
                uint32_t index0 = ico.faces[i].x;
                uint32_t index1 = ico.faces[i].y;
                uint32_t index2 = ico.faces[i].z;

                uint32_t index3 = (uint32_t) ico.vertices.size();
                uint32_t index4 = index3 + 1;
                uint32_t index5 = index4 + 1;

                ico.faces[i].y = index3;
                ico.faces[i].z = index5;

                ico.faces.push_back({ index3, index1, index4 });
                ico.faces.push_back({ index5, index3, index4 });
                ico.faces.push_back({ index5, index4, index2 });

                ico.vertices.push_back(0.5f * (ico.vertices[index0] + ico.vertices[index1]));
                ico.vertices.push_back(0.5f * (ico.vertices[index1] + ico.vertices[index2]));
                ico.vertices.push_back(0.5f * (ico.vertices[index2] + ico.vertices[index0]));

                ico.normals.push_back(0.5f * (ico.normals[index0] + ico.normals[index1]));
                ico.normals.push_back(0.5f * (ico.normals[index1] + ico.normals[index2]));
                ico.normals.push_back(0.5f * (ico.normals[index2] + ico.normals[index0]));
            }
        }

        for (auto & v : ico.vertices) v = normalize(v);
        for (auto & n : ico.normals) n = normalize(n);

        ico.texcoord0.resize(ico.normals.size(), float2());
        for (size_t i = 0; i < ico.normals.size(); ++i)
        {
            const float3 & normal = ico.normals[i];
            ico.texcoord0[i].x = 0.5f - 0.5f * std::atan2(normal.x, -normal.z) / float(POLYMER_PI);
            ico.texcoord0[i].y = 1.0f - std::acos(normal.y) / float(POLYMER_PI);
        }

        auto add_unique_vertex = [&](size_t i, uint32_t component, const float2 & uv) 
        {
            uint3 & face = ico.faces[i];
            const uint32_t index = face[component];

            face[component] = (uint32_t)ico.vertices.size();

            ico.vertices.push_back(ico.vertices[index]);
            ico.normals.push_back(ico.normals[index]);
            ico.texcoord0.push_back(uv);
        };

        const size_t numTriangles = ico.faces.size();
        for (size_t i = 0; i < numTriangles; ++i)
        {
            const float2 & uv0 = ico.texcoord0[ico.faces[i].x];
            const float2 & uv1 = ico.texcoord0[ico.faces[i].y];
            const float2 & uv2 = ico.texcoord0[ico.faces[i].z];

            const float d1 = uv1.x - uv0.x;
            const float d2 = uv2.x - uv0.x;

            if (std::abs(d1) > 0.5f && std::abs(d2) > 0.5f)  add_unique_vertex(i, 0, uv0 + float2((d1 > 0.0f) ? 1.0f : -1.0f, 0.0f));
            else if (std::abs(d1) > 0.5f) add_unique_vertex(i, 1, uv1 + float2((d1 < 0.0f) ? 1.0f : -1.0f, 0.0f));
            else if (std::abs(d2) > 0.5f) add_unique_vertex(i, 2, uv2 + float2((d2 < 0.0f) ? 1.0f : -1.0f, 0.0f));
        }

        compute_tangents(ico);

        return ico;
    }
    
} // end namespace polymer

#pragma warning(pop)

#endif // procedural_mesh_h

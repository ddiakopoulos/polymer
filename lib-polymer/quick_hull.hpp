/*
 * Implementation of the 3d QuickHull algorithm originally by Antti Kuukka
 * Input: a list of points in 3d space (for example, vertices of a 3d mesh)
 * Output: a ConvexHull object which provides vertex and index buffers of the generated convex hull as a triangle mesh.
 * License: This is free and unencumbered software released into the public domain.
 * Original source: https://github.com/akuukka/quickhull
 * References: 
 * [1] http://box2d.org/files/GDC2014/DirkGregorius_ImplementingQuickHull.pdf
 * [2] http://thomasdiewald.com/blog/?p=1888
 * [3] https://fgiesen.wordpress.com/2012/02/21/half-edge-based-mesh-representations-theory/
 */

#ifndef polymer_quickhull_hpp
#define polymer_quickhull_hpp

#include "util.hpp"
#include "math-core.hpp"
#include <memory>
#include <unordered_map>
#include <array>
#include <assert.h>
#include <deque>

namespace quickhull 
{
    ////////////////////////
    //   Math Utilities   //
    ////////////////////////

    inline float getSquaredDistanceBetweenPointAndRay(const float3 & p, const ray & r) 
    {
        const float3 s = p - r.origin;
        float t = dot(s, r.direction);
        return length2(s) - t * t * (1.f / length2(r.direction));
    }

    inline float getSignedDistanceToPlane(const float3 & v, const plane & p) { return dot(p.get_normal(),v) + p.get_distance(); }
    inline float3 getTriangleNormal(const float3 & a, const float3 & b, const float3 & c) { return safe_normalize(cross(b - a, c - a)); }

    //////////////////
    //   The Pool   //
    //////////////////

    template<typename T>
    class Pool 
    {
        std::vector<std::unique_ptr<T>> data;
    public:
        void clear() { data.clear(); }
        void reclaim(std::unique_ptr<T> & ptr) { data.push_back(std::move(ptr)); }
        std::unique_ptr<T> get() 
        {
            if (data.size() == 0) return std::unique_ptr<T>(new T());
            auto it = data.end() - 1;
            std::unique_ptr<T> r = std::move(*it);
            data.erase(it);
            return r;
        }
    };

    //////////////////////
    //   Mesh Builder   //
    //////////////////////

    struct MeshBuilder 
    {
        struct HalfEdge 
        {
            size_t m_endVertex;
            size_t m_opp;
            size_t m_face;
            size_t m_next;
            HalfEdge() {};
            HalfEdge(size_t end, size_t opp, size_t face, size_t next) : m_endVertex(end), m_opp(opp), m_face(face), m_next(next) {}
            void disable() { m_endVertex = std::numeric_limits<size_t>::max(); }
            bool isDisabled() const { return m_endVertex == std::numeric_limits<size_t>::max(); }
        };

        struct Face 
        {
            size_t m_he;
            plane m_P;
            float m_mostDistantPointDist{ 0 };
            size_t m_mostDistantPoint{ 0 };
            size_t m_visibilityCheckedOnIteration{ 0 };
            std::uint8_t m_isVisibleFaceOnCurrentIteration : 1;
            std::uint8_t m_inFaceStack : 1;
            std::uint8_t m_horizonEdgesOnCurrentIteration : 3; // Bit for each half edge assigned to this face, each being 0 or 1 depending on whether the edge belongs to horizon edge
            std::unique_ptr<std::vector<size_t>> m_pointsOnPositiveSide;

            Face() : m_he(std::numeric_limits<size_t>::max()),
                     m_isVisibleFaceOnCurrentIteration(0),
                     m_inFaceStack(0),
                     m_horizonEdgesOnCurrentIteration(0) {}

            void disable() { m_he = std::numeric_limits<size_t>::max(); }
            bool isDisabled() const { return m_he == std::numeric_limits<size_t>::max(); }
        };

        // Mesh data
        std::vector<Face> m_faces;
        std::vector<HalfEdge> m_halfEdges;
        
        // When the mesh is modified and faces and half edges are removed from it, we do not actually remove them from the container vectors.
        // Insted, they are marked as disabled which means that the indices can be reused when we need to add new faces and half edges to the mesh.
        // We store the free indices in the following vectors.
        std::vector<size_t> m_disabledFaces,m_disabledHalfEdges;
        
        size_t addFace() 
        {
            if (m_disabledFaces.size()) 
            {
                size_t index = m_disabledFaces.back();
                auto & f = m_faces[index];
                assert(f.isDisabled());
                assert(!f.m_pointsOnPositiveSide);
                f.m_mostDistantPointDist = 0;
                m_disabledFaces.pop_back();
                return index;
            }
            m_faces.emplace_back();
            return m_faces.size()-1;
        }

        size_t addHalfEdge() 
        {
            if (m_disabledHalfEdges.size()) 
            {
                const size_t index = m_disabledHalfEdges.back();
                m_disabledHalfEdges.pop_back();
                return index;
            }
            m_halfEdges.emplace_back();
            return m_halfEdges.size() - 1;
        }

        // Mark a face as disabled and return a pointer to the points that were on the positive of it.
        std::unique_ptr<std::vector<size_t>> disableFace(size_t faceIndex) 
        {
            auto & f = m_faces[faceIndex];
            f.disable();
            m_disabledFaces.push_back(faceIndex);
            return std::move(f.m_pointsOnPositiveSide);
        }

        void disableHalfEdge(size_t heIndex) 
        {
            auto & he = m_halfEdges[heIndex];
            he.disable();
            m_disabledHalfEdges.push_back(heIndex);
        }

        MeshBuilder() = default;
        
        // Create a mesh with initial tetrahedron ABCD. Dot product of AB with the normal of triangle ABC should be negative.
        MeshBuilder(size_t a, size_t b, size_t c, size_t d) 
        {
            // Create halfedges
            m_halfEdges.emplace_back(b, 6, 0, 1);     // ab
            m_halfEdges.emplace_back(c, 9, 0, 2);     // bc
            m_halfEdges.emplace_back(a, 3, 0, 0);     // ca
            m_halfEdges.emplace_back(c, 2, 1, 4);     // ac
            m_halfEdges.emplace_back(d, 11, 1, 5);    // cd
            m_halfEdges.emplace_back(a, 7, 1, 3);     // da
            m_halfEdges.emplace_back(a, 0, 2, 7);     // ba
            m_halfEdges.emplace_back(d, 5, 2, 8);     // ad
            m_halfEdges.emplace_back(b, 10, 2, 6);    // db
            m_halfEdges.emplace_back(b, 1, 3, 10);    // cb
            m_halfEdges.emplace_back(d, 8, 3, 11);    // bd
            m_halfEdges.emplace_back(c, 4, 3, 9);     // dc

            // Create faces
            Face ABC, ACD, BAD, CBD;
            ABC.m_he = 0;
            m_faces.push_back(std::move(ABC));
            ACD.m_he = 3;
            m_faces.push_back(std::move(ACD));
            BAD.m_he = 6;
            m_faces.push_back(std::move(BAD));
            CBD.m_he = 9;
            m_faces.push_back(std::move(CBD));
        }

        std::array<size_t, 3> getVertexIndicesOfFace(const Face & f) const 
        {
            std::array<size_t, 3> v;
            const HalfEdge * he = &m_halfEdges[f.m_he];
            v[0] = he->m_endVertex;
            he = &m_halfEdges[he->m_next];
            v[1] = he->m_endVertex;
            he = &m_halfEdges[he->m_next];
            v[2] = he->m_endVertex;
            return v;
        }

        std::array<size_t, 2> getVertexIndicesOfHalfEdge(const HalfEdge & he) const 
        { 
            return {m_halfEdges[he.m_opp].m_endVertex, he.m_endVertex};
        }

        std::array<size_t,3> getHalfEdgeIndicesOfFace(const Face & f) const 
        {
            return {f.m_he, m_halfEdges[f.m_he].m_next, m_halfEdges[m_halfEdges[f.m_he].m_next].m_next};
        }
    };

    ////////////////////////
    //   Half Edge Mesh   //
    ////////////////////////

    struct HalfEdgeMesh 
    {
        struct HalfEdge { size_t m_endVertex, m_opp, m_face, m_next; };
        struct Face { size_t m_halfEdgeIndex; }; // Index of one of the half edges of this face
        
        std::vector<float3> m_vertices;
        std::vector<Face> m_faces;
        std::vector<HalfEdge> m_halfEdges;
        
        HalfEdgeMesh(const MeshBuilder & builderObject, const std::vector<float3> & vertexData )
        {
            std::unordered_map<size_t, size_t> faceMapping, halfEdgeMapping, vertexMapping;
            
            size_t i = 0;

            for (const auto & face : builderObject.m_faces) 
            {
                if (!face.isDisabled()) 
                {
                    m_faces.push_back({static_cast<size_t>(face.m_he)});
                    faceMapping[i] = m_faces.size()-1;
                    
                    const auto heIndices = builderObject.getHalfEdgeIndicesOfFace(face);
                    for (const auto heIndex : heIndices) 
                    {
                        const size_t vertexIndex = builderObject.m_halfEdges[heIndex].m_endVertex;
                        if (vertexMapping.count(vertexIndex)==0) 
                        {
                            m_vertices.push_back(vertexData[vertexIndex]);
                            vertexMapping[vertexIndex] = m_vertices.size()-1;
                        }
                    }
                }
                i++;
            }
            
            i = 0;

            for (const auto & halfEdge : builderObject.m_halfEdges) 
            {
                if (!halfEdge.isDisabled()) 
                {
                    m_halfEdges.push_back({
                        static_cast<size_t>(halfEdge.m_endVertex),
                        static_cast<size_t>(halfEdge.m_opp),
                        static_cast<size_t>(halfEdge.m_face),
                        static_cast<size_t>(halfEdge.m_next)});

                    halfEdgeMapping[i] = m_halfEdges.size()-1;
                }
                i++;
            }
            
            for (auto & face : m_faces) 
            {
                assert(halfEdgeMapping.count(face.m_halfEdgeIndex) == 1);
                face.m_halfEdgeIndex = halfEdgeMapping[face.m_halfEdgeIndex];
            }
            
            for (auto & he : m_halfEdges) 
            {
                he.m_face = faceMapping[he.m_face];
                he.m_opp = halfEdgeMapping[he.m_opp];
                he.m_next = halfEdgeMapping[he.m_next];
                he.m_endVertex = vertexMapping[he.m_endVertex];
            }
        }  
    };

    /////////////////////
    //   Convex Hull   //
    /////////////////////

    class ConvexHull 
    {
        std::unique_ptr<std::vector<float3>> m_optimizedVertexBuffer;
        std::vector<float3> & m_vertices;
        std::vector<size_t> m_indices;

    public:

        // Construct vertex and index buffers from half edge mesh and pointcloud
        ConvexHull(const MeshBuilder & mesh, std::vector<float3> & pointCloud, bool CCW, bool useOriginalIndices) : m_vertices(pointCloud)
        {
            if (!useOriginalIndices) 
            {
                m_optimizedVertexBuffer.reset(new std::vector<float3>());
            }
            
            std::vector<bool> faceProcessed(mesh.m_faces.size(),false);
            std::vector<size_t> faceStack;

            // Map vertex indices from original point cloud to the new mesh vertex indices
            std::unordered_map<size_t,size_t> vertexIndexMapping; 
            for (size_t i = 0;i<mesh.m_faces.size();i++) 
            {
                if (!mesh.m_faces[i].isDisabled()) 
                {
                    faceStack.push_back(i);
                    break;
                }
            }

            if (faceStack.size() == 0) return;

            const size_t finalMeshFaceCount = mesh.m_faces.size() - mesh.m_disabledFaces.size();
            m_indices.reserve(finalMeshFaceCount * 3);

            while (faceStack.size()) 
            {
                auto it = faceStack.end()-1;
                size_t top = *it;
                assert(!mesh.m_faces[top].isDisabled());
                faceStack.erase(it);

                if (faceProcessed[top]) 
                {
                    continue; 
                }
                else 
                {
                    faceProcessed[top]=true;

                    auto halfEdges = mesh.getHalfEdgeIndicesOfFace(mesh.m_faces[top]);

                    size_t adjacent[] = {
                        mesh.m_halfEdges[mesh.m_halfEdges[halfEdges[0]].m_opp].m_face,
                        mesh.m_halfEdges[mesh.m_halfEdges[halfEdges[1]].m_opp].m_face,
                        mesh.m_halfEdges[mesh.m_halfEdges[halfEdges[2]].m_opp].m_face
                    };

                    for (auto a : adjacent) 
                    {
                        if (!faceProcessed[a] && !mesh.m_faces[a].isDisabled()) faceStack.push_back(a);
                    }

                    auto vertices = mesh.getVertexIndicesOfFace(mesh.m_faces[top]);

                    if (!useOriginalIndices) 
                    {
                        for (auto & v : vertices) 
                        {
                            auto it = vertexIndexMapping.find(v);

                            if (it == vertexIndexMapping.end()) 
                            {
                                m_optimizedVertexBuffer->push_back(pointCloud[v]);
                                vertexIndexMapping[v] = m_optimizedVertexBuffer->size()-1;
                                v = m_optimizedVertexBuffer->size()-1;
                            }
                            else v = it->second;
                        }
                    }

                    m_indices.push_back(vertices[0]);

                    if (CCW) 
                    {
                        m_indices.push_back(vertices[2]);
                        m_indices.push_back(vertices[1]);
                    }
                    else 
                    {
                        m_indices.push_back(vertices[1]);
                        m_indices.push_back(vertices[2]);
                    }
                }
            }
            
            if (!useOriginalIndices) m_vertices = *m_optimizedVertexBuffer;
            else m_vertices = pointCloud;
        }

        std::vector<size_t> & getIndexBuffer() { return m_indices; }
        std::vector<float3> & getVertexBuffer() { return m_vertices; }
    };

    class QuickHull 
    {
        const float Epsilon{ 0.0001f };

        float m_epsilon, m_epsilonSquared, m_scale;
        bool m_planar;

        std::vector<float3> & m_vertexData;

        std::vector<float3> m_planarPointCloudTemp;
        MeshBuilder m_mesh;

        std::array<size_t, 6> m_extremeValues;

        size_t m_failedHorizonEdges{ 0 };

        // Temporary variables used during iteration process
        std::vector<size_t> m_newFaceIndices;
        std::vector<size_t> m_newHalfEdgeIndices;
        std::vector< std::unique_ptr<std::vector<size_t>> > m_disabledFacePointVectors;

        // Create a half edge mesh representing the base tetrahedron from which the QuickHull iteration proceeds. m_extremeValues must be properly set up when this is called.
        MeshBuilder getInitialTetrahedron()
        {
            const size_t vertexCount = m_vertexData.size();
            
            // If we have at most 4 points, just return a degenerate tetrahedron:
            if (vertexCount <= 4) 
            {
                size_t v[4] = {0, std::min((size_t)1,vertexCount-1), std::min((size_t)2,vertexCount-1), std::min((size_t)3,vertexCount-1)};

                const float3 N = getTriangleNormal(m_vertexData[v[0]],m_vertexData[v[1]],m_vertexData[v[2]]);

                const plane trianglePlane(N,m_vertexData[v[0]]);

                if (trianglePlane.is_positive_half_space(m_vertexData[v[3]])) 
                {
                    std::swap(v[0],v[1]);
                }
                return MeshBuilder(v[0],v[1],v[2],v[3]);
            }
            
            // Find two most distant extreme points.
            float maxD = m_epsilonSquared;
            std::pair<size_t, size_t> selectedPoints;
            for (size_t i=0;i<6;i++) 
            {
                for (size_t j=i+1;j<6;j++) 
                {
                    const float d = distance2(m_vertexData[m_extremeValues[i]], m_vertexData[m_extremeValues[j]]);
                    if (d > maxD) 
                    {
                        maxD=d;
                        selectedPoints={m_extremeValues[i],m_extremeValues[j]};
                    }
                }
            }

            // A degenerate case: the point cloud seems to consists of a single point
            if (maxD == m_epsilonSquared) 
            {
                return MeshBuilder(0,std::min((size_t)1,vertexCount-1), std::min((size_t)2,vertexCount-1),std::min((size_t)3,vertexCount-1));
            }

            assert(selectedPoints.first != selectedPoints.second);
            
            // Find the most distant point to the line between the two chosen extreme points.
            const ray r(m_vertexData[selectedPoints.first], (m_vertexData[selectedPoints.second] - m_vertexData[selectedPoints.first]));
            maxD = m_epsilonSquared;
            size_t maxI = std::numeric_limits<size_t>::max();
            const size_t vCount = m_vertexData.size();

            for (size_t i = 0; i < vCount; i++) 
            {
                const float distToRay = getSquaredDistanceBetweenPointAndRay(m_vertexData[i], r);
                if (distToRay > maxD) 
                {
                    maxD = distToRay;
                    maxI = i;
                }
            }

            if (maxD == m_epsilonSquared) 
            {
                // It appears that the point cloud belongs to a 1 dimensional subspace of R^3: convex hull has no volume => return a thin triangle
                // Pick any point other than selectedPoints.first and selectedPoints.second as the third point of the triangle
                auto it = std::find_if(m_vertexData.begin(),m_vertexData.end(),[&](const float3 & ve) 
                {
                    return ve != m_vertexData[selectedPoints.first] && ve != m_vertexData[selectedPoints.second];
                });

                const size_t thirdPoint = (it == m_vertexData.end()) ? selectedPoints.first : std::distance(m_vertexData.begin(),it);

                it = std::find_if(m_vertexData.begin(),m_vertexData.end(),[&](const float3 & ve) 
                {
                    return ve != m_vertexData[selectedPoints.first] && ve != m_vertexData[selectedPoints.second] && ve != m_vertexData[thirdPoint];
                });

                const size_t fourthPoint = (it == m_vertexData.end()) ? selectedPoints.first : std::distance(m_vertexData.begin(),it);

                return MeshBuilder(selectedPoints.first,selectedPoints.second,thirdPoint,fourthPoint);
            }

            // These three points form the base triangle for our tetrahedron.
            assert(selectedPoints.first != maxI && selectedPoints.second != maxI);

            std::array<size_t,3> baseTriangle{selectedPoints.first, selectedPoints.second, maxI};
            const float3 baseTriangleVertices[]={ m_vertexData[baseTriangle[0]], m_vertexData[baseTriangle[1]],  m_vertexData[baseTriangle[2]] };
            
            // Next step is to find the 4th vertex of the tetrahedron. We naturally choose the point farthest away from the triangle plane.
            maxD=m_epsilon;
            maxI=0;
            const float3 N = getTriangleNormal(baseTriangleVertices[0],baseTriangleVertices[1],baseTriangleVertices[2]);
            plane trianglePlane(N,baseTriangleVertices[0]);

            for (size_t i = 0; i < vCount; i++) 
            {
                const float d = std::abs(getSignedDistanceToPlane(m_vertexData[i],trianglePlane));
                if (d > maxD) 
                {
                    maxD=d;
                    maxI=i;
                }
            }

            if (maxD == m_epsilon) 
            {
                // All the points seem to lie on a 2D subspace of R^3. How to handle this? Well, let's add one extra point to the point 
                // cloud so that the convex hull will have volume.
                m_planar = true;

                const float3 N = getTriangleNormal(baseTriangleVertices[1],baseTriangleVertices[2],baseTriangleVertices[0]);
                m_planarPointCloudTemp.clear();
                m_planarPointCloudTemp.insert(m_planarPointCloudTemp.begin(),m_vertexData.begin(),m_vertexData.end());

                const float3 extraPoint = N + m_vertexData[0];
                m_planarPointCloudTemp.push_back(extraPoint);
                maxI = m_planarPointCloudTemp.size() - 1;

                m_vertexData = m_planarPointCloudTemp;
            }

            // Enforce CCW orientation (if user prefers clockwise orientation, swap two vertices in each triangle when final mesh is created)
            const plane triPlane(N,baseTriangleVertices[0]);
            if (triPlane.is_positive_half_space(m_vertexData[maxI])) 
            {
                std::swap(baseTriangle[0],baseTriangle[1]);
            }

            // Create a tetrahedron half edge mesh and compute planes defined by each triangle
            MeshBuilder mesh(baseTriangle[0],baseTriangle[1],baseTriangle[2],maxI);
            for (auto & f : mesh.m_faces) 
            {
                auto v = mesh.getVertexIndicesOfFace(f);
                const float3 & va = m_vertexData[v[0]];
                const float3 & vb = m_vertexData[v[1]];
                const float3 & vc = m_vertexData[v[2]];
                const float3 N = getTriangleNormal(va, vb, vc);
                const plane trianglePlane(N,va);
                f.m_P = trianglePlane;
            }

            // Finally we assign a face for each vertex outside the tetrahedron (vertices inside the tetrahedron have no role anymore)
            for (size_t i=0;i<vCount;i++) 
            {
                for (auto & face : mesh.m_faces) 
                {
                    if (addPointToFace(face, i)) 
                    {
                        break;
                    }
                }
            }

            return mesh;
        }

        // Given a list of half edges, try to rearrange them so that they form a loop. Return true on success.
        bool reorderHorizonEdges(std::vector<size_t> & horizonEdges)
        {
            const size_t horizonEdgeCount = horizonEdges.size();

            for (size_t i=0;i<horizonEdgeCount-1;i++) 
            {
                const size_t endVertex = m_mesh.m_halfEdges[horizonEdges[i]].m_endVertex;
                bool foundNext = false;
                for (size_t j=i+1;j<horizonEdgeCount;j++) 
                {
                    const size_t beginVertex = m_mesh.m_halfEdges[m_mesh.m_halfEdges[horizonEdges[j]].m_opp].m_endVertex;
                    if (beginVertex == endVertex) 
                    {
                        std::swap(horizonEdges[i+1],horizonEdges[j]);
                        foundNext = true;
                        break;
                    }
                }
                if (!foundNext) return false;
            }

            assert(m_mesh.m_halfEdges[horizonEdges[horizonEdges.size()-1]].m_endVertex == m_mesh.m_halfEdges[m_mesh.m_halfEdges[horizonEdges[0]].m_opp].m_endVertex);

            return true;
        }
        
        // Find indices of extreme values (max x, min x, max y, min y, max z, min z) for the given point cloud
        std::array<size_t,6> getExtremeValues()
        {
            std::array<size_t,6> outIndices{0,0,0,0,0,0};

            float extremeVals[6] = {m_vertexData[0].x,m_vertexData[0].x,m_vertexData[0].y,m_vertexData[0].y,m_vertexData[0].z,m_vertexData[0].z};

            const size_t vCount = m_vertexData.size();

            auto set = [&extremeVals, &outIndices](const int & i, const float & v, const size_t & idx)
            {
                extremeVals[i] = v;
                outIndices[i] = idx;
            };

            for (size_t i = 1; i < vCount; i++) 
            {
                const float3 & pos = m_vertexData[i];

                if (pos.x > extremeVals[0]) set(0, pos.x, i);
                else if (pos.x<extremeVals[1]) set(1, pos.x, i);

                if (pos.y>extremeVals[2]) set(2, pos.y, i);
                else if (pos.y<extremeVals[3]) set(3, pos.y, i);

                if (pos.z>extremeVals[4]) set(4, pos.z, i);
                else if (pos.z<extremeVals[5]) set(5, pos.z, i);
            }

            return outIndices;       
        }
        
        // Compute scale of the vertex data.
        float getScale(const std::array<size_t, 6> & extremeValues)
        {
            float s = 0;
            for (size_t i = 0; i < 6; i++) 
            {
                const float * v = (const float *)(&m_vertexData[extremeValues[i]]);
                v += i/2;
                auto a = std::abs(*v);
                if (a > s) s = a;
            }
            return s;
        }
        
        // Each face contains a unique pointer to a vector of indices. However, many - often most - faces do not have any points on the positive
        // side of them especially at the the end of the iteration. When a face is removed from the mesh, its associated point vector, if such
        // exists, is moved to the index vector pool, and when we need to add new faces with points on the positive side to the mesh,
        // we reuse these vectors. This reduces the amount of std::vectors we have to deal with, and impact on performance is remarkable.
        Pool<std::vector<size_t>> m_indexVectorPool;

        std::unique_ptr<std::vector<size_t>> getIndexVectorFromPool()
        {
            auto r = std::move(m_indexVectorPool.get());
            r->clear();
            return r;
        }

        // Reduce memory usage! Huge vectors are needed at the beginning of iteration when faces have 
        // many points on their positive side. Later on, smaller vectors will suffice.
        void reclaimToIndexVectorPool(std::unique_ptr<std::vector<size_t>> & ptr)
        {
            const size_t oldSize = ptr->size();
            if ((oldSize + 1) * 128 < ptr->capacity())
            {
                ptr.reset(nullptr);
                return;
            }
            m_indexVectorPool.reclaim(ptr);
        }
        
        // Associates a point with a face if the point resides on the positive side of the plane. Returns true if the points was on the positive side.
        bool addPointToFace(typename MeshBuilder::Face& f, size_t pointIndex)
        {
            const float D = getSignedDistanceToPlane(m_vertexData[pointIndex], f.m_P);
            if (D > 0 && D*D > m_epsilonSquared * length2(f.m_P.get_normal()))
            {
                if (!f.m_pointsOnPositiveSide) f.m_pointsOnPositiveSide = std::move(getIndexVectorFromPool());
                f.m_pointsOnPositiveSide->push_back(pointIndex);
                if (D > f.m_mostDistantPointDist)
                {
                    f.m_mostDistantPointDist = D;
                    f.m_mostDistantPoint = pointIndex;
                }
                return true;
            }
            return false;
        }
        
        // This will update m_mesh from which we create the ConvexHull object that getConvexHull function returns
        void createConvexHalfEdgeMesh()
        {
            // Temporary variables used during iteration
            std::vector<size_t> visibleFaces;
            std::vector<size_t> horizonEdges;

            struct FaceData 
            {
                size_t m_faceIndex;
                size_t m_enteredFromHalfEdge; // If the face turns out not to be visible, this half edge will be marked as horizon edge
                FaceData(size_t fi, size_t he) : m_faceIndex(fi), m_enteredFromHalfEdge(he) {}
            };
            std::vector<FaceData> possiblyVisibleFaces;

            m_mesh = getInitialTetrahedron(); // Compute base tetrahedron
            assert(m_mesh.m_faces.size() == 4);

            // Init face stack with those faces that have points assigned to them
            std::deque<size_t> faceList;
            for (size_t i=0;i < 4;i++) 
            {
                auto & f = m_mesh.m_faces[i];
                if (f.m_pointsOnPositiveSide && f.m_pointsOnPositiveSide->size()>0) 
                {
                    faceList.push_back(i);
                    f.m_inFaceStack = 1;
                }
            }

            // Process faces until the face list is empty.
            size_t iter = 0;
            while (!faceList.empty()) 
            {
                iter++;
                if (iter == std::numeric_limits<size_t>::max()) 
                {
                    // Visible face traversal marks visited faces with iteration counter (to mark that the face has been visited on this iteration) 
                    // and the max value represents unvisited faces. At this point we have to reset iteration counter. This shouldn't be an issue on 64 bit machines.
                    iter = 0;
                }
                
                const size_t topFaceIndex = faceList.front();
                faceList.pop_front();
                
                auto & tf = m_mesh.m_faces[topFaceIndex];
                tf.m_inFaceStack = 0;

                assert(!tf.m_pointsOnPositiveSide || tf.m_pointsOnPositiveSide->size() > 0);
                if (!tf.m_pointsOnPositiveSide || tf.isDisabled()) continue;
                
                // Pick the most distant point to this triangle plane as the point to which we extrude
                const float3 & activePoint = m_vertexData[tf.m_mostDistantPoint];
                const size_t activePointIndex = tf.m_mostDistantPoint;

                // Find out the faces that have our active point on their positive side (these are the "visible faces"). The face on top of the stack of course is one of them. At the same time, we create a list of horizon edges.
                horizonEdges.clear();
                possiblyVisibleFaces.clear();
                visibleFaces.clear();

                possiblyVisibleFaces.emplace_back(topFaceIndex, std::numeric_limits<size_t>::max());

                while (possiblyVisibleFaces.size()) 
                {
                    const auto & faceData = possiblyVisibleFaces.back();
                    possiblyVisibleFaces.pop_back();

                    auto & pvf = m_mesh.m_faces[faceData.m_faceIndex];
                    assert(!pvf.isDisabled());
                    
                    if (pvf.m_visibilityCheckedOnIteration == iter) 
                    {
                        if (pvf.m_isVisibleFaceOnCurrentIteration) 
                        {
                            continue;
                        }
                    }
                    else 
                    {
                        const plane & P = pvf.m_P;
                        pvf.m_visibilityCheckedOnIteration = iter;
                        const float d = dot(P.get_normal(), activePoint) + P.get_distance();

                        if (d > 0) 
                        {
                            pvf.m_isVisibleFaceOnCurrentIteration = 1;
                            pvf.m_horizonEdgesOnCurrentIteration = 0;
                            visibleFaces.push_back(faceData.m_faceIndex);
                            for (auto heIndex : m_mesh.getHalfEdgeIndicesOfFace(pvf)) 
                            {
                                if (m_mesh.m_halfEdges[heIndex].m_opp != faceData.m_enteredFromHalfEdge) 
                                {
                                    possiblyVisibleFaces.emplace_back( m_mesh.m_halfEdges[m_mesh.m_halfEdges[heIndex].m_opp].m_face,heIndex );
                                }
                            }
                            continue;
                        }

                        assert(faceData.m_faceIndex != topFaceIndex);
                    }

                    // The face is not visible. Therefore, the halfedge we came from is part of the horizon edge.
                    pvf.m_isVisibleFaceOnCurrentIteration = 0;

                    horizonEdges.push_back(faceData.m_enteredFromHalfEdge);

                    // Store which half edge is the horizon edge. The other half edges of the face will not be part of the final mesh so their data slots can by recycled.
                    const auto halfEdges = m_mesh.getHalfEdgeIndicesOfFace(m_mesh.m_faces[m_mesh.m_halfEdges[faceData.m_enteredFromHalfEdge].m_face]);

                    const std::int8_t ind = (halfEdges[0]==faceData.m_enteredFromHalfEdge) ? 0 : (halfEdges[1]==faceData.m_enteredFromHalfEdge ? 1 : 2);

                    m_mesh.m_faces[m_mesh.m_halfEdges[faceData.m_enteredFromHalfEdge].m_face].m_horizonEdgesOnCurrentIteration |= (1<<ind);
                }

                const size_t horizonEdgeCount = horizonEdges.size();

                // Order horizon edges so that they form a loop. This may fail due to numerical instability in which case we give up trying to solve horizon edge for this point and accept a minor degeneration in the convex hull.
                if (!reorderHorizonEdges(horizonEdges)) 
                {
                    m_failedHorizonEdges++;

                    auto it = std::find(tf.m_pointsOnPositiveSide->begin(),tf.m_pointsOnPositiveSide->end(),activePointIndex);
                    tf.m_pointsOnPositiveSide->erase(it);

                    if (tf.m_pointsOnPositiveSide->size() == 0) 
                    {
                        reclaimToIndexVectorPool(tf.m_pointsOnPositiveSide);
                    }

                    continue;
                }

                // Except for the horizon edges, all half edges of the visible faces can be marked as disabled. Their data slots will be reused.
                // The faces will be disabled as well, but we need to remember the points that were on the positive side of them - therefore
                // we save pointers to them.
                m_newFaceIndices.clear();
                m_newHalfEdgeIndices.clear();
                m_disabledFacePointVectors.clear();

                size_t disableCounter = 0;

                for (auto faceIndex : visibleFaces) 
                {
                    auto & disabledFace = m_mesh.m_faces[faceIndex];

                    auto halfEdges = m_mesh.getHalfEdgeIndicesOfFace(disabledFace);

                    for (size_t j=0;j<3;j++) 
                    {
                        if ((disabledFace.m_horizonEdgesOnCurrentIteration & (1<<j)) == 0) 
                        {
                            if (disableCounter < horizonEdgeCount*2) 
                            {
                                // Use on this iteration
                                m_newHalfEdgeIndices.push_back(halfEdges[j]);
                                disableCounter++;
                            }
                            else m_mesh.disableHalfEdge(halfEdges[j]); // Mark for reusal on later iteration step
                        }
                    }

                    // Disable the face, but retain pointer to the points that were on the positive side of it. We need to assign those points
                    // to the new faces we create shortly.
                    auto t = std::move(m_mesh.disableFace(faceIndex));
                    if (t) 
                    {
                        assert(t->size()); // Because we should not assign point vectors to faces unless needed...
                        m_disabledFacePointVectors.push_back(std::move(t));
                    }
                }

                if (disableCounter < horizonEdgeCount * 2) 
                {
                    const size_t newHalfEdgesNeeded = horizonEdgeCount*2-disableCounter;
                    for (size_t i=0;i<newHalfEdgesNeeded;i++) 
                    {
                        m_newHalfEdgeIndices.push_back(m_mesh.addHalfEdge());
                    }
                }
                
                // Create new faces using the edgeloop
                for (size_t i = 0; i < horizonEdgeCount; i++) 
                {
                    const size_t AB = horizonEdges[i];

                    auto horizonEdgeVertexIndices = m_mesh.getVertexIndicesOfHalfEdge(m_mesh.m_halfEdges[AB]);

                    size_t A,B,C;
                    A = horizonEdgeVertexIndices[0];
                    B = horizonEdgeVertexIndices[1];
                    C = activePointIndex;

                    const size_t newFaceIndex = m_mesh.addFace();
                    m_newFaceIndices.push_back(newFaceIndex);

                    const size_t CA = m_newHalfEdgeIndices[2*i+0];
                    const size_t BC = m_newHalfEdgeIndices[2*i+1];

                    m_mesh.m_halfEdges[AB].m_next = BC;
                    m_mesh.m_halfEdges[BC].m_next = CA;
                    m_mesh.m_halfEdges[CA].m_next = AB;

                    m_mesh.m_halfEdges[BC].m_face = newFaceIndex;
                    m_mesh.m_halfEdges[CA].m_face = newFaceIndex;
                    m_mesh.m_halfEdges[AB].m_face = newFaceIndex;

                    m_mesh.m_halfEdges[CA].m_endVertex = A;
                    m_mesh.m_halfEdges[BC].m_endVertex = C;

                    auto & newFace = m_mesh.m_faces[newFaceIndex];

                    const float3 planeNormal = getTriangleNormal(m_vertexData[A],m_vertexData[B],activePoint);
                    newFace.m_P = plane(planeNormal, activePoint);
                    newFace.m_he = AB;

                    m_mesh.m_halfEdges[CA].m_opp = m_newHalfEdgeIndices[i>0 ? i*2-1 : 2*horizonEdgeCount-1];
                    m_mesh.m_halfEdges[BC].m_opp = m_newHalfEdgeIndices[((i+1)*2) % (horizonEdgeCount*2)];
                }

                // Assign points that were on the positive side of the disabled faces to the new faces.
                for (auto & disabledPoints : m_disabledFacePointVectors) 
                {
                    assert(disabledPoints);

                    for (const auto & point : *(disabledPoints)) 
                    {
                        if (point == activePointIndex) 
                        {
                            continue;
                        }

                        for (size_t j=0;j<horizonEdgeCount;j++) 
                        {
                            if (addPointToFace(m_mesh.m_faces[m_newFaceIndices[j]], point)) 
                            {
                                break;
                            }
                        }
                    }
                    // The points are no longer needed: we can move them to the vector pool for reuse.
                    reclaimToIndexVectorPool(disabledPoints);
                }

                // Increase face stack size if needed
                for (const auto newFaceIndex : m_newFaceIndices) 
                {
                    auto & newFace = m_mesh.m_faces[newFaceIndex];
                    if (newFace.m_pointsOnPositiveSide) 
                    {
                        assert(newFace.m_pointsOnPositiveSide->size()>0);
                        if (!newFace.m_inFaceStack) 
                        {
                            faceList.push_back(newFaceIndex);
                            newFace.m_inFaceStack = 1;
                        }
                    }
                }
            }
            
            // Cleanup
            m_indexVectorPool.clear();
        }
        
        // Constructs the convex hull into a MeshBuilder object which can be converted to a ConvexHull or Mesh object
        void buildMesh(bool CCW, bool useOriginalIndices, float eps)
        {
            m_extremeValues = getExtremeValues(); // find extreme values and use them to compute the scale of the point cloud.
            m_scale = getScale(m_extremeValues);
            
            m_epsilon = Epsilon * m_scale; // Epsilon we use depends on the scale
            m_epsilonSquared = m_epsilon*m_epsilon;
            
            m_planar = false; // The planar case happens when all the points appear to lie on a two dimensional subspace of R^3.

            createConvexHalfEdgeMesh();

            if (m_planar) 
            {
                const size_t extraPointIndex = m_planarPointCloudTemp.size()-1;
                for (auto & he : m_mesh.m_halfEdges) 
                {
                    if (he.m_endVertex == extraPointIndex) he.m_endVertex = 0;
                }
                m_planarPointCloudTemp.clear();
            }
        }

        ConvexHull getConvexHull(bool CCW, bool useOriginalIndices, float eps)
        {
            buildMesh(CCW, useOriginalIndices, eps);
            return ConvexHull(m_mesh, m_vertexData, CCW, useOriginalIndices);
        }

    public:
        
        // Fair warning: QuickHull has the capacity to mutate the output of pointCloud.
        QuickHull(std::vector<float3> & pointCloud) : m_vertexData(pointCloud) { }

        /*
         * UseOriginalIndices: should the output mesh use same vertex indices as the original point cloud. If this is false,
         * then we generate a new vertex buffer which contains only the vertices that are part of the convex hull.
         * Epsilon : minimum distance to a plane to consider a point being on positive of it (for a point cloud with scale 1)
         */
        ConvexHull computeConvexHull(bool formatOutputCCW, bool useOriginalIndices, float eps = 0.00001)
        {
            assert(m_vertexData.size() >= 3);
            return getConvexHull(formatOutputCCW, useOriginalIndices, eps);
        }
        
        const size_t & getFailedHorizonEdges() { return m_failedHorizonEdges; }
    };

} // namespace quickhull

#endif // polymer_quickhull_hpp

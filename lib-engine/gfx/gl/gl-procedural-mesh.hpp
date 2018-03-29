#ifndef gl_procedural_mesh_hpp
#define gl_procedural_mesh_hpp

#include "procedural_mesh.hpp"
#include "gl-mesh.hpp"

#include <assert.h>

namespace polymer
{
    
    inline GlMesh make_cube_mesh()
    {
        return make_mesh_from_geometry(make_cube());
    }

    inline GlMesh make_sphere_mesh(float radius)
    {
        return make_mesh_from_geometry(make_sphere(radius));
    }
    
    inline GlMesh make_cylinder_mesh(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded = false)
    {
        return make_mesh_from_geometry(make_cylinder(radiusTop, radiusBottom, height, radialSegments, heightSegments, openEnded));
    }
  
    inline GlMesh make_ring_mesh(float innerRadius = 1.0f, float outerRadius = 2.0f)
    {
        return make_mesh_from_geometry(make_ring(innerRadius, outerRadius));
    }

    inline GlMesh make_3d_ring_mesh(float innerRadius = 1.0f, float outerRadius = 2.0f, float length = 1.0f)
    {
        return make_mesh_from_geometry(make_3d_ring(innerRadius, outerRadius, length));
    }

    inline GlMesh make_frustum_mesh(float aspectRatio = 1.33333f)
    {
        auto frustumMesh = make_mesh_from_geometry(make_frustum(aspectRatio));
        frustumMesh.set_non_indexed(GL_LINES);
        return frustumMesh;
    }
   
    inline GlMesh make_torus_mesh(int radial_segments = 8)
    {
        return make_mesh_from_geometry(make_torus(radial_segments));
    }

    inline GlMesh make_capsule_mesh(int segments, float radius, float length)
    {
        return make_mesh_from_geometry(make_capsule(segments, radius, length));
    }
    
    inline GlMesh make_plane_mesh(float width, float height, uint32_t nw, uint32_t nh, bool backfaces = true)
    {
        return make_mesh_from_geometry(make_plane(width, height, nw, nh, backfaces));
    }
    
    inline GlMesh make_curved_plane_mesh()
    {
        return make_mesh_from_geometry(make_curved_plane());
    }
    
    inline GlMesh make_axis_mesh()
    {
        auto axisMesh = make_mesh_from_geometry(make_axis());
        axisMesh.set_non_indexed(GL_LINES);
        return axisMesh;
    }

    inline GlMesh make_axis_mesh(const float3 & xAxis, const float3 & yAxis, const float3 & zAxis)
    {
        auto axisMesh = make_mesh_from_geometry(make_axis(xAxis, yAxis, zAxis));
        axisMesh.set_non_indexed(GL_LINES);
        return axisMesh;
    }

    inline GlMesh make_spiral_mesh(float resolution = 512.0f, float freq = 128.f)
    {
        assert(freq < resolution);
        auto sprialMesh = make_mesh_from_geometry(make_spiral());
        sprialMesh.set_non_indexed(GL_LINE_STRIP);
        return sprialMesh;
    }
    
    inline GlMesh make_icosahedron_mesh()
    {
        return make_mesh_from_geometry(make_icosahedron());
    }
    
    inline GlMesh make_octohedron_mesh()
    {
        return make_mesh_from_geometry(make_octohedron());
    }
    
    inline GlMesh make_tetrahedron_mesh()
    {
        return make_mesh_from_geometry(make_tetrahedron());
    }
    
    inline GlMesh make_fullscreen_quad_ndc()
    {
        Geometry g;
        g.vertices = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
        g.texcoord0 = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
        g.faces = { { 0, 1, 2 },{ 3, 4, 5 } };
        return make_mesh_from_geometry(g);
    }

    inline GlMesh make_fullscreen_quad()
    {
        return make_fullscreen_quad_ndc();
    }

    inline GlMesh make_fullscreen_quad_screenspace()
    {
        Geometry g;
        g.vertices = { { 0.0f, 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
        g.texcoord0 = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
        g.faces = { { 0, 1, 2 },{ 3, 4, 5 }, { 5, 4, 3 },{ 2, 1, 0 } }; // with backfaces
        return make_mesh_from_geometry(g);
    }

    inline GlMesh make_supershape_3d_mesh(const int segments, const float m, const float n1, const float n2, const float n3, const float a = 1.0, const float b = 1.0)
    {
        return make_mesh_from_geometry(make_supershape_3d(segments, m, n1, n2, n3, a, b));
    }
    
}

#endif // gl_procedural_mesh_hpp

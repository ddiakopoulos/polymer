#pragma once

#ifndef polymer_environment_hpp
#define polymer_environment_hpp

#include "geometry.hpp"

#include "gl-api.hpp"
#include "gl-mesh-util.hpp"
#include "gl-camera.hpp"
#include "gl-procedural-sky.hpp"

#include "uniforms.hpp"
#include "material.hpp"
#include "material-library.hpp"
#include "asset-handle-utils.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"

using namespace polymer;

namespace polymer
{
    struct screen_raycaster
    {
        perspective_camera & cam; float2 viewport;
        screen_raycaster(perspective_camera & camera, const float2 viewport) : cam(camera), viewport(viewport) {}
        ray from(const float2 & cursor) const { return cam.get_world_ray(cursor, viewport); };
    };

    struct raycast_result
    {
        bool hit{ false };
        float distance{ std::numeric_limits<float>::max() };
        float3 normal{ 0, 0, 0 };
        raycast_result(bool h, float t, float3 n) : hit(h), distance(t), normal(n) {}
    };
}

//////////////////////////
//   Scene Definition   //
//////////////////////////

namespace polymer
{
    ////////////////////////
    //   mesh_component   //
    ////////////////////////

    // GPU-side gl_mesh
    struct mesh_component : public base_component
    {
        gpu_mesh_handle mesh;
        mesh_component() {};
        mesh_component(entity e) : base_component(e) {}
        void set_mesh_render_mode(const GLenum mode) { if (mode != GL_TRIANGLE_STRIP) mesh.get().set_non_indexed(mode); }
        void draw() const { mesh.get().draw_elements(); }
    };
    POLYMER_SETUP_TYPEID(mesh_component);

    template<class F> void visit_fields(mesh_component & o, F f)
    {
        f("gpu_mesh_handle", o.mesh);
    }

    ////////////////////////////
    //   material_component   //
    ////////////////////////////

    struct material_component : public base_component
    {
        material_handle material;
        bool receive_shadow{ true };
        bool cast_shadow{ true };
        material_component() {};
        material_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(material_component);

    template<class F> void visit_fields(material_component & o, F f)
    {
        f("material_handle", o.material);
        f("receive_shadow", o.receive_shadow);
        f("cast_shadow", o.cast_shadow);
    }

    ////////////////////////////
    //   geometry_component   //
    ////////////////////////////

    // CPU-side runtime_mesh
    struct geometry_component : public base_component
    {
        cpu_mesh_handle geom;
        geometry_component() {};
        geometry_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    template<class F> void visit_fields(geometry_component & o, F f)
    {
        f("cpu_mesh_handle", o.geom);
    }

    ///////////////////////////////
    //   point_light_component   //
    ///////////////////////////////

    struct point_light_component : public base_component
    {
        bool enabled = true;
        uniforms::point_light data;
        point_light_component() {};
        point_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(point_light_component);

    template<class F> void visit_fields(point_light_component & o, F f)
    {
        f("enabled", o.enabled);
        f("position", o.data.position);
        f("color", o.data.color);
        f("radius", o.data.radius);
    }

    /////////////////////////////////////
    //   directional_light_component   //
    /////////////////////////////////////

    struct directional_light_component : public base_component
    {
        bool enabled = true;
        uniforms::directional_light data;
        directional_light_component() {};
        directional_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(directional_light_component);

    template<class F> void visit_fields(directional_light_component & o, F f)
    {
        f("enabled", o.enabled);
        f("direction", o.data.direction);
        f("color", o.data.color);
        f("amount", o.data.amount);
    }

    /////////////////////
    //   environment   //
    /////////////////////

    class pbr_render_system;
    class collision_system;
    class transform_system;
    class identifier_system;
    struct material_library;

    class environment
    {
        std::vector<entity> active_entities;
    public:
        std::shared_ptr<polymer::material_library> mat_library;
        std::unique_ptr<polymer::gl_procedural_sky> skybox;
        polymer::pbr_render_system * render_system;
        polymer::collision_system * collision_system;
        polymer::transform_system * xform_system;
        polymer::identifier_system * identifier_system;
        entity track_entity(entity e);
        const std::vector<entity> & entity_list();
        void destroy(entity e);
    };

} // end namespace polymer

#endif // end polymer_environment_hpp

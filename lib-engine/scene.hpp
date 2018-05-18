#pragma once

#ifndef core_scene_hpp
#define core_scene_hpp

#include "geometry.hpp"

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"
#include "gl-mesh.hpp"
#include "gl-procedural-sky.hpp"

#include "uniforms.hpp"
#include "material.hpp"
#include "material-library.hpp"
#include "asset-handle-utils.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/system-name.hpp"
#include "ecs/system-transform.hpp"

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
    struct material_library;

    // Mesh Component (GPU-side gl_mesh and associated material)
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

    // Material
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

    // geometry (CPU-side runtime_mesh)
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

    // Point Light
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

    // Directional Light
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

    // Collision System
    class collision_system final : public base_system
    {
    public:
        std::unordered_map<entity, geometry_component> meshes;
        transform_system * xform_system{ nullptr };

        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, hash(get_typename<geometry_component>()));
        }

        raycast_result raycast(const entity e, const ray & worldRay)
        {
            if (!xform_system)
            {
                base_system * xform_base = orchestrator->get_system(get_typeid<transform_system>());
                xform_system = dynamic_cast<transform_system *>(xform_base);
                assert(xform_system != nullptr);
            }

            const transform meshPose = xform_system->get_world_transform(e)->world_pose;
            const float3 meshScale = xform_system->get_local_transform(e)->local_scale;
            const runtime_mesh & geometry = meshes[e].geom.get();

            ray localRay = meshPose.inverse() * worldRay;
            localRay.origin /= meshScale;
            localRay.direction /= meshScale;
            float outT = 0.0f;
            float3 outNormal = { 0, 0, 0 };
            const bool hit = intersect_ray_mesh(localRay, geometry, &outT, &outNormal);
            return{ hit, outT, outNormal };
        }

        virtual bool create(entity e, poly_typeid hash, void * data) override final { return true; }
        virtual void destroy(entity e) override final {}
    };
    POLYMER_SETUP_TYPEID(collision_system);

    template<class F> void visit_component_fields(entity e, collision_system * system, F f)
    {
        auto iter = system->meshes.find(e);
        if (iter != system->meshes.end()) f("geometry_component", iter->second);
    }
}

namespace polymer
{
    class pbr_render_system;
}

class poly_scene
{
    std::vector<entity> active_entities;
public:
    std::shared_ptr<polymer::material_library> mat_library;
    std::unique_ptr<polymer::gl_procedural_sky> skybox;
    polymer::pbr_render_system * render_system;
    polymer::collision_system * collision_system;
    polymer::transform_system * xform_system;
    polymer::name_system * name_system;
    entity track_entity(entity e) { active_entities.push_back(e); return e; }
    std::vector<entity> & entity_list() { return active_entities; }
    void clear_tracked_entities() { active_entities.clear(); }
    void destroy(entity e)
    {
       //visit_systems(this, [e](const char * name, auto * system_pointer)
       //{
       //    if (system_pointer) system_pointer->destroy(e);
       //});
    }
};

#endif // end core_scene_hpp
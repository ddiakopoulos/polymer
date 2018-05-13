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
        Ray from(const float2 & cursor) const { return cam.get_world_ray(cursor, viewport); };
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

    // Mesh Component (GPU-side GlMesh and associated material)
    struct mesh_component : public base_component
    {
        gpu_mesh_handle mesh;

        void set_mesh_render_mode(const GLenum mode)
        {
            if (mode != GL_TRIANGLE_STRIP) mesh.get().set_non_indexed(mode);
        }

        void draw() const
        {
            mesh.get().draw_elements();
        }

        mesh_component() {};
        mesh_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(mesh_component);

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

    // Geometry (CPU-side runtime_mesh)
    struct geometry_component : public base_component
    {
        cpu_mesh_handle geom;
        geometry_component() {};
        geometry_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    // Point Light
    struct point_light_component : public base_component
    {
        bool enabled = true;
        uniforms::point_light data;

        point_light_component() {};
        point_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(point_light_component);

    // Directional Light
    struct directional_light_component : public base_component
    {
        bool enabled = true;
        uniforms::directional_light data;

        directional_light_component() {};
        directional_light_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(directional_light_component);

    // Collision System
    class collision_system final : public base_system
    {
    public:
        std::unordered_map<entity, geometry_component> meshes;
        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, hash(get_typename<geometry_component>()));
        }
    };
    POLYMER_SETUP_TYPEID(collision_system);
}

namespace polymer
{
    class pbr_render_system;
}

struct poly_scene
{
    std::unique_ptr<polymer::material_library> mat_library;
    std::unique_ptr<polymer::pbr_render_system> render_system;
    std::unique_ptr<polymer::collision_system> collision_system;
    std::unique_ptr<polymer::transform_system> xform_system;
    std::unique_ptr<polymer::name_system> name_system;
    std::shared_ptr<polymer::gl_procedural_sky> skybox;
};

#endif // end core_scene_hpp
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
#include <string>

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

    template<class F> void visit_fields(mesh_component & o, F f) {
        f("gpu_mesh_handle", o.mesh);
    }

    template<class Archive> void serialize(Archive & archive, mesh_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
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

    template<class F> void visit_fields(material_component & o, F f) {
        f("material_handle", o.material);
        f("receive_shadow", o.receive_shadow);
        f("cast_shadow", o.cast_shadow);
    }

    template<class Archive> void serialize(Archive & archive, material_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
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

    template<class F> void visit_fields(geometry_component & o, F f) {
        f("cpu_mesh_handle", o.geom);
    }

    template<class Archive> void serialize(Archive & archive, geometry_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
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

    template<class F> void visit_fields(point_light_component & o, F f) {
        f("enabled", o.enabled);
        f("position", o.data.position);
        f("color", o.data.color);
        f("radius", o.data.radius);
    }

    template<class Archive> void serialize(Archive & archive, point_light_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
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

    template<class Archive> void serialize(Archive & archive, directional_light_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
    }

    ///////////////////////////////////////////////////////////
    //   scene_graph_component & world_transform_component   //
    ///////////////////////////////////////////////////////////

    struct scene_graph_component : public base_component
    {
        scene_graph_component() {};
        scene_graph_component(entity e) : base_component(e) {}
        polymer::transform local_pose;
        polymer::float3 local_scale;
        entity parent{ kInvalidEntity };
        std::vector<entity> children;
    };
    POLYMER_SETUP_TYPEID(scene_graph_component);

    template<class F> void visit_fields(scene_graph_component & o, F f)
    {
        f("local_pose", o.local_pose);
        f("local_scale", o.local_scale);
        f("parent", o.parent);
        f("children", o.children, editor_hidden{});
    }

    template<class Archive> void serialize(Archive & archive, scene_graph_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
    }

    struct world_transform_component : public base_component
    {
        world_transform_component() {};
        world_transform_component(entity e) : base_component(e) {}
        polymer::transform world_pose;
    };
    POLYMER_SETUP_TYPEID(world_transform_component);

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
        void import_environment(const std::string & path);
        void export_environment(const std::string & path);
        entity track_entity(entity e);        
        const std::vector<entity> & entity_list();
        void copy(entity src, entity dest);
        void destroy(entity e);
    };


    //inline void prologue(cereal::JSONOutputArchive & archive, polymer::environment const & o) {}
    //inline void epilogue(cereal::JSONOutputArchive & archive, polymer::environment const & o) {}

    template<class Archive> void serialize(Archive & archive, environment & env)
    {
        // foreach entity
        for (const auto & e : env.entity_list())
        {
            archive(cereal::make_nvp("entity", e));

            // foreach system
            visit_systems(&env, [&](const char * system_name, auto * system_pointer)
            {
                if (system_pointer)
                {
                    // foreach component
                    visit_components(e, system_pointer, [&](const char * component_name, auto & component_ref, auto... component_metadata)
                    {
                        //auto & t = ;

                        auto n = get_typename<std::decay<decltype(component_ref)>::type>();
                        std::cout << n << std::endl;
                        //archive(cereal::make_nvp(, component_ref));
                    });
                }
            });
        }
    }

} // end namespace polymer

#endif // end polymer_environment_hpp

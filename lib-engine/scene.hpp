#pragma once

#ifndef polymer_scene_hpp
#define polymer_scene_hpp

#include "geometry.hpp"

#include "gl-api.hpp"
#include "gl-mesh-util.hpp"
#include "gl-camera.hpp"
#include "gl-procedural-sky.hpp"

#include "renderer-uniforms.hpp"
#include "material.hpp"
#include "material-library.hpp"
#include "asset-handle-utils.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/core-events.hpp"

#include "json.hpp"

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
        float2 uv{ 0, 0 };
        raycast_result() {};
        raycast_result(bool h, float t, float3 n, float2 uv) : hit(h), distance(t), normal(n), uv(uv) {}
    };

    struct entity_hit_result
    {
        entity e{ kInvalidEntity };
        raycast_result r;
    };
}

namespace linalg
{
    using json = nlohmann::json;

    // Linalg Types
    inline void to_json(json & archive, const int2 & m)   { archive = json{ { "x", m.x() },{ "y", m.y() } }; }
    inline void from_json(const json & archive, int2 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); }

    inline void to_json(json & archive, const int3 & m)   { archive = json{ { "x", m.x() },{ "y", m.y() },{ "z", m.z() } }; }
    inline void from_json(const json & archive, int3 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); p.z = archive.at("z").get<int32_t>(); }

    inline void to_json(json & archive, const int4 & m)   { archive = json{ { "x", m.x() },{ "y", m.y() },{ "z", m.z() },{ "w", m.w() } }; }
    inline void from_json(const json & archive, int4 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); p.z = archive.at("z").get<int32_t>(); p.w = archive.at("w").get<int32_t>(); }

    inline void to_json(json & archive, const float2 & m) { archive = json{ { "x", m.x() },{ "y", m.y() } }; }
    inline void from_json(const json & archive, float2 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); }

    inline void to_json(json & archive, const float3 & m) { archive = json{ { "x", m.x() },{ "y", m.y() },{ "z", m.z() } }; }
    inline void from_json(const json & archive, float3 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); p.z = archive.at("z").get<float>(); }

    inline void to_json(json & archive, const float4 & m) { archive = json{ { "x", m.x() },{ "y", m.y() },{ "z", m.z() },{ "w", m.w() } }; }
    inline void from_json(const json & archive, float4 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); p.z = archive.at("z").get<float>(); p.w = archive.at("w").get<float>(); }

    inline void to_json(json & archive, const quatf & m) { archive = json{ { "x", m.x },{ "y", m.y },{ "z", m.z },{ "w", m.w } }; }
    inline void from_json(const json & archive, quatf & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); p.z = archive.at("z").get<float>(); p.w = archive.at("w").get<float>(); }
}

namespace polymer
{
    using json = nlohmann::json;

    template<class F> void visit_fields(polymer::transform & o, F f) { f("position", o.position); f("orientation", o.orientation); }

    // Polymer Asset Handles
    inline void to_json(json & archive, const texture_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, texture_handle & m) { m = texture_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const gpu_mesh_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, gpu_mesh_handle & m) { m = gpu_mesh_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const cpu_mesh_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, cpu_mesh_handle & m) { m = cpu_mesh_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const material_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, material_handle & m) { m = material_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const shader_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, shader_handle & m) { m = shader_handle(archive.get<std::string>()); }

    // Polymer Primitive Types
    inline void to_json(json & archive, const aabb_2d & m) { archive = json{ { "min", m._min },{ "max", m._max } }; }
    inline void from_json(const json & archive, aabb_2d & p) { p._min = archive.at("min").get<float2>(); p._max = archive.at("max").get<float2>(); }

    inline void to_json(json & archive, const aabb_3d & m) { archive = json{ { "min", m._min },{ "max", m._max } }; }
    inline void from_json(const json & archive, aabb_3d & p) { p._min = archive.at("min").get<float3>(); p._max = archive.at("max").get<float3>(); }

    inline void to_json(json & archive, const transform & m) { archive = json{ { "position", m.position },{ "orientation", m.orientation } }; }
    inline void from_json(const json & archive, transform & p) { p.position = archive.at("position").get<float3>(); p.orientation = archive.at("orientation").get<quatf>(); }

}

//////////////////////////
//   Scene Definition   //
//////////////////////////

namespace polymer
{

    //////////////////////////////
    //   identifier_component   //
    //////////////////////////////

    struct identifier_component : public base_component
    {
        std::string id;
        identifier_component() {};
        identifier_component(entity e) : base_component(e) {}
        identifier_component(entity e, const std::string & id) : base_component(e), id(id) {}
    };
    POLYMER_SETUP_TYPEID(identifier_component);

    template<class F> void visit_fields(identifier_component & o, F f) {
        f("id", o.id);
    }

    inline void to_json(json & j, const identifier_component & p) {
        visit_fields(const_cast<identifier_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, identifier_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ////////////////////////
    //   mesh_component   //
    ////////////////////////

    // GPU-side gl_mesh
    struct mesh_component : public base_component
    {
        gpu_mesh_handle mesh;
        mesh_component() {};
        mesh_component(entity e) : base_component(e) {}
        mesh_component(entity e, gpu_mesh_handle & handle) : base_component(e), mesh(handle) {}
        void set_mesh_render_mode(const GLenum mode) { if (mode != GL_TRIANGLE_STRIP) mesh.get().set_non_indexed(mode); }
        void draw() const { mesh.get().draw_elements(); }
    };
    POLYMER_SETUP_TYPEID(mesh_component);

    template<class F> void visit_fields(mesh_component & o, F f) {
        f("gpu_mesh_handle", o.mesh);
    }

    inline void to_json(json & j, const mesh_component & p) {
        visit_fields(const_cast<mesh_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, mesh_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ////////////////////////////
    //   material_component   //
    ////////////////////////////

    struct material_component : public base_component
    {
        material_handle material{ material_library::kDefaultMaterialId };
        bool receive_shadow{ true };
        bool cast_shadow{ true };
        material_component() {};
        material_component(entity e) : base_component(e) {}
        material_component(entity e, material_handle handle) : base_component(e), material(handle) {}
    };
    POLYMER_SETUP_TYPEID(material_component);

    template<class F> void visit_fields(material_component & o, F f) {
        f("material_handle", o.material);
        f("receive_shadow", o.receive_shadow);
        f("cast_shadow", o.cast_shadow);
    }

    inline void to_json(json & j, const material_component & p) {
        visit_fields(const_cast<material_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, material_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ////////////////////////////
    //   geometry_component   //
    ////////////////////////////

    // CPU-side runtime_mesh
    struct geometry_component : public base_component
    {
        cpu_mesh_handle geom;
        cpu_mesh_handle proxy_geom;
        geometry_component() {};
        geometry_component(entity e) : base_component(e) {}
        geometry_component(entity e, cpu_mesh_handle handle) : base_component(e), geom(handle) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    template<class F> void visit_fields(geometry_component & o, F f) {
        f("cpu_mesh_handle", o.geom);
        f("cpu_mesh_proxy_handle", o.proxy_geom);
    }

    inline void to_json(json & j, const geometry_component & p) {
        visit_fields(const_cast<geometry_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, geometry_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };
    
    /////////////////////////////////////
    //   procedural_skybox_component   //
    /////////////////////////////////////

    struct procedural_skybox_component : public base_component
    {
        polymer::gl_hosek_sky sky;
        entity sun_directional_light {kInvalidEntity};
        procedural_skybox_component() {};
        procedural_skybox_component(entity e) : base_component(e) { }
    };
    POLYMER_SETUP_TYPEID(procedural_skybox_component);
    POLYMER_SETUP_TYPEID(polymer::gl_hosek_sky);

    template<class F> void visit_fields(procedural_skybox_component & o, F f) {
        f("procedural_skybox", o.sky);
        f("sun_directional_light", o.sun_directional_light, entity_ref{});
    }

    inline void to_json(json & j, const procedural_skybox_component & p) {
        visit_fields(const_cast<procedural_skybox_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, procedural_skybox_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ///////////////////////////
    //   cubemap_component   //
    ///////////////////////////

    struct cubemap_component : public base_component
    {
        texture_handle ibl_radianceCubemap{ "default-radiance-cubemap" };
        texture_handle ibl_irradianceCubemap{ "default-irradiance-cubemap" };
        bool force_draw{ false };
        cubemap_component() {};
        cubemap_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(cubemap_component);

    template<class F> void visit_fields(cubemap_component & o, F f) {
        f("ibl_radiance_cubemap", o.ibl_radianceCubemap);
        f("ibl_irradiance_cubemap", o.ibl_irradianceCubemap);
        f("force_draw", o.force_draw);
    }

    inline void to_json(json & j, const cubemap_component & p) {
        visit_fields(const_cast<cubemap_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, cubemap_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ///////////////////////////////
    //   point_light_component   //
    ///////////////////////////////

    struct point_light_component : public base_component
    {
        bool enabled{ true };
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

    inline void to_json(json & j, const point_light_component & p) {
        visit_fields(const_cast<point_light_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, point_light_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

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

    inline void to_json(json & j, const directional_light_component & p) {
        visit_fields(const_cast<directional_light_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, directional_light_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    ///////////////////////////////////////////////////////////////
    //   local_transform_component & world_transform_component   //
    ///////////////////////////////////////////////////////////////

    struct local_transform_component : public base_component
    {
        local_transform_component() {};
        local_transform_component(entity e) : base_component(e) {}
        polymer::transform local_pose;
        polymer::float3 local_scale{ 1, 1, 1 };
        entity parent{ kInvalidEntity };
        std::vector<entity> children;
    };
    POLYMER_SETUP_TYPEID(local_transform_component);

    template<class F> void visit_fields(local_transform_component & o, F f)
    {
        f("entity", o.get_entity_ref(), serializer_hidden{}); 
        f("local_pose", o.local_pose);
        f("local_scale", o.local_scale);
        f("parent", o.parent);
        f("children", o.children); // editor_hidden{}
    }

    inline void to_json(json & j, const local_transform_component & p) {
        visit_fields(const_cast<local_transform_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, local_transform_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            if (auto * no_serialize = unpack<serializer_hidden>(metadata...)) return;
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    struct world_transform_component : public base_component
    {
        world_transform_component() {};
        world_transform_component(entity e) : base_component(e) {}
        polymer::transform world_pose;
    };
    POLYMER_SETUP_TYPEID(world_transform_component);

    //////////////////////////
    //   render_component   //
    //////////////////////////

    /// @todo - render_component submission groups
    struct render_component : public base_component
    {
        render_component() {};
        render_component(entity e) : base_component(e) {}
        polymer::material_component * material{ nullptr };
        polymer::mesh_component * mesh{ nullptr };
        polymer::world_transform_component * world_transform{ nullptr };
        polymer::local_transform_component * local_transform{ nullptr };
        uint32_t render_sort_order {0};
    };
    POLYMER_SETUP_TYPEID(render_component);

    template<class F> void visit_fields(render_component & o, F f)
    {
        f("material_component", o.material);
        f("mesh_component", o.mesh);
        f("world_transform_component", o.world_transform);
        f("local_transform_component", o.local_transform);
    }

    ///////////////
    //   scene   //
    ///////////////

    class render_system;
    class collision_system;
    class transform_system;
    class identifier_system;
    struct material_library;
    class asset_resolver;

    class scene
    {
        std::vector<entity> active_entities;
        std::unordered_map<entity, entity> remap_table; // for serialization only

        template<typename component_t, typename system_t> bool inflate_serialized_component(
            const entity new_entity,
            const std::string & type_name,
            system_t * system_pointer,
            polymer::json::const_iterator & componentIterator)
        {
            const auto system_name = get_typename<std::remove_pointer_t<decltype(system_pointer)>>();

            if (type_name == get_typename<component_t>())
            {
                component_t the_new_component = componentIterator.value();
                the_new_component.e = new_entity;

                if (system_pointer->create(new_entity, get_typeid(type_name.c_str()), &the_new_component))
                {
                    // So what's this nonsense? If a field has an `entity_ref` attribute, it means that the entity refers to another entity. 
                    // The implication for serialization is that we need to remap the old entity value to the new one. 
                    // Due to the use of auto and this template, the compiler is very unclear about the type of the field 
                    // in the ` if (use_entity_ref) ... ` block. Thus, we resort to memcpy to avoid even uglier hacks. 
                    visit_fields(the_new_component, [&the_new_component, this](const char * name, auto & field, auto... metadata)
                    {
                        if (auto * use_entity_ref = unpack<entity_ref>(metadata...))
                        {   
                            entity deserialized_entity = kInvalidEntity;
                            std::memcpy(&deserialized_entity, &field, sizeof(entity));
                            entity remapped_entity = remap_table[deserialized_entity];
                            std::memcpy(&field, &remapped_entity, sizeof(entity));
                        }
                    });
                    //log::get()->engine_log->info("created {} on {}", type_name, system_name);
                }
                else { /* log::get()->engine_log->info("could not create {} on {}", type_name, system_name); */ }

                return true;
            }
            return false;
        }

    public:

        std::unique_ptr<polymer::material_library> mat_library;
        std::unique_ptr<polymer::event_manager_async> event_manager;
        std::unique_ptr<polymer::asset_resolver> resolver;

        polymer::render_system * render_system;
        polymer::collision_system * collision_system;
        polymer::transform_system * xform_system; 
        polymer::identifier_system * identifier_system;

        void import_environment(const std::string & path, entity_system_manager & o);
        void export_environment(const std::string & path);

        entity track_entity(entity e);        
        const std::vector<entity> & entity_list();
        void copy(entity src, entity dest);
        void destroy(entity e);

        void reset(entity_system_manager & o, int2 default_renderer_resolution, bool create_default_entities = false);
    };

    template<class F> void visit_systems(scene * p, F f)
    {
        f("identifier_system", p->identifier_system);
        f("transform_system", p->xform_system);
        f("render_system", p->render_system);
        f("collision_system", p->collision_system);
    }

    render_component assemble_render_component(scene & env, const entity e);

    entity make_standard_scene_object(entity_system_manager * esm, scene * env,
        const std::string & name, const transform & pose, const float3 & scale,
        const material_handle & mh, const gpu_mesh_handle & gmh, const cpu_mesh_handle & cmh);

} // end namespace polymer

#endif // end polymer_scene_hpp

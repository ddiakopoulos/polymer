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
        raycast_result(bool h, float t, float3 n) : hit(h), distance(t), normal(n) {}
    };
}

//////////////////////////
//   Scene Definition   //
//////////////////////////

namespace linalg
{
    using json = nlohmann::json;

    // Linalg Types
    inline void to_json(json & archive, const int2 & m)   { archive = json{ { "x", m.x },{ "y", m.y } }; }
    inline void from_json(const json & archive, int2 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); }

    inline void to_json(json & archive, const int3 & m)   { archive = json{ { "x", m.x },{ "y", m.y },{ "z", m.z } }; }
    inline void from_json(const json & archive, int3 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); p.z = archive.at("z").get<int32_t>(); }

    inline void to_json(json & archive, const int4 & m)   { archive = json{ { "x", m.x },{ "y", m.y },{ "z", m.z },{ "w", m.w } }; }
    inline void from_json(const json & archive, int4 & p) { p.x = archive.at("x").get<int32_t>(); p.y = archive.at("y").get<int32_t>(); p.z = archive.at("z").get<int32_t>(); p.w = archive.at("w").get<int32_t>(); }

    inline void to_json(json & archive, const float2 & m) { archive = json{ { "x", m.x },{ "y", m.y } }; }
    inline void from_json(const json & archive, float2 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); }

    inline void to_json(json & archive, const float3 & m) { archive = json{ { "x", m.x },{ "y", m.y },{ "z", m.z } }; }
    inline void from_json(const json & archive, float3 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); p.z = archive.at("z").get<float>(); }

    inline void to_json(json & archive, const float4 & m) { archive = json{ { "x", m.x },{ "y", m.y },{ "z", m.z },{ "w", m.w } }; }
    inline void from_json(const json & archive, float4 & p) { p.x = archive.at("x").get<float>(); p.y = archive.at("y").get<float>(); p.z = archive.at("z").get<float>(); p.w = archive.at("w").get<float>(); }
}

namespace polymer
{
    using json = nlohmann::json;

    template<class F> void visit_fields(polymer::transform & o, F f) { f("position", o.position); f("orientation", o.orientation); }

    // Polymer Asset Handles
    inline void to_json(json & archive, const texture_handle & m)  { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, texture_handle & m) { m = texture_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const gpu_mesh_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, gpu_mesh_handle & m) { m = gpu_mesh_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const cpu_mesh_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, cpu_mesh_handle & m) { m = cpu_mesh_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const material_handle & m) { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, material_handle & m) { m = material_handle(archive.get<std::string>()); }

    inline void to_json(json & archive, const shader_handle & m)   { archive = m.name == "empty" ? "" : m.name; }
    inline void from_json(const json & archive, shader_handle & m) { m = shader_handle(archive.get<std::string>()); }

    // Polymer Primitive Types
    inline void to_json(json & archive, const aabb_2d & m)   { archive = json{ { "min", m._min },{ "max", m._max } }; }
    inline void from_json(const json & archive, aabb_2d & p) { p._min = archive.at("min").get<float2>(); p._max = archive.at("max").get<float2>(); }

    inline void to_json(json & archive, const aabb_3d & m)   { archive = json{ { "min", m._min },{ "max", m._max } }; }
    inline void from_json(const json & archive, aabb_3d & p) { p._min = archive.at("min").get<float3>(); p._max = archive.at("max").get<float3>(); }

    inline void to_json(json & archive, const transform & m) { archive = json{ { "position", m.position },{ "orientation", m.orientation } }; }
    inline void from_json(const json & archive, transform & p) { p.position = archive.at("position").get<float3>(); p.orientation = archive.at("orientation").get<float4>(); }

    //////////////////////////////
    //   identifier_component   //
    //////////////////////////////

    struct identifier_component : public base_component
    {
        std::string id;
        identifier_component() {};
        identifier_component(const std::string & id) : id(id) {};
        identifier_component(entity e) : base_component(e) {}
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
        geometry_component() {};
        geometry_component(entity e) : base_component(e) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    template<class F> void visit_fields(geometry_component & o, F f) {
        f("cpu_mesh_handle", o.geom);
    }

    inline void to_json(json & j, const geometry_component & p) {
        visit_fields(const_cast<geometry_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, geometry_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

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

    inline void to_json(json & j, const scene_graph_component & p) {
        visit_fields(const_cast<scene_graph_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, scene_graph_component & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
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
        void import_environment(const std::string & path, entity_orchestrator & o);
        void export_environment(const std::string & path);
        entity track_entity(entity e);        
        const std::vector<entity> & entity_list();
        void copy(entity src, entity dest);
        void destroy(entity e);
    };

    template<class F> void visit_systems(environment * p, F f)
    {
        f("identifier_system", p->identifier_system);
        f("transform_system", p->xform_system);
        f("render_system", p->render_system);
        f("collision_system", p->collision_system);
    }

} // end namespace polymer

#endif // end polymer_environment_hpp

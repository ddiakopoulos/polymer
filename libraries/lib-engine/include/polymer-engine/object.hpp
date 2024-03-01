#pragma once

#ifndef polymer_object_hpp
#define polymer_object_hpp

#include <unordered_map>
#include <memory>
#include <limits>

#include "polymer-core/tools/property.hpp"

#include "polymer-engine/material.hpp"
#include "polymer-engine/material-library.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/ecs/component-pool.hpp"
#include "nlohmann/json.hpp"

#include "polymer-engine/renderer/renderer-procedural-sky.hpp"
#include "polymer-engine/renderer/renderer-uniforms.hpp"

namespace linalg
{
    using json = nlohmann::json;
    using namespace linalg::aliases;

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

    inline void to_json(json & archive, const entity & m) { archive = m.as_string(); }
    inline void from_json(const json & archive, entity & m) { m = entity(archive.get<std::string>()); }
}

namespace polymer
{

    ////////////////////////
    //   base_component   //
    ////////////////////////

    class base_component
    {
    public:
        base_component() = default;
        virtual ~base_component() {}
    };
    POLYMER_SETUP_TYPEID(base_component);

    /////////////////////////////////////
    //   procedural_skybox_component   //
    /////////////////////////////////////

    struct procedural_skybox_component : public base_component
    {
        polymer::gl_hosek_sky sky;
        entity sun_directional_light {kInvalidEntity};
        procedural_skybox_component() {};
        virtual ~procedural_skybox_component() {};
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

    ///////////////////////////////
    //   point_light_component   //
    ///////////////////////////////

    struct point_light_component : public base_component
    {
        bool enabled{ true };
        uniforms::point_light data;
        point_light_component() {};
        virtual ~point_light_component() {};
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
        virtual ~directional_light_component() {};
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

    ///////////////////////
    //   ibl_component   //
    ///////////////////////

    struct ibl_component : public base_component
    {
        texture_handle ibl_radianceCubemap{ "default-radiance-cubemap" };
        texture_handle ibl_irradianceCubemap{ "default-irradiance-cubemap" };
        bool force_draw{ false };
        ibl_component() {};
        virtual ~ibl_component() {};
    };
    POLYMER_SETUP_TYPEID(ibl_component);

    template<class F> void visit_fields(ibl_component & o, F f) {
        f("ibl_radiance_cubemap", o.ibl_radianceCubemap);
        f("ibl_irradiance_cubemap", o.ibl_irradianceCubemap);
        f("force_draw", o.force_draw);
    }

    inline void to_json(json & j, const ibl_component & p) {
        visit_fields(const_cast<ibl_component&>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({ name, field }); });
    }

    inline void from_json(const json & archive, ibl_component & m) {
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
        mesh_component(gpu_mesh_handle handle) : mesh(handle) {}
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
    //   geometry_component   //
    ////////////////////////////

    // CPU-side runtime_mesh
    struct geometry_component : public base_component
    {
        cpu_mesh_handle geom;
        cpu_mesh_handle proxy_geom;
        bool is_static {true};
        geometry_component() {};
        geometry_component(cpu_mesh_handle handle) : geom(handle) {}
    };
    POLYMER_SETUP_TYPEID(geometry_component);

    template <class F>
    void visit_fields(geometry_component & o, F f)
    {
        f("cpu_mesh_handle", o.geom);
        f("cpu_mesh_proxy_handle", o.proxy_geom);
        f("is_static", o.is_static);
    }

    ////////////////////////////
    //   material_component   //
    ////////////////////////////

    struct uniform_override_t
    {
        std::unordered_map<std::string, polymer::uniform_variant_t> table;
    };
    POLYMER_SETUP_TYPEID(uniform_override_t);

    template<class F> void visit_fields(uniform_override_t & o, F f) {
        for (auto & uniform : o.table)
        {
            if (auto * val = nonstd::get_if<polymer::property<int>>(&uniform.second))    f(uniform.first.c_str(), (*val).raw());
            if (auto * val = nonstd::get_if<polymer::property<float>>(&uniform.second))  f(uniform.first.c_str(), (*val).raw());
            if (auto * val = nonstd::get_if<polymer::property<float2>>(&uniform.second)) f(uniform.first.c_str(), (*val).raw());
            if (auto * val = nonstd::get_if<polymer::property<float3>>(&uniform.second)) f(uniform.first.c_str(), (*val).raw());
            if (auto * val = nonstd::get_if<polymer::property<float4>>(&uniform.second)) f(uniform.first.c_str(), (*val).raw());
        }
    }

    inline void to_json(json & j, const uniform_override_t & p) {
        visit_fields(const_cast<uniform_override_t&>(p), [&j](const char * name, auto & field, auto... metadata) 
        {   
            j[name] = field;
        });
    }

    inline void from_json(const json & archive, uniform_override_t & m) {

        for (auto iter = archive.begin(); iter != archive.end(); ++iter)
        {
            try 
            { 
                m.table[iter.key().c_str()] = polymer::property<int>(iter.value().get<int>());
                m.table[iter.key().c_str()] = polymer::property<float>(iter.value().get<float>());
                m.table[iter.key().c_str()] = polymer::property<float2>(iter.value().get<float2>());
                m.table[iter.key().c_str()] = polymer::property<float3>(iter.value().get<float3>());
                m.table[iter.key().c_str()] = polymer::property<float4>(iter.value().get<float4>());
            }
            catch (const std::exception & e) 
            { 
                log::get()->import_log->info("{} uniform_override", e.what()); 
            }
        }
    };

    struct material_component : public base_component
    {
        material_handle material {material_library::kDefaultMaterialId};
        bool receive_shadow {true};
        bool cast_shadow {true};
        uniform_override_t override_table;
        material_component() = default;
        material_component(material_handle handle) : material(handle) {}
    };
    POLYMER_SETUP_TYPEID(material_component);

    template <class F>
    void visit_fields(material_component & o, F f)
    {
        f("material_handle", o.material);
        f("cast_shadow", o.cast_shadow);
        f("receive_shadow", o.receive_shadow);
        f("uniform_overrides", o.override_table);
    }

    inline void to_json(json & j, const material_component & p)
    {
        visit_fields(const_cast<material_component &>(p), [&j](const char * name, auto & field, auto... metadata) { j.push_back({name, field}); });
    }

    inline void from_json(const json & archive, material_component & m)
    {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            try { field = archive.at(name).get<std::remove_reference_t<decltype(field)>>(); }
            catch (const std::exception & e) { log::get()->import_log->info("{} not found in json", e.what()); } 
        });
    };

    /////////////////////////////
    //   transform_component   //
    /////////////////////////////

    class base_object;

    struct transform_component : public base_component
    {
        friend class scene_graph;
    private: 
        polymer::transform world_pose;
    public:
        polymer::transform local_pose;
        polymer::float3 local_scale {1, 1, 1};
        transform_component() {};
        transform_component(transform t, float3 s) : local_pose(t), local_scale(s) {};
        transform get_world_transform() const { return world_pose; }
        virtual ~transform_component() {};
    };
    POLYMER_SETUP_TYPEID(transform_component);

    //////////////////////////
    //   render_component   //
    //////////////////////////

    /// @todo - render_component submission groups
    // not serialized 
    struct render_component : public base_component
    {
        polymer::material_component * material{ nullptr };
        polymer::mesh_component * mesh{ nullptr };
        float4x4 world_matrix;
        uint32_t render_sort_order {0};
        render_component() {};
        virtual ~render_component() {};
    };
    POLYMER_SETUP_TYPEID(render_component);

    /////////////////////
    //   base_object   //
    /////////////////////

    // abstract base class?
    // callback: add, change, delete
    // dirty flag?
    // tags?
    // layers?

    class base_object
    {
        friend struct base_object_hash;
        friend class scene;  // for serialization and cloning operations to modify e directly
        friend class scene_graph;

        entity e {kInvalidEntity};

        entity parent {kInvalidEntity};
        std::vector<entity> children;

        transform_component transform;
        std::unordered_map<poly_typeid, std::shared_ptr<base_component>> components;

    public:

        bool enabled      = true;
        bool serializable = true;
        std::string name;

        base_object()
        {
            e = make_guid();
        }

        base_object(const std::string & name) : name(name)
        {
            e = make_guid();
        }

        entity get_entity() const { return e; }
        std::string get_name() const { return name; };

        void set_name(const std::string & n)
        {
            name = n;
        }

        template <typename T>
        void add_component(const T & component)
        {
            std::shared_ptr<T> shared = std::make_shared<T>();
            *shared = component;
            auto tid = get_typeid<T>();
            components[tid] = shared;
        }

        template <>
        void add_component(const transform_component & component)
        {
            transform = component;
        }

        template <typename T>
        void remove_component()
        {
            auto it = components.find(get_typeid<T>());
            if (it != components.end()) components.erase(it);
        }

        template <typename T>
        T * get_component()
        {
            auto it = components.find(get_typeid<T>());
            if (it != components.end())
            {
                auto & obj     = (it->second);
                auto component = dynamic_cast<T *>(obj.get());
                return component;
            }
            return nullptr;
        }

        template <>
        transform_component * get_component()
        {
            return &transform;
        }
    };

    // Hash functor so this can be used in unordered containers.
    struct base_object_hash
    {
        entity operator()(const base_object & c) const { return c.e; }
    };

    /////////////////////
    //   scene_graph   //
    /////////////////////

    class scene_graph
    {
        void recalculate_world_transform(entity child)
        {
            base_object & node = graph_objects[child];

            // If the node has a parent then we can compute a new world transform.
            // Note that during deserialization we might not have created the parent yet
            // so we are allowed to no-op given a null parent node
            if (node.parent != kInvalidEntity)
            {
                auto & parent_node = graph_objects[node.parent];
                node.transform.world_pose = parent_node.transform.local_pose * node.transform.local_pose;
            }
            else
            {
                // If the node has no parent, it should be considered already in world space.
                node.transform.world_pose = node.transform.local_pose;
            }

            // For each child, calculate its new world transform
            for (const entity & c : node.children) recalculate_world_transform(c);
        }

        void destroy_recursive(entity child, std::vector<entity> & destroyed_entities)
        {
            base_object & node = graph_objects[child];

            if (node.e != kInvalidEntity)
            {
                const auto children_copy = node.children;
                for (auto & n : children_copy)
                {
                    destroy_recursive(n, destroyed_entities);
                }
                if (node.parent != kInvalidEntity)
                {
                    // If this node has a parent, remove the child from its list
                    remove_child_from_parent(child);
                }
            }

            destroyed_entities.push_back(child);

            // Erase graph node
            graph_objects.erase(child);
        }

        // Resolve orphans. For instance, if we change the parent of an entity using the UI, it never gets added to
        // the list of children of the parent.
        void fix_parent_child_orphans()
        {
            for (auto & t : graph_objects)
            {
                const auto e = t.second.get_entity();
                auto parent = get_parent(e);
                if (parent != kInvalidEntity)
                {
                    // If we have a parent, check if its list of children has this entity
                    if (!has_child(parent, e))
                    {
                        std::cout << "found orphan relationship: " << parent << ", " << e << std::endl;
                        add_child(parent, e);
                    }
                } 
            }
        }

        // template <class F>
        // friend void visit_components(entity e, transform_system * system, F f);

    public:

        std::unordered_map<entity, base_object> graph_objects;

        scene_graph()  = default;
        ~scene_graph() = default;

        // need to get base_object ptr from entity
        // get entity from base_object ptr (rare)

        template <class T>
        void add_object(const T && object)
        {
            graph_objects[object.get_entity()] = std::move(object);
            // should we recalculate poses? 
        }

        // todo: use optional? 
        base_object & get_object(const entity & e) { return graph_objects[e]; }

        bool add_child(entity parent, entity child)
        {
            if (parent == child) throw std::invalid_argument("parent and child cannot be the same");
            /// if (parent == kInvalidEntity) throw std::invalid_argument("parent was invalid"); kInvalidEntity means no parent
            /// if (!has_transform(parent)) throw std::invalid_argument("parent has no transform component");
            if (child == kInvalidEntity) throw std::invalid_argument("child was invalid");

            if (parent != kInvalidEntity) graph_objects[parent].children.push_back(child);  // add to children list if not a top level root node

            graph_objects[child].parent = parent;

            if (parent != kInvalidEntity) recalculate_world_transform(parent);  // only recalc if there is a sub graph

            return true;
        }

        void insert_child(entity parent, entity child, uint32_t idx = -1)
        { 
            /* todo */
        }

        void move_child(entity child, uint32_t idx)
        { 
            /* todo */
        }

        entity get_parent(const entity & child)
        {
            if (child == kInvalidEntity) return kInvalidEntity;
            const auto & e = graph_objects[child];
            return e.parent;
        }

        bool has_child(const entity & parent, const entity & child) 
        {
            if (parent == kInvalidEntity) return false;
            if (child == kInvalidEntity) return false;

            bool found_child = false;
            const auto & e = graph_objects[parent];

                for (auto & one_child : e.children)
                {
                    if (one_child == child)
                    {
                        found_child = true;
                    }
                }
            return found_child;
        }

        void remove_child_from_parent(entity child)
        {
            if (child == kInvalidEntity) throw std::invalid_argument("entity was invalid");
            base_object & child_node = graph_objects[child];

            if (child_node.parent != kInvalidEntity)
            {
                base_object & parent_node = graph_objects[child_node.parent];
                parent_node.children.erase(std::remove(parent_node.children.begin(), parent_node.children.end(), child), parent_node.children.end());
                child_node.parent = kInvalidEntity;
                recalculate_world_transform(child);
            }
        }

        void destroy(entity e)
        {
            std::vector<entity> destroyed_entities;
            if (e == kInvalidEntity) throw std::invalid_argument("entity was invalid");
            destroy_recursive(e, destroyed_entities);
        }

        std::vector<entity> destroy_with_list(entity e)
        {
            if (e == kInvalidEntity) throw std::invalid_argument("entity was invalid");
            std::vector<entity> destroyed_entities;
            destroy_recursive(e, destroyed_entities);
            return destroyed_entities;
        }

        void refresh()
        {
            for (auto & t : graph_objects)
            {
                const auto e = t.second.get_entity();
                if (e != kInvalidEntity) recalculate_world_transform(e); 
            };
        }
    };

    // template <class F>
    // void visit_components(entity e, scene_graph * system, F f)
    // {
    //     local_transform_component * component = system->graph_objects.get(e);
    //     if (component != nullptr) f("transform component", *component);
    // 
    //     // while inspecting, we need to continuously recalculate based on potentially changed data
    //     system->fix_parent_child_orphans();
    //     system->recalculate_world_transform(e);
    // }

} // end namespace polymer

#endif // polymer_object_hpp

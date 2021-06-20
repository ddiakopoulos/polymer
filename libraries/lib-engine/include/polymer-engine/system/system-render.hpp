#pragma once

#ifndef polymer_render_system_hpp
#define polymer_render_system_hpp

#include "polymer-core/tools/geometry.hpp"

#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/ecs/typeid.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/system/system-transform.hpp"
#include "polymer-engine/system/system-identifier.hpp"
#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/scene.hpp"

namespace polymer
{
    
    ///////////////////////
    //   render_system   //
    ///////////////////////

    class render_system final : public base_system
    {
        friend class asset_resolver; // for private access to the components

        std::unordered_map<entity, mesh_component> meshes;
        std::unordered_map<entity, material_component> materials;
        std::unordered_map<entity, point_light_component> point_lights;
        directional_light_component directional_light;
        std::unordered_map<entity, uint32_t> render_priority;

        procedural_skybox_component the_procedural_skybox;
        cubemap_component the_cubemap;

        renderer_settings settings;
        std::unique_ptr<pbr_renderer> renderer;

        template<class F> friend void visit_components(entity e, render_system * system, F f);

        void initialize_procedural_skybox_and_sun(entity_system_manager * esm)
        {
            the_procedural_skybox = procedural_skybox_component(esm->create_entity());
            the_procedural_skybox.sun_directional_light = esm->create_entity();

            transform_system * transform_sys = dynamic_cast<transform_system *>(esm->get_system(get_typeid<transform_system>()));
            transform_sys->create(the_procedural_skybox.get_entity(), transform(), {});
            transform_sys->create(the_procedural_skybox.sun_directional_light, transform(), {});

            identifier_system * identifier_sys = dynamic_cast<identifier_system *>(esm->get_system(get_typeid<identifier_system>()));
            identifier_sys->create(the_procedural_skybox.get_entity(), "procedural-skybox");
            identifier_sys->create(the_procedural_skybox.sun_directional_light, "procedural-skybox-sun");

            // Setup the skybox; link internal parameters to a directional light entity owned by the render system. 
            the_procedural_skybox.sky.onParametersChanged = [this]
            {
                directional_light_component dir_light(the_procedural_skybox.sun_directional_light);
                dir_light.data.direction = the_procedural_skybox.sky.get_sun_direction();
                dir_light.data.color = float3(1.f, 1.f, 1.f);
                dir_light.data.amount = 1.f;
                create(the_procedural_skybox.sun_directional_light, std::move(dir_light));
            };

            // Set initial values on the skybox with the sunlight entity we just created
            the_procedural_skybox.sky.onParametersChanged();
        }

        void initialize_cubemap(entity_system_manager * esm)
        {
            the_cubemap = cubemap_component(esm->create_entity());

            transform_system * transform_sys = dynamic_cast<transform_system *>(esm->get_system(get_typeid<transform_system>()));
            transform_sys->create(the_cubemap.get_entity(), transform(), {});

            identifier_system * identifier_sys = dynamic_cast<identifier_system *>(esm->get_system(get_typeid<identifier_system>()));
            identifier_sys->create(the_cubemap.get_entity(), "ibl-cubemap");
        }

        void construct()
        {
            register_system_for_type(this, get_typeid<mesh_component>());
            register_system_for_type(this, get_typeid<material_component>());
            register_system_for_type(this, get_typeid<point_light_component>());
            register_system_for_type(this, get_typeid<directional_light_component>());
            register_system_for_type(this, get_typeid<procedural_skybox_component>());
            register_system_for_type(this, get_typeid<cubemap_component>());
            renderer.reset(new pbr_renderer(settings));
        }

    public:

        transform_system * xform_system{ nullptr };

        render_system(renderer_settings s, entity_system_manager * esm) 
            : base_system(esm), settings(s)
        {
            construct();
        }

        // We only need to create the skybox and cubemap sometimes, like a brand
        // new scene or a fully procedural application that doesn't use serialization.
        // If we import a serialized scene, components for these will be created
        // and associated already.
        render_system(renderer_settings s, entity_system_manager * esm, scene * the_scene)
            : base_system(esm), settings(s)
        {
            construct();

            initialize_procedural_skybox_and_sun(esm);
            initialize_cubemap(esm);

            the_scene->track_entity(directional_light.get_entity());
            the_scene->track_entity(the_procedural_skybox.get_entity());
            the_scene->track_entity(the_cubemap.get_entity());
        }

        pbr_renderer * get_renderer() { return renderer.get(); }

        void reconfigure(const renderer_settings new_settings)
        {
            settings = new_settings;
            renderer.reset(new pbr_renderer(settings));
        }

        mesh_component * get_mesh_component(entity e)
        {
            auto iter = meshes.find(e);
            if (iter != meshes.end()) return &iter->second;
            return nullptr;
        }

        material_component * get_material_component(entity e)
        {
            auto iter = materials.find(e);
            if (iter != materials.end()) return &iter->second;
            return nullptr;
        }

        point_light_component * get_point_light_component(entity e)
        {
            auto iter = point_lights.find(e);
            if (iter != point_lights.end()) return &iter->second;
            return nullptr;
        }

        directional_light_component * get_directional_light_component(entity e)
        {
            if (directional_light.get_entity() == e) return &directional_light;
            return nullptr;
        }

        procedural_skybox_component * get_procedural_skybox_component(entity e)
        {   
            if (the_procedural_skybox.get_entity() == e) return &the_procedural_skybox;
            return nullptr;
        }

        cubemap_component * get_cubemap_component(entity e)
        {
            if (the_cubemap.get_entity() == e) return &the_cubemap;
            return nullptr;
        }

        virtual bool create(entity e, poly_typeid hash, void * data, void *& out_data) override final
        { 
            if (hash == get_typeid<mesh_component>() && get_mesh_component(e) == nullptr) 
            {
                meshes[e] = *static_cast<mesh_component *>(data);
                out_data = &meshes[e];
                return true;
            }
            else if (hash == get_typeid<material_component>() && get_material_component(e) == nullptr)
            { 
                materials[e] = *static_cast<material_component *>(data); 
                out_data = &materials[e];
                return true;
            }
            else if (hash == get_typeid<point_light_component>() && get_point_light_component(e) == nullptr)
            { 
                point_lights[e] = *static_cast<point_light_component *>(data); 
                out_data = &point_lights[e];
                return true;
            }
            else if (hash == get_typeid<directional_light_component>() && get_directional_light_component(e) == nullptr)
            { 
                directional_light = *static_cast<directional_light_component *>(data); 
                out_data = &directional_light;
                return true; 
            }
            else if (hash == get_typeid<procedural_skybox_component>() && get_procedural_skybox_component(e) == nullptr)
            {
                the_procedural_skybox = *static_cast<procedural_skybox_component *>(data);
                out_data = &the_procedural_skybox;
                return true;
            }
            else if (hash == get_typeid<cubemap_component>())
            {
                the_cubemap = *static_cast<cubemap_component *>(data);
                out_data = &the_cubemap;
                return true;
            }
            return false;
        }

        virtual bool create(entity e, poly_typeid hash, void * data) override final
        {
            void * out_data = nullptr;
            return create(e, hash, data, out_data);
        }

        mesh_component * create(entity e, mesh_component && c) { meshes[e] = std::move(c); return &meshes[e]; }
        material_component * create(entity e, material_component && c) { materials[e] = std::move(c); return &materials[e]; }
        point_light_component * create(entity e, point_light_component && c) { point_lights[e] = std::move(c); return &point_lights[e]; }
        directional_light_component * create(entity e, directional_light_component && c) { directional_light = std::move(c); return &directional_light; }
        procedural_skybox_component * create(entity e, procedural_skybox_component && c) { the_procedural_skybox = std::move(c); return &the_procedural_skybox; }
        cubemap_component * create(entity e, cubemap_component && c) { the_cubemap = std::move(c); return &the_cubemap; }

        virtual void destroy(entity e) override final 
        {
            if (e == kAllEntities)
            {
                meshes.clear();
                materials.clear();
                point_lights.clear();
                directional_light = directional_light_component(kInvalidEntity);
                return;
            }

            auto meshIter = meshes.find(e);
            if (meshIter != meshes.end()) meshes.erase(meshIter);

            auto matIter = materials.find(e);
            if (matIter != materials.end()) materials.erase(matIter);

            auto ptLightIter = point_lights.find(e);
            if (ptLightIter != point_lights.end()) point_lights.erase(ptLightIter);

            if (the_procedural_skybox.get_entity() == e) the_procedural_skybox = {};
            if (the_cubemap.get_entity() == e) the_cubemap = {};
        }

        void set_render_priority(const entity e, const uint32_t priority)
        {
            render_priority[e] = priority;
        }

        uint32_t get_render_priority(const entity e)
        {
            auto priIter = render_priority.find(e);
            if (priIter != render_priority.end()) return priIter->second;
            return 0;
        }

        renderer_settings get_settings() const { return settings; }
    };
    POLYMER_SETUP_TYPEID(render_system);

    template<class F> void visit_components(entity e, render_system * system, F f)
    {
        if (system->the_procedural_skybox.get_entity() == e) f("procedural skybox component", system->the_procedural_skybox);
        if (system->the_cubemap.get_entity() == e) f("cubemap component", system->the_cubemap);
        if (auto ptr = system->get_mesh_component(e)) f("mesh component", *ptr);
        if (auto ptr = system->get_material_component(e)) f("material component", *ptr);
        if (auto ptr = system->get_point_light_component(e))
        {
            // hack hack
            transform_system * transform_sys = dynamic_cast<transform_system *>(system->esm->get_system(get_typeid<transform_system>()));
            auto pt_light_xform = transform_sys->get_world_transform(e);
            ptr->data.position = pt_light_xform->world_pose.position;
            f("point light component", *ptr);
        }
        if (auto ptr = system->get_directional_light_component(e)) f("directional light component", *ptr);
    }

} // end namespace polymer

#endif // end polymer_render_system_hpp

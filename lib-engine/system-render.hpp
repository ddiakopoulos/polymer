#pragma once

#ifndef polymer_render_system_hpp
#define polymer_render_system_hpp

#include "geometry.hpp"
#include "asset-handle-utils.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "environment.hpp"
#include "renderer-pbr.hpp"

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
        std::unordered_map<entity, directional_light_component> directional_lights;

        procedural_skybox_component the_procedural_skybox;
        cubemap_component the_cubemap;

        renderer_settings settings;
        std::unique_ptr<pbr_renderer> renderer;

        template<class F> friend void visit_components(entity e, render_system * system, F f);

        void initialize_procedural_skybox(entity_orchestrator * orch)
        {
            the_procedural_skybox = procedural_skybox_component(orch->create_entity());
            the_procedural_skybox.sun_directional_light = orch->create_entity();

            transform_system * transform_sys = dynamic_cast<transform_system *>(orch->get_system(get_typeid<transform_system>()));
            transform_sys->create(the_procedural_skybox.get_entity(), transform(), {});
            transform_sys->create(the_procedural_skybox.sun_directional_light, transform(), {});

            identifier_system * identifier_sys = dynamic_cast<identifier_system *>(orch->get_system(get_typeid<identifier_system>()));
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

        void initialize_cubemap(entity_orchestrator * orch)
        {
            the_cubemap = cubemap_component(orch->create_entity());

            transform_system * transform_sys = dynamic_cast<transform_system *>(orch->get_system(get_typeid<transform_system>()));
            transform_sys->create(the_cubemap.get_entity(), transform(), {});

            identifier_system * identifier_sys = dynamic_cast<identifier_system *>(orch->get_system(get_typeid<identifier_system>()));
            identifier_sys->create(the_cubemap.get_entity(), "ibl-cubemap");

            // wells-radiance-cubemap
            // wells-irradiance-cubemap

            // @fixme - to be resolved rather than hard-coded
            auto radianceBinary = read_file_binary("../assets/textures/envmaps/studio_radiance.dds");
            auto irradianceBinary = read_file_binary("../assets/textures/envmaps/studio_irradiance.dds");
            gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
            gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
            the_cubemap.ibl_radianceCubemap = create_handle_for_asset("default-radiance-cubemap", load_cubemap(radianceHandle));
            the_cubemap.ibl_irradianceCubemap = create_handle_for_asset("default-irradiance-cubemap", load_cubemap(irradianceHandle));
        }

    public:

        transform_system * xform_system{ nullptr };

        render_system(renderer_settings s, entity_orchestrator * orch) : base_system(orch), settings(s)
        {
            register_system_for_type(this, get_typeid<mesh_component>());
            register_system_for_type(this, get_typeid<material_component>());
            register_system_for_type(this, get_typeid<point_light_component>());
            register_system_for_type(this, get_typeid<directional_light_component>());
            register_system_for_type(this, get_typeid<procedural_skybox_component>());
            register_system_for_type(this, get_typeid<cubemap_component>());

            renderer.reset(new pbr_renderer(settings));

            initialize_procedural_skybox(orch);
            initialize_cubemap(orch);
        }

        pbr_renderer * get_renderer() { return renderer.get(); }

        void reconfigure(const renderer_settings new_settings)
        {
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
            auto iter = directional_lights.find(e);
            if (iter != directional_lights.end()) return &iter->second;
            return nullptr;
        }

        procedural_skybox_component * get_procedural_skybox() 
        {   
            if (the_procedural_skybox.get_entity() != kInvalidEntity) return &the_procedural_skybox;
            return nullptr;
        }

        cubemap_component * get_cubemap()
        {
            if (the_cubemap.get_entity() != kInvalidEntity) return &the_cubemap;
            return nullptr;
        }

        virtual bool create(entity e, poly_typeid hash, void * data) override final 
        { 
            if (hash == get_typeid<mesh_component>()) 
            {
                meshes[e] = *static_cast<mesh_component *>(data);
                return true;
            }
            else if (hash == get_typeid<material_component>()) 
            { 
                materials[e] = *static_cast<material_component *>(data); 
                return true;
            }
            else if (hash == get_typeid<point_light_component>()) 
            { 
                point_lights[e] = *static_cast<point_light_component *>(data); 
                return true;
            }
            else if (hash == get_typeid<directional_light_component>()) 
            { 
                directional_lights[e] = *static_cast<directional_light_component *>(data); 
                return true; 
            }
            else if (hash == get_typeid<procedural_skybox_component>())
            {
                the_procedural_skybox = std::move(*static_cast<procedural_skybox_component *>(data));
                return true;
            }
            else if (hash == get_typeid<cubemap_component>())
            {
                the_cubemap = std::move(*static_cast<cubemap_component *>(data));
                return true;
            }
            return false;
        }

        mesh_component * create(entity e, mesh_component && c) { meshes[e] = std::move(c); return &meshes[e]; }
        material_component * create(entity e, material_component && c) { materials[e] = std::move(c); return &materials[e]; }
        point_light_component * create(entity e, point_light_component && c) { point_lights[e] = std::move(c); return &point_lights[e]; }
        directional_light_component * create(entity e, directional_light_component && c) { directional_lights[e] = std::move(c); return &directional_lights[e]; }
        procedural_skybox_component * create(entity e, procedural_skybox_component && c) { the_procedural_skybox = std::move(c); return &the_procedural_skybox; }
        cubemap_component * create(entity e, cubemap_component && c) { the_cubemap = std::move(c); return &the_cubemap; }

        virtual void destroy(entity e) override final 
        {
            if (e == kAllEntities)
            {
                meshes.clear();
                materials.clear();
                point_lights.clear();
                directional_lights.clear();
                return;
            }

            auto meshIter = meshes.find(e);
            if (meshIter != meshes.end()) meshes.erase(meshIter);

            auto matIter = materials.find(e);
            if (matIter != materials.end()) materials.erase(matIter);

            auto ptLightIter = point_lights.find(e);
            if (ptLightIter != point_lights.end()) point_lights.erase(ptLightIter);

            auto dirLightIter = directional_lights.find(e);
            if (dirLightIter != directional_lights.end()) directional_lights.erase(dirLightIter);

            if (the_procedural_skybox.get_entity() == e) the_procedural_skybox = {};
            if (the_cubemap.get_entity() == e) the_cubemap = {};
        }
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
            transform_system * transform_sys = dynamic_cast<transform_system *>(system->orchestrator->get_system(get_typeid<transform_system>()));
            auto pt_light_xform = transform_sys->get_world_transform(e);
            ptr->data.position = pt_light_xform->world_pose.position;
            f("point light component", *ptr);
        }
        if (auto ptr = system->get_directional_light_component(e)) f("directional light component", *ptr);
    }

} // end namespace polymer

#endif // end polymer_render_system_hpp

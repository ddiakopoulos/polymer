#pragma once

#ifndef polymer_render_system_hpp
#define polymer_render_system_hpp

#include "geometry.hpp"
#include "asset-handle-utils.hpp"
#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"
#include "environment.hpp"
#include "renderer-pbr.hpp"

namespace polymer
{
    
    //////////////////////////
    //   collision system   //
    //////////////////////////

    class render_system final : public base_system
    {

        std::unordered_map<entity, mesh_component> meshes;
        std::unordered_map<entity, material_component> materials;
        std::unordered_map<entity, point_light_component> point_lights;
        std::unordered_map<entity, directional_light_component> directional_lights;

        renderer_settings settings;
        std::unique_ptr<pbr_renderer> renderer;

        friend class asset_resolver; // for private access to the components

    public:

        transform_system * xform_system{ nullptr };

        render_system(renderer_settings s, entity_orchestrator * orch) : base_system(orch), settings(s)
        {
            register_system_for_type(this, get_typeid<mesh_component>());
            register_system_for_type(this, get_typeid<material_component>());
            register_system_for_type(this, get_typeid<point_light_component>());
            register_system_for_type(this, get_typeid<directional_light_component>());

            renderer.reset(new pbr_renderer(settings));
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

        virtual bool create(entity e, poly_typeid hash, void * data) override final 
        { 
            if (hash == get_typeid<mesh_component>()) 
            {
                meshes[e] = *static_cast<mesh_component *>(data); return true;
            }
            else if (hash == get_typeid<material_component>()) 
            { 
                materials[e] = *static_cast<material_component *>(data); return true;
            }
            else if (hash == get_typeid<point_light_component>()) 
            { 
                point_lights[e] = *static_cast<point_light_component *>(data); return true;
            }
            else if (hash == get_typeid<directional_light_component>()) 
            { 
                directional_lights[e] = *static_cast<directional_light_component *>(data); return true; 
            }
            return false;
        }

        bool create(entity e, mesh_component && c) { meshes[e] = std::move(c); return true;  }
        bool create(entity e, material_component && c) { materials[e] = std::move(c); return true; }
        bool create(entity e, point_light_component && c) { point_lights[e] = std::move(c); return true; }
        bool create(entity e, directional_light_component && c) { directional_lights[e] = std::move(c); return true; }

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
        }
    };

    POLYMER_SETUP_TYPEID(render_system);

    template<class F> void visit_components(entity e, render_system * system, F f)
    {
        //auto iter = system->meshes.find(e);
        //if (iter != system->meshes.end()) f("geometry component (collision)", iter->second);
    }

} // end namespace polymer

#endif // end polymer_render_system_hpp

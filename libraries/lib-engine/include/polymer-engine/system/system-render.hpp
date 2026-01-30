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

    class render_system
    {
        friend class asset_resolver; // for private access to the components

        std::unordered_map<entity, uint32_t> render_priority;

        procedural_skybox_component the_procedural_skybox;
        ibl_component the_cubemap;

        renderer_settings settings;
        std::unique_ptr<pbr_renderer> renderer;

        // Setup the skybox callback to update the directional light when sky parameters change.
        // This should be called after the scene has created the procedural skybox and sun entities.
        void initialize_procedural_skybox_callback(scene * the_scene)
        {
            if (!the_scene) return;

            // Find the procedural skybox in the scene graph
            for (auto & [e, obj] : the_scene->get_graph().graph_objects)
            {
                if (auto * skybox = obj.get_component<procedural_skybox_component>())
                {
                    // Setup the callback to update the sun direction
                    skybox->sky.onParametersChanged = [the_scene, skybox]()
                    {
                        if (skybox->sun_directional_light == kInvalidEntity) return;

                        // Use scene::get_object which safely returns nullptr for invalid entities
                        if (base_object * sun_obj = the_scene->get_object(skybox->sun_directional_light))
                        {
                            if (auto * dir_light = sun_obj->get_component<directional_light_component>())
                            {
                                dir_light->data.direction = skybox->sky.get_sun_direction();
                                dir_light->data.color = float3(1.f, 1.f, 1.f);
                                dir_light->data.amount = 1.f;
                            }
                        }
                    };

                    // Set initial values
                    skybox->sky.onParametersChanged();
                    break;
                }
            }
        }

    public:

        render_system(renderer_settings s) : settings(s)
        {
            renderer.reset(new pbr_renderer(settings));
        }

        // We only need to create the skybox and cubemap sometimes, like a brand
        // new scene or a fully procedural application that doesn't use serialization.
        // If we import a serialized scene, components for these will be created
        // and associated already.
        render_system(renderer_settings s, scene * the_scene) : settings(s)
        {
            //initialize_procedural_skybox_and_sun();
        }

        pbr_renderer * get_renderer() { return renderer.get(); }

        void reconfigure(const renderer_settings new_settings)
        {
            settings = new_settings;
            renderer.reset(new pbr_renderer(settings));
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

} // end namespace polymer

#endif // end polymer_render_system_hpp

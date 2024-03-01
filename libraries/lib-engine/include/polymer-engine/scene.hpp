#pragma once

#ifndef polymer_scene_hpp
#define polymer_scene_hpp

#include <memory>

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-gfx-gl/gl-mesh-util.hpp"

#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/renderer/renderer-procedural-sky.hpp"
#include "polymer-engine/renderer/renderer-uniforms.hpp"

#include "polymer-engine/asset/asset-import.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include "polymer-engine/material.hpp"
#include "polymer-engine/material-library.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"

#include "polymer-engine/ecs/typeid.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/ecs/core-events.hpp"

#include "polymer-core/tools/geometry.hpp"
#include "polymer-core/tools/camera.hpp"

#include "polymer-engine/system/system-collision.hpp"

using namespace polymer;

namespace polymer
{
    ///////////////
    //   scene   //
    ///////////////

    class scene
    {
        scene_graph graph;

        procedural_skybox_component the_procedural_skybox;
        ibl_component the_cubemap;

        renderer_settings settings;
        std::unique_ptr<pbr_renderer> renderer;
        std::unique_ptr<collision_system> the_collision_system;

    public:

        scene()
        {
            renderer_settings settings;
            settings.renderSize = {1920, 1080}; // FIXME
            renderer.reset(new pbr_renderer(settings));
            the_collision_system.reset(new collision_system(graph));
        }

        std::unique_ptr<polymer::material_library> mat_library;
        std::unique_ptr<polymer::event_manager_async> event_manager;
        std::unique_ptr<polymer::asset_resolver> resolver;

        void import_environment(const std::string & path);
        void export_environment(const std::string & path);
     
        void copy(entity src, entity dest);
        void destroy(entity e);

        pbr_renderer * get_renderer() { return renderer.get(); }
        collision_system * get_collision_system() { return the_collision_system.get(); }

        scene_graph & get_graph() { return graph; }

        void reset(int2 default_renderer_resolution, bool create_default_entities = false);
    };

} // end namespace polymer

#endif // end polymer_scene_hpp

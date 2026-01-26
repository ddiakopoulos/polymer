#pragma once

#ifndef polymer_scene_hpp
#define polymer_scene_hpp

#include <memory>

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-gfx-gl/gl-mesh-util.hpp"

#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/renderer/renderer-procedural-sky.hpp"
#include "polymer-engine/renderer/renderer-uniforms.hpp"

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
            // Note: renderer and collision_system are created in reset()
            // because OpenGL context must be current first
            graph.set_scene(this);  // Wire up scene pointer
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

        // Called every frame to update all objects
        void update(float delta_time);

        // Convenience object access
        base_object * get_object(const entity & e);

        // Factory methods (returns reference to object in scene)
        base_object & instantiate(base_object && obj);

        base_object & instantiate_mesh(
            const std::string & name,
            const transform & pose,
            const float3 & scale,
            const std::string & mesh_name,
            const std::string & material_name = material_library::kDefaultMaterialId);

        base_object & instantiate_empty(
            const std::string & name,
            const transform & pose = transform(),
            const float3 & scale = float3(1, 1, 1));

        base_object & instantiate_point_light(
            const std::string & name,
            const float3 & position,
            const float3 & color,
            float radius);
    };

} // end namespace polymer

#endif // end polymer_scene_hpp

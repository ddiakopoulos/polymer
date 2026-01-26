#pragma once

#ifndef polymer_collision_system_hpp
#define polymer_collision_system_hpp

#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/ecs/typeid.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-engine/object.hpp"

#include "polymer-core/tools/bvh.hpp"
#include "polymer-core/tools/geometry.hpp"

#include <algorithm>

namespace polymer
{
    struct entity_hit_result
    {
        entity e {kInvalidEntity};
        raycast_result r;
    };

    //////////////////////////
    //   collision system   //
    //////////////////////////
    
    enum class raycast_type
    {
        mesh,
        box,
    };

    class collision_system
    {
        template<class F> friend void visit_components(entity e, collision_system * system, F f);
        friend class asset_resolver;

    public:

        std::unique_ptr<bvh_tree> static_accelerator;
        std::unique_ptr<bvh_tree> dynamic_accelerator; // unimplemented 

        std::vector<bvh_node_data> collidable_objects;

        scene_graph & graph;
        std::vector<entity> collidable_entities;

        collision_system(scene_graph & g) : graph(g) { }

        void add_collidable(const entity & e)
        {
            // Prevent duplicate registration (idempotent)
            if (std::find(collidable_entities.begin(), collidable_entities.end(), e)
                == collidable_entities.end())
            {
                collidable_entities.push_back(e);
                queue_acceleration_rebuild();
            }
        }

        void remove_collidable(const entity & e)
        {
            auto it = std::find(collidable_entities.begin(), collidable_entities.end(), e);
            if (it != collidable_entities.end())
            {
                collidable_entities.erase(it);
                queue_acceleration_rebuild();
            }
        }

        entity_hit_result raycast(const ray & world_ray, const raycast_type type = raycast_type::mesh)
        {
            setup_acceleration();

            auto raycast_mesh = [&](entity e) -> raycast_result
            {
                auto & obj = graph.get_object(e);
                geometry_component * geom_component = obj.get_component<geometry_component>();
                transform_component * xform_component = obj.get_component<transform_component>();

                const runtime_mesh & geometry = geom_component->geom.get();
                if (geometry.vertices.empty()) return {};

                const transform meshPose = xform_component->get_world_transform();
                const float3 meshScale  = xform_component->local_scale;

                ray localRay = meshPose.inverse() * world_ray;
                localRay.origin /= meshScale;
                localRay.direction /= meshScale;
                float outT = 0.0f;
                float3 outNormal = { 0, 0, 0 };
                float2 outUv = { -1, -1 };
                const bool hit = intersect_ray_mesh(localRay, geometry, &outT, &outNormal, &outUv);
                return { hit, outT, outNormal, outUv };
            };

            // Raycasting onto a mesh is a two-step process. First, we find all the bounding boxes that
            // collide with the ray, and then refine by testing each of the meshes in them for the best
            // result (because the aabb isn't a tight fit). 
            entity hit_entity = kInvalidEntity;
            raycast_result out_result;
            std::vector<std::pair<bvh_node_data*, float>> box_hit_results;
            if (static_accelerator->intersect(world_ray, box_hit_results))
            {
                float mesh_best_t = std::numeric_limits<float>::max();

                for (auto & box_hit : box_hit_results)
                {
                    entity * e = static_cast<entity *>(box_hit.first->user_data);
                    const raycast_result the_raycast = raycast_mesh(*e);

                    if (the_raycast.hit)
                    {
                        if (the_raycast.distance < mesh_best_t)
                        {
                            mesh_best_t = the_raycast.distance;
                            out_result = the_raycast;
                            hit_entity  = *e;
                        }
                    }
                }
            }

            if (out_result.hit) { return { hit_entity, out_result }; }
            else return { kInvalidEntity, raycast_result() };
        }

        void queue_acceleration_rebuild()
        {
            static_accelerator.reset();
            dynamic_accelerator.reset();
        }

        void setup_acceleration()
        {
            if (!static_accelerator)
            {
                static_accelerator.reset(new bvh_tree());

                // @todo: this is parallelizable
                for (auto & e : collidable_entities)
                {
                    auto & obj = graph.get_object(e);
                    geometry_component * geom_component = obj.get_component<geometry_component>();
                    transform_component * xform_component = obj.get_component<transform_component>();

                    const geometry & geom = geom_component->geom.get();
                     
                    const transform geom_transform = xform_component->get_world_transform();
                    const float3 geom_scale = xform_component->local_scale;

                    // Construct a world-space axis-aligned box that encompasses the rotated and
                    // scaled position of the mesh
                    std::vector<float3> verts = geom.vertices;
                    for (auto & v : verts)
                    {
                        v *= geom_scale;
                        v = geom_transform.transform_coord(v);
                    }
                    const auto world_bounds = compute_bounds(verts);

                    bvh_node_data node;
                    node.bounds = world_bounds;
                    node.user_data = &e;
                    collidable_objects.push_back(node);
                }

                for (int i = 0; i < collidable_objects.size(); ++i)
                {
                    static_accelerator->add(&collidable_objects[i]);
                }

                static_accelerator->build();
            }
        }

        std::vector<entity> get_visible_entities(const frustum & camera_frustum)
        {
            setup_acceleration();

            auto visible_bvh_nodes = static_accelerator->find_visible_nodes(camera_frustum);

            std::vector<entity> visible_entities;
            for (const bvh_node_data * n : visible_bvh_nodes)
            {
                entity * e = static_cast<entity *>(n->user_data);
                visible_entities.push_back(*e);
            }

            return visible_entities;
        }
    };

} // end namespace polymer

#endif // end polymer_collision_system_hpp

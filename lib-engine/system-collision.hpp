#pragma once

#ifndef polymer_collision_system_hpp
#define polymer_collision_system_hpp

#include "geometry.hpp"
#include "asset-handle-utils.hpp"
#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"
#include "environment.hpp"
#include "bvh.hpp"

namespace polymer
{
    
    //////////////////////////
    //   collision system   //
    //////////////////////////
    
    enum class raycast_type
    {
        mesh,
        box,
    };

    class collision_system final : public base_system
    {
        std::unordered_map<entity, geometry_component> meshes;
        transform_system * xform_system{ nullptr };

        template<class F> friend void visit_components(entity e, collision_system * system, F f);
        friend class asset_resolver;

    public:

        std::unique_ptr<bvh_tree> scene_accelerator;
        std::vector<scene_object> bvh_objects;

        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, get_typeid<geometry_component>());
        }

        entity_hit_result raycast(const ray & world_ray, const raycast_type type = raycast_type::mesh)
        {
            setup_acceleration();

            auto raycast_mesh = [&](entity e) -> raycast_result
            {
                if (!xform_system->has_transform(e)) return {};
                const runtime_mesh & geometry = meshes[e].geom.get();
                if (geometry.vertices.empty()) return {};

                const transform meshPose = xform_system->get_world_transform(e)->world_pose;
                const float3 meshScale = xform_system->get_local_transform(e)->local_scale;

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
            std::vector<std::pair<scene_object*, float>> box_hit_results;
            if (scene_accelerator->intersect(world_ray, box_hit_results))
            {
                float mesh_best_t = std::numeric_limits<float>::max();

                for (auto & box_hit : box_hit_results)
                {
                    geometry_component * gc = static_cast<geometry_component *>(box_hit.first->user_data);
                    const raycast_result the_raycast = raycast_mesh(gc->get_entity());

                    if (the_raycast.hit)
                    {
                        if (the_raycast.distance < mesh_best_t)
                        {
                            mesh_best_t = the_raycast.distance;
                            out_result = the_raycast;
                            hit_entity = gc->get_entity();
                        }
                    }
                }
            }

            if (out_result.hit) { return { hit_entity, out_result }; }
            else return { kInvalidEntity, raycast_result() };
        }

        virtual bool create(entity e, poly_typeid hash, void * data) override final 
        { 
            if (hash != get_typeid<geometry_component>()) { return false; }
            meshes[e] = *static_cast<geometry_component *>(data);
            return true;
        }
        
        bool create(entity e, geometry_component && c)
        {
            meshes[e] = std::move(c);
            return true;
        }

        geometry_component * get_component(entity e)
        {
            auto iter = meshes.find(e);
            if (iter != meshes.end()) return &iter->second;
            return nullptr;
        }

        virtual void destroy(entity e) override final 
        {
            auto iter = meshes.find(e);
            if (iter != meshes.end()) meshes.erase(e);
        }

        void queue_acceleration_rebuild()
        {
            scene_accelerator.reset();
        }

        void setup_acceleration()
        {
            if (!scene_accelerator)
            {
                if (!xform_system)
                {
                    base_system * xform_base = orchestrator->get_system(get_typeid<transform_system>());
                    xform_system = dynamic_cast<transform_system *>(xform_base);
                    assert(xform_system != nullptr);
                }

                scene_accelerator.reset(new bvh_tree());

                /// @todo - parallelizable
                for (auto & m : meshes)
                {
                    const geometry & geom = m.second.geom.get();
                     
                    auto local_scale = xform_system->get_local_transform(m.second.get_entity())->local_scale;
                    auto world_transform = xform_system->get_world_transform(m.second.get_entity())->world_pose;

                    // Construct a world-space axis-aligned box that encompasses the rotated and
                    // scaled position of the mesh
                    std::vector<float3> verts = geom.vertices;
                    for (auto & v : verts)
                    {
                        v *= local_scale;
                        v = world_transform.transform_coord(v);
                    }
                    const auto world_bounds = compute_bounds(verts);

                    scene_object obj;
                    obj.bounds = world_bounds;
                    obj.user_data = &m.second;
                    bvh_objects.emplace_back(std::move(obj));
                }

                for (int i = 0; i < bvh_objects.size(); ++i)
                {
                    scene_accelerator->add(&bvh_objects[i]);
                }

                scene_accelerator->build();
            }
        }

        std::vector<entity> get_visible_entities(const frustum & camera_frustum)
        {
            setup_acceleration();

            auto visible_bvh_nodes = scene_accelerator->find_visible_nodes(camera_frustum);

            std::vector<entity> visible_entities;
            for (auto & n : visible_bvh_nodes)
            {
                geometry_component * gc = static_cast<geometry_component *>(n->user_data);
                visible_entities.push_back(gc->get_entity());
            }

            return visible_entities;
        }
    };
    POLYMER_SETUP_TYPEID(collision_system);

    template<class F> void visit_components(entity e, collision_system * system, F f)
    {
        auto iter = system->meshes.find(e);
        if (iter != system->meshes.end()) f("geometry component", iter->second);
    }

} // end namespace polymer

#endif // end polymer_collision_system_hpp

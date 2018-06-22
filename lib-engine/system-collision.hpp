#pragma once

#ifndef polymer_collision_system_hpp
#define polymer_collision_system_hpp

#include "geometry.hpp"
#include "asset-handle-utils.hpp"
#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"
#include "environment.hpp"

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

    // todo - need to support proxy mesh (sphere or box)
    // todo - accelerate using spatial data structure (use existing octree?)
    class collision_system final : public base_system
    {
        std::unordered_map<entity, geometry_component> meshes;
        transform_system * xform_system{ nullptr };

        template<class F> friend void visit_components(entity e, collision_system * system, F f);
        friend class asset_resolver;

    public:

        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, get_typeid<geometry_component>());
        }

        entity_hit_result raycast(const ray & world_ray, const raycast_type type = raycast_type::mesh)
        {
            // Potential cross-orchestrator issues here
            if (!xform_system)
            {
                base_system * xform_base = orchestrator->get_system(get_typeid<transform_system>());
                xform_system = dynamic_cast<transform_system *>(xform_base);
                assert(xform_system != nullptr);
            }

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

            auto raycast_box = [&](entity e) -> raycast_result
            {
                if (!xform_system->has_transform(e)) return {};
                const runtime_mesh & geometry = meshes[e].geom.get();
                if (geometry.vertices.empty()) return {};

                const transform meshPose = xform_system->get_world_transform(e)->world_pose; // store bounds?
                const float3 meshScale = xform_system->get_local_transform(e)->local_scale;

                ray r = meshPose.inverse() * world_ray;
                r.origin /= meshScale;
                r.direction /= meshScale;

                const aabb_3d mesh_bounds = compute_bounds(geometry);
                float outMinT, outMaxT;
                const bool hit = intersect_ray_box(r, mesh_bounds.min(), mesh_bounds.max(), &outMinT, &outMaxT);
                return raycast_result(hit, outMaxT, {}, {});
            };

            float best_t = std::numeric_limits<float>::max();
            entity hit_entity = kInvalidEntity;
            raycast_result out_result;

            // fixme - we really need an environment-wide spatial data structure for this
            if (type == raycast_type::mesh)
            {
                for (const auto & mesh : meshes)
                {
                    if (mesh.first == kInvalidEntity) continue;

                    raycast_result res = raycast_mesh(mesh.first);
                    if (res.hit)
                    {
                        if (res.distance < best_t)
                        {
                            best_t = res.distance;
                            hit_entity = mesh.first;
                            out_result = res;
                        }
                    }
                }
            }
            else if (type == raycast_type::box)
            {
                for (const auto & mesh : meshes)
                {
                    if (mesh.first == kInvalidEntity) continue;

                    raycast_result res = raycast_box(mesh.first);
                    if (res.hit)
                    {
                        if (res.distance < best_t)
                        {
                            best_t = res.distance;
                            hit_entity = mesh.first;
                            out_result = res;
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
    };
    POLYMER_SETUP_TYPEID(collision_system);

    template<class F> void visit_components(entity e, collision_system * system, F f)
    {
        auto iter = system->meshes.find(e);
        if (iter != system->meshes.end()) f("geometry component", iter->second);
    }

} // end namespace polymer

#endif // end polymer_collision_system_hpp

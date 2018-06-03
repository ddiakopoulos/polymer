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

        entity_hit_result raycast(const ray & worldRay)
        {
            // Potential cross-orchestrator issues here
            if (!xform_system)
            {
                base_system * xform_base = orchestrator->get_system(get_typeid<transform_system>());
                xform_system = dynamic_cast<transform_system *>(xform_base);
                assert(xform_system != nullptr);
            }

            auto raycast = [&](entity e) ->raycast_result
            {
                const transform meshPose = xform_system->get_world_transform(e)->world_pose;
                const float3 meshScale = xform_system->get_local_transform(e)->local_scale;
                const runtime_mesh & geometry = meshes[e].geom.get();

                ray localRay = meshPose.inverse() * worldRay;
                localRay.origin /= meshScale;
                localRay.direction /= meshScale;
                float outT = 0.0f;
                float3 outNormal = { 0, 0, 0 };
                float2 outUv = { -1, -1 };
                const bool hit = intersect_ray_mesh(localRay, geometry, &outT, &outNormal, &outUv);
                return{ hit, outT, outNormal, outUv };
            };

            float best_t = std::numeric_limits<float>::max();
            raycast_result result;
            entity hit_entity = kInvalidEntity;

            for (auto & mesh : meshes)
            {
                if (mesh.first == kInvalidEntity) continue;

                result = raycast(mesh.first);
                if (result.hit)
                {
                    if (result.distance < best_t)
                    {
                        best_t = result.distance;
                        hit_entity = mesh.first;
                    }
                }
            }

            entity_hit_result out_result;
            out_result.e = hit_entity;
            out_result.r = result;
            return out_result;
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

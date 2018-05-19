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
    // Collision System
    class collision_system final : public base_system
    {
    public:
        std::unordered_map<entity, geometry_component> meshes;
        transform_system * xform_system{ nullptr };

        collision_system(entity_orchestrator * orch) : base_system(orch)
        {
            register_system_for_type(this, hash(get_typename<geometry_component>()));
        }

        raycast_result raycast(const entity e, const ray & worldRay)
        {
            if (!xform_system)
            {
                base_system * xform_base = orchestrator->get_system(get_typeid<transform_system>());
                xform_system = dynamic_cast<transform_system *>(xform_base);
                assert(xform_system != nullptr);
            }

            const transform meshPose = xform_system->get_world_transform(e)->world_pose;
            const float3 meshScale = xform_system->get_local_transform(e)->local_scale;
            const runtime_mesh & geometry = meshes[e].geom.get();

            ray localRay = meshPose.inverse() * worldRay;
            localRay.origin /= meshScale;
            localRay.direction /= meshScale;
            float outT = 0.0f;
            float3 outNormal = { 0, 0, 0 };
            const bool hit = intersect_ray_mesh(localRay, geometry, &outT, &outNormal);
            return{ hit, outT, outNormal };
        }

        virtual bool create(entity e, poly_typeid hash, void * data) override final { return true; }
        virtual void destroy(entity e) override final {}
    };
    POLYMER_SETUP_TYPEID(collision_system);

    template<class F> void visit_component_fields(entity e, collision_system * system, F f)
    {
        auto iter = system->meshes.find(e);
        if (iter != system->meshes.end()) f("geometry_component", iter->second);
    }

} // end namespace polymer

#endif // end polymer_collision_system_hpp

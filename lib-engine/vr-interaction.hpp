#pragma once

#ifndef polymer_vr_interaction_hpp
#define polymer_vr_interaction_hpp

#include "gl-imgui.hpp"
#include "environment.hpp"
#include "ecs/core-events.hpp"
#include "material.hpp"
#include "openvr-hmd.hpp"

namespace polymer
{
    class imgui_vr : public gui::imgui_surface
    {
        entity imgui_billboard;
        entity pointer;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool should_draw_pointer{ false };
    public:
        imgui_vr(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window);
        void update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state);
        void update_renderloop();
        entity get_pointer() const;
        entity get_billboard() const;
    };

    struct vr_teleport_event { float3 world_position; uint64_t frame_count; };
    POLYMER_SETUP_TYPEID(vr_teleport_event);

    class vr_teleport_system final : public base_system
    {
        pointer_data pointer;
        geometry nav_geometry;
        float3 target_location;
        entity teleportation_arc{ kInvalidEntity };
        mesh_component * cached_mesh{ nullptr };
        bool should_draw{ false };
        openvr_hmd * hmd{ nullptr };
    public:
        vr_teleport_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd);
        virtual bool create(entity e, poly_typeid hash, void * data) override final;
        virtual void destroy(entity e) override final;
        void update(const uint64_t current_frame);
        entity get_teleportation_arc();
    };
    POLYMER_SETUP_TYPEID(vr_teleport_system);

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp

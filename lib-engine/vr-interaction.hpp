#pragma once

#ifndef polymer_vr_interaction_hpp
#define polymer_vr_interaction_hpp

#include "gl-imgui.hpp"
#include "gl-gizmo.hpp"
#include "environment.hpp"
#include "ecs/core-events.hpp"
#include "material.hpp"
#include "openvr-hmd.hpp"
#include "renderer-pbr.hpp"


namespace polymer
{
    /// interface: pre-render, post-render, handle input
    /// render_component rather than assemble_renderable
    /// renderable submission groups
    /// renderable sort order

    enum class vr_event_t : uint32_t
    {
        focus_begin,
        focus_end,
        focus_update,
        press,
        release,
        cancel
    };

    enum class vr_input_source_t : uint32_t
    {
        left_controller,
        right_controller,
        left_hand,
        right_hand,
        tracker
    };

    struct vr_input_focus
    {
        entity target{ kInvalidEntity };
        ray focused_ray;
        vr_input_source_t source;
    };

    class vr_input_processor
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };

    public:

        vr_input_processor(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)
            : env(env), hmd(hmd)
        {

        }

    };

    enum class controller_render_style_t : uint32_t
    {
        laser,
        arc
    };

    // visual appearance of openvr controller
    // render as: arc, line
    // shaders + materials
    // lock/unlock focus for teleportation
    struct vr_controller_system
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };

    public:

        vr_controller_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)
            : env(env), hmd(hmd)
        {

        }
    };

    //////////////////////////
    //   vr_imgui_surface   //
    //////////////////////////

    class vr_imgui_surface : public gui::imgui_surface
    {
        entity imgui_billboard;
        entity pointer;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool should_draw_pointer{ false };
    public:
        vr_imgui_surface(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window);
        void update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state);
        void update_renderloop();
        entity get_pointer() const;
        entity get_billboard() const;
    };

    ////////////////////////////
    //   vr_teleport_system   //
    ////////////////////////////

    // todo - use event
    struct vr_teleport_event { float3 world_position; uint64_t frame_count; };
    POLYMER_SETUP_TYPEID(vr_teleport_event);

    class vr_teleport_system
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
        void update(const uint64_t current_frame);
        entity get_teleportation_arc();
    };

    //////////////////
    //   vr_gizmo   //
    //////////////////

    class vr_gizmo
    {
        entity gizmo_entity, pointer;
        std::shared_ptr<polymer_fx_material> gizmo_material;
        bool should_draw_pointer{ false };
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;
    public:
        vr_gizmo(entity_orchestrator * orch, environment * env, material_library * library);
        void handle_input(const app_input_event & e);
        void update(const view_data view);
        void render();
        entity get_pointer() const;
        entity get_gizmo() const;
    };

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp

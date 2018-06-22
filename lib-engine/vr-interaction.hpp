#pragma once

#ifndef polymer_vr_interaction_hpp
#define polymer_vr_interaction_hpp

#include "gl-imgui.hpp"
#include "gl-gizmo.hpp"
#include "ecs/core-events.hpp"
#include "material.hpp"
#include "openvr-hmd.hpp"
#include "renderer-pbr.hpp"

#include "environment.hpp"
#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"

#include <stack>

namespace polymer
{
    enum class vr_event_t : uint32_t
    {
        focus_begin,
        focus_end,
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

    struct vr_input_focus { ray r; entity_hit_result result; };
    inline bool operator != (const vr_input_focus & a, const vr_input_focus & b) { return (a.result.e != b.result.e); }
    inline bool operator == (const vr_input_focus & a, const vr_input_focus & b) { return (a.result.e == b.result.e); }

    struct vr_input_event
    {
        vr_event_t type;
        vr_input_source_t source;
        vr_input_focus focus;
        uint64_t timestamp;
        openvr_controller controller;
    };
    POLYMER_SETUP_TYPEID(vr_input_event);

    struct vr_teleport_event { float3 world_position; uint64_t timestamp; };
    POLYMER_SETUP_TYPEID(vr_teleport_event);

    inline vr_input_event make_event(vr_event_t t, vr_input_source_t s, vr_input_focus f, openvr_controller c)
    {
        return { t, s, f, system_time_ns(), c };
    }

    ////////////////////////////
    //   vr_input_processor   //
    ////////////////////////////

    /// The input processor polls the openvr system directly for updated controller input.
    /// This system dispatches vr_input_events through the environment's event manager
    /// with respect to button presses, releases, and focus events. Entity focus is presently 
    /// expensive because there is no scene-wide acceleration structure used for raycasting. 

    // todo - triple buffer vr_input_event state
    class vr_input_processor
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };

        vr::ETrackedControllerRole dominant_hand{ vr::TrackedControllerRole_RightHand };

        vr_input_focus last_focus;
        vr_input_focus recompute_focus(const openvr_controller & controller);

    public:

        vr_input_processor(entity_orchestrator * orch, environment * env, openvr_hmd * hmd);
        ~vr_input_processor();
        vr::ETrackedControllerRole get_dominant_hand() const;
        vr_input_focus get_focus() const;
        void process(const float dt);
    };
    POLYMER_SETUP_TYPEID(vr_input_processor);

    //////////////////////////////
    //   vr_controller_system   //
    //////////////////////////////

    /// The controller system is responsible for creating, updating, and drawing the
    /// state of openvr controllers. It also implements logic to draw a laser pointer
    /// or teleportation arc. This system additionally integrates code to teleport the
    /// user in the world, although this functionality might be refactored in the future. 

    enum class controller_render_style_t : uint32_t
    {
        invisible,
        laser,
        arc
    };

    class vr_controller_system
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        vr_input_processor * processor{ nullptr };

        mesh_component * pointer_mesh_component{ nullptr };
        entity pointer, left_controller, right_controller;
        arc_pointer_data arc_pointer;
        std::vector<float3> arc_curve;
        float3 target_location;

        std::stack<controller_render_style_t> render_styles;
        bool need_controller_render_data{ true };

        polymer::event_manager_sync::connection input_handler_connection;
        void handle_event(const vr_input_event & event);

    public:

        vr_controller_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, vr_input_processor * processor);
        ~vr_controller_system();
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };
    POLYMER_SETUP_TYPEID(vr_controller_system);

    //////////////////////////
    //   vr_imgui_surface   //
    //////////////////////////

    class vr_imgui_surface : public gui::imgui_surface
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        vr_input_processor * processor{ nullptr };

        entity imgui_billboard;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool focused{ false };
        void handle_event(const vr_input_event & event);
        polymer::event_manager_sync::connection input_handler_connection;

    public:

        vr_imgui_surface(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, vr_input_processor * processor, const uint2 size, GLFWwindow * window);
        ~vr_imgui_surface();
        void set_surface_transform(const transform & t);
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };
    POLYMER_SETUP_TYPEID(vr_imgui_surface);

    //////////////////
    //   vr_gizmo   //
    //////////////////

    class vr_gizmo
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        vr_input_processor * processor{ nullptr };

        entity gizmo_entity;
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;
        tinygizmo::rigid_transform xform;
        geometry transient_gizmo_geom;

        bool focused{ false };
        void handle_event(const vr_input_event & event);
        polymer::event_manager_sync::connection input_handler_connection;

    public:

        vr_gizmo(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, vr_input_processor * processor);
        ~vr_gizmo();
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };
    POLYMER_SETUP_TYPEID(vr_gizmo);

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp

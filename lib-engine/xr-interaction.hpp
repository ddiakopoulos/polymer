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

namespace polymer {
namespace xr {

    enum class vr_event_t : uint32_t
    {
        focus_begin,    // (dominant hand) when a hand enters the focus region of an entity
        focus_end,      // (dominant hand) leaving the focus region
        press,          // (either hand) for all button press events
        release,        // (either hand) for all button release events
        cancel          // (either hand) unimplemented
    };

    enum class vr_input_source_t : uint32_t
    {
        left_controller,
        right_controller,
        tracker
    };

    struct xr_input_focus { ray r; entity_hit_result result; };
    inline bool operator != (const xr_input_focus & a, const xr_input_focus & b) { return (a.result.e != b.result.e); }
    inline bool operator == (const xr_input_focus & a, const xr_input_focus & b) { return (a.result.e == b.result.e); }

    struct xr_input_event
    {
        vr_event_t type;
        vr_input_source_t source;
        xr_input_focus focus;
        uint64_t timestamp;
        openvr_controller controller;
    };

    struct xr_teleport_event 
    {
        float3 world_position; 
        uint64_t timestamp; 
    };

    inline xr_input_event make_event(vr_event_t t, vr_input_source_t s, const xr_input_focus & f, const openvr_controller & c)
    {
        return { t, s, f, system_time_ns(), c };
    }

    ////////////////////////////
    //   xr_input_processor   //
    ////////////////////////////

    /// The input processor polls the openvr system directly for updated controller input.
    /// This system dispatches vr_input_events through the environment's event manager
    /// with respect to button presses, releases, and focus events. Entity focus is presently 
    /// expensive because there is no scene-wide acceleration structure used for raycasting. 

    // todo - triple buffer xr_input_event state
    class xr_input_processor
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };

        vr::ETrackedControllerRole dominant_hand{ vr::TrackedControllerRole_RightHand };

        xr_input_focus last_focus;
        xr_input_focus recompute_focus(const openvr_controller & controller);

    public:

        xr_input_processor(entity_orchestrator * orch, environment * env, openvr_hmd * hmd);
        ~xr_input_processor();
        vr::ETrackedControllerRole get_dominant_hand() const;
        xr_input_focus get_focus() const;
        void process(const float dt);
    };

    //////////////////////////////
    //   xr_controller_system   //
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

    class xr_controller_system
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        mesh_component * pointer_mesh_component{ nullptr };
        entity pointer, left_controller, right_controller;
        arc_pointer_data arc_pointer;
        std::vector<float3> arc_curve;
        float3 target_location;

        std::stack<controller_render_style_t> render_styles;
        bool need_controller_render_data{ true };

        polymer::event_manager_sync::connection input_handler_connection;
        void handle_event(const xr_input_event & event);

    public:

        xr_controller_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, xr_input_processor * processor);
        ~xr_controller_system();
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };

    /////////////////////////
    //   xr_imgui_system   //
    /////////////////////////

    class xr_imgui_system : public gui::imgui_surface
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        entity imgui_billboard;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool focused{ false };
        void handle_event(const xr_input_event & event);
        polymer::event_manager_sync::connection input_handler_connection;

    public:

        xr_imgui_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, xr_input_processor * processor, const uint2 size, GLFWwindow * window);
        ~xr_imgui_system();
        void set_surface_transform(const transform & t);
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };

    /////////////////////////
    //   xr_gizmo_system   //
    /////////////////////////

    class xr_gizmo_system
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        entity gizmo_entity;
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;
        tinygizmo::rigid_transform xform;
        geometry transient_gizmo_geom;

        bool focused{ false };
        void handle_event(const xr_input_event & event);
        polymer::event_manager_sync::connection input_handler_connection;

    public:

        xr_gizmo_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, xr_input_processor * processor);
        ~xr_gizmo_system();
        std::vector<entity> get_renderables() const;
        void process(const float dt);
    };

} // end namespace xr

POLYMER_SETUP_TYPEID(polymer::xr::xr_input_processor);
POLYMER_SETUP_TYPEID(polymer::xr::xr_gizmo_system);
POLYMER_SETUP_TYPEID(polymer::xr::xr_imgui_system);
POLYMER_SETUP_TYPEID(polymer::xr::xr_controller_system);
POLYMER_SETUP_TYPEID(polymer::xr::xr_input_event);
POLYMER_SETUP_TYPEID(polymer::xr::xr_teleport_event);
POLYMER_SETUP_TYPEID(polymer::xr::xr_input_focus);

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp
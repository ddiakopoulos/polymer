#pragma once

#ifndef polymer_vr_interaction_hpp
#define polymer_vr_interaction_hpp

#include "gl-imgui.hpp"
#include "gl-gizmo.hpp"
#include "ecs/core-events.hpp"
#include "material.hpp"
#include "hmd-base.hpp"
#include "renderer-pbr.hpp"

#include "environment.hpp"
#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"

#include <stack>

namespace polymer {
namespace xr {

    // todo - enable/disable

    enum class xr_button_event : uint32_t
    {
        focus_begin, // (dominant hand) when a hand enters the focus region of an entity
        focus_end,   // (dominant hand) leaving the focus region
        press,       // (either hand) for all button press events
        release,     // (either hand) for all button release events
        cancel       // (either hand) unimplemented
    };

    // todo - this is partially redundant with vr_controller_role
    enum class vr_input_source_t : uint32_t
    {
        left_controller,
        right_controller,
        tracker
    };

    struct xr_input_focus { ray r; entity_hit_result result; bool soft{ false }; };
    inline bool operator != (const xr_input_focus & a, const xr_input_focus & b) { return (a.result.e != b.result.e); }
    inline bool operator == (const xr_input_focus & a, const xr_input_focus & b) { return (a.result.e == b.result.e); }

    struct xr_input_event
    {
        xr_button_event type;
        vr_input_source_t source; // todo - refactor for xr namespace
        xr_input_focus focus;
        uint64_t timestamp;
        vr_controller controller;
    };

    struct xr_teleport_event 
    {
        float3 world_position; 
        uint64_t timestamp; 
    };

    inline xr_input_event make_event(xr_button_event t, vr_input_source_t s, const xr_input_focus & f, const vr_controller & c)
    {
        return { t, s, f, system_time_ns(), c };
    }

    ////////////////////////////
    //   xr_input_processor   //
    ////////////////////////////

    /// The input processor polls the openvr system directly for updated controller input.
    /// This system dispatches `vr_input_events` through the environment's event manager
    /// with respect to button presses, releases, and focus events. Entity focus is presently 
    /// expensive because there is no scene-wide acceleration structure used for raycasting. 
    /// This class is also an abstraction over all input handling in `openvr_hmd` and should
    /// be used instead of an `openvr_hmd` instance directly.

    // todo - double buffer xr_input_event state for safety
    class xr_input_processor
    {
        environment * env{ nullptr };
        hmd_base * hmd{ nullptr };

        vr_controller_role dominant_hand{ vr_controller_role::right_hand };
        bool fixed_dominant_hand {false};

        xr_input_focus last_focus;
        xr_input_focus recompute_focus(const vr_controller & controller);

    public:

        xr_input_processor(entity_orchestrator * orch, environment * env, hmd_base * hmd);
        ~xr_input_processor();
        vr_controller_role get_dominant_hand() const;
        vr_controller get_controller(const vr_controller_role hand);
        xr_input_focus get_focus() const;
        void process(const float dt);

        // The dominant hand changes depending on which controller last pressed the primary trigger.
        // This function can pin the dominant hand. For instance, if we attach some 
        // UI to one hand, then this function will enable us to stop generating raycast/pointer events
        // if we press the trigger on that hand. 
        void set_fixed_dominant_hand(const vr_controller_role hand);
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
        laser_to_entity,
        laser_infinite,
        arc
    };

    class xr_controller_system
    {
        environment * env{ nullptr };
        hmd_base * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        std::shared_ptr<polymer_blinn_phong_standard> controller_material[2];
        std::shared_ptr<polymer_fx_material> laser_pointer_material;

        simple_animator animator;
        mesh_component * pointer_mesh_component{ nullptr };
        entity pointer, left_controller, right_controller;
        arc_pointer_data arc_pointer;
        std::vector<float3> arc_curve;
        float3 target_location;
        float3 laser_color {172.f/255.f, 54.f/255.f, 134.f/255.f};
        float laser_alpha{ 0.f };
        float laser_line_thickness{ 0.0075f };
        float laser_fade_seconds{ 0.25f };
        float laser_alpha_on_teleport{ 0.f };
        float laser_fixed_draw_distance{ 2.f };
        void update_laser_geometry(const float distance);
        std::stack<controller_render_style_t> render_styles;

        std::vector<entity> ignored_entities;

        polymer::event_manager_sync::connection xr_input;
        void handle_event(const xr_input_event & event);

    public:

        xr_controller_system(entity_orchestrator * orch, environment * env, hmd_base * hmd, xr_input_processor * processor);
        ~xr_controller_system();
        std::vector<entity> get_renderables() const;
        void process(const float dt);
        void add_focus_ignore(const entity ignored_entity); 
        const entity get_entity_for_controller(vr_controller_role role) const { return (role == vr_controller_role::left_hand ? left_controller : right_controller); }
    };

    /////////////////////////
    //   xr_imgui_system   //
    /////////////////////////

    class xr_imgui_system : public gui::imgui_surface
    {
        environment * env{ nullptr };
        hmd_base * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        entity imgui_billboard;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool focused{ false };

        polymer::event_manager_sync::connection xr_input;
        void handle_event(const xr_input_event & event);

    public:

        xr_imgui_system(entity_orchestrator * orch, environment * env, hmd_base * hmd, xr_input_processor * processor, const uint2 size, GLFWwindow * window);
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
        hmd_base * hmd{ nullptr };
        xr_input_processor * processor{ nullptr };

        entity gizmo_entity;
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;
        tinygizmo::rigid_transform xform;
        geometry transient_gizmo_geom;

        bool focused{ false };

        polymer::event_manager_sync::connection xr_input;
        void handle_event(const xr_input_event & event);

    public:

        xr_gizmo_system(entity_orchestrator * orch, environment * env, hmd_base * hmd, xr_input_processor * processor);
        ~xr_gizmo_system();
        std::vector<entity> get_renderables() const;
        void process(const float dt);

        void set_transform(const transform t);
        transform get_transform() const;
		void set_render_scale(const float scale = 1.f);
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

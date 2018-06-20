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

namespace polymer
{
    /// interface: pre-render, post-render, handle input
    /// render_source (get list of render components)
    /// render_source -> update (renderloop)

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

    struct vr_input_focus
    {
        ray r;
        entity_hit_result result;
    };

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

    // triple buffer input_event state
    // lock/unlock focus for teleportation
    class vr_input_processor
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        vr_input_focus last_focus;
        vr::ETrackedControllerRole dominant_hand{ vr::TrackedControllerRole_RightHand };

        inline vr_input_focus get_focus(const openvr_controller & controller)
        {
            const ray controller_ray = ray(controller.t.position, -qzdir(controller.t.orientation));
            const entity_hit_result box_result = env->collision_system->raycast(controller_ray, raycast_type::box);

            if (box_result.r.hit)
            {
                const entity_hit_result mesh_result = env->collision_system->raycast(controller_ray, raycast_type::mesh);
                // Refine if hit the mesh
                if (mesh_result.r.hit)
                {
                    return { controller_ray, mesh_result };
                }
                else
                {
                    // Otherwise a bounding box is still considered "in focus"
                    return { controller_ray, box_result };
                }
            }
        }

        vr_event_t type;
        vr_input_source_t source;
        vr_input_focus focus;
        uint64_t timestamp;
        openvr_controller controller;

    public:

        vr_input_processor(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)
            : env(env), hmd(hmd)
        {

        }

        vr::ETrackedControllerRole get_dominant_hand() const { return dominant_hand; }
        vr_input_focus get_focus() const { return last_focus; }

        void process(const float dt)
        {
            //scoped_timer t("vr_input_processor::process()");

            // Generate button events
            for (auto hand : { vr::TrackedControllerRole_LeftHand, vr::TrackedControllerRole_RightHand })
            {
                const openvr_controller controller = hmd->get_controller(hand);
                const vr_input_source_t src = (hand == vr::TrackedControllerRole_LeftHand) ? vr_input_source_t::left_controller : vr_input_source_t::right_controller;

                for (auto b : controller.buttons)
                {
                    if (b.second.pressed)
                    {
                        vr_input_event press = make_event(vr_event_t::press, src, focus, controller);
                        env->event_manager->send(press);

                        if (b.first == vr::EVRButtonId::k_EButton_SteamVR_Trigger)
                        {
                            dominant_hand = hand;
                        }

                        std::cout << "dispatching vr_event_t::press" << std::endl;
                    }
                    else if (b.second.released)
                    {
                        vr_input_event release = make_event(vr_event_t::release, src, focus, controller);
                        env->event_manager->send(release);
                        std::cout << "dispatching vr_event_t::release" << std::endl;
                    }
                }
            }

            // Generate focus events for the dominant hand. todo: this can be rate-limited
            {
                const openvr_controller controller = hmd->get_controller(dominant_hand);
                const vr_input_source_t src = (dominant_hand == vr::TrackedControllerRole_LeftHand) ? vr_input_source_t::left_controller : vr_input_source_t::right_controller;
                const vr_input_focus focus = get_focus(controller);

                // New focus, not invalid
                if (focus != last_focus && focus.result.e != kInvalidEntity)
                {
                    vr_input_event focus_gained = make_event(vr_event_t::focus_begin, src, focus, controller);
                    env->event_manager->send(focus_gained);
                    std::cout << "dispatching vr_event_t::focus_begin for entity" << focus.result.e << std::endl;

                    // todo - focus_end on old entity
                }

                // Last one valid, new one invalid
                if (last_focus.result.e != kInvalidEntity && focus.result.e == kInvalidEntity)
                {
                    vr_input_event focus_lost = make_event(vr_event_t::focus_end, src, last_focus, controller);
                    env->event_manager->send(focus_lost);
                    std::cout << "dispatching vr_event_t::focus_end" << std::endl;
                }

                last_focus = focus;
            }
        }
    };

    enum class controller_render_style_t : uint32_t
    {
        invisible,
        laser,
        arc
    };

    // visual appearance of openvr controller
    // render as: arc, line
    // shaders + materials
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
        controller_render_style_t style{ controller_render_style_t::laser };
        bool should_draw_pointer{ false };
        bool need_controller_render_data{ true };
        void set_visual_style(const controller_render_style_t new_style) { style = new_style; }
        polymer::event_manager_sync::connection input_handler_connection;
    public:

        vr_controller_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, vr_input_processor * processor)
            : env(env), hmd(hmd), processor(processor)
        {
            // Setup the pointer entity (which is re-used between laser/arc styles)
            arc_pointer.xz_plane_bounds = aabb_3d({ -24.f, -0.01f, -24.f }, { +24.f, +0.01f, +24.f });

            pointer = env->track_entity(orch->create_entity());
            env->identifier_system->create(pointer, "vr-pointer");
            env->xform_system->create(pointer, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
            env->render_system->create(pointer, material_component(pointer, material_handle(material_library::kDefaultMaterialId)));
            pointer_mesh_component = env->render_system->create(pointer, polymer::mesh_component(pointer, gpu_mesh_handle("vr-pointer")));

            // Setup left controller
            left_controller = env->track_entity(orch->create_entity());
            env->identifier_system->create(left_controller, "openvr-left-controller");
            env->xform_system->create(left_controller, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
            env->render_system->create(left_controller, material_component(left_controller, material_handle(material_library::kDefaultMaterialId)));
            env->render_system->create(left_controller, mesh_component(left_controller));

            // Setup right controller
            right_controller = env->track_entity(orch->create_entity());
            env->identifier_system->create(right_controller, "openvr-right-controller");
            env->xform_system->create(right_controller, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
            env->render_system->create(right_controller, material_component(right_controller, material_handle(material_library::kDefaultMaterialId)));
            env->render_system->create(right_controller, mesh_component(right_controller));

            // Setup render models for controllers when they are loaded
            hmd->controller_render_data_callback([this, env](cached_controller_render_data & data)
            {
                // We will get this callback for each controller, but we only need to handle it once for both.
                if (need_controller_render_data == true)
                {
                    need_controller_render_data = false;

                    // Create new gpu mesh from the openvr geometry
                    create_handle_for_asset("controller-mesh", make_mesh_from_geometry(data.mesh));

                    // Re-lookup components since they were std::move'd above
                    auto lmc = env->render_system->get_mesh_component(left_controller);
                    assert(lmc != nullptr);

                    auto rmc = env->render_system->get_mesh_component(right_controller);
                    assert(rmc != nullptr);

                    // Set the handles
                    lmc->mesh = gpu_mesh_handle("controller-mesh");
                    rmc->mesh = gpu_mesh_handle("controller-mesh");
                }
            });

            input_handler_connection = env->event_manager->connect(this, [this](const vr_input_event & event)
            { 
                handle_event(event); 
            });
        }

        ~vr_controller_system()
        {
            // todo - input_handler_connection disconnect
        }

        std::vector<entity> get_renderables() const
        {
            if (style != controller_render_style_t::invisible && should_draw_pointer) return { pointer, left_controller, right_controller };
            return { left_controller, right_controller };
        }

        void handle_event(const vr_input_event & event)
        {
            std::cout << "vr_controller_system handle event " << event.timestamp << std::endl;

            // can this entity be pointed at? (list)

            // Draw laser on focus
            if (event.type == vr_event_t::focus_begin)
            {
                set_visual_style(controller_render_style_t::laser);
                should_draw_pointer = true;
            }
            else if (event.type == vr_event_t::focus_end)
            {
                set_visual_style(controller_render_style_t::invisible);
                should_draw_pointer = false;
            }
        }

        void process(const float dt)
        {
            // update left/right controller positions
            const transform lct = hmd->get_controller(vr::TrackedControllerRole_LeftHand).t;
            env->xform_system->set_local_transform(left_controller, lct);
            const transform rct = hmd->get_controller(vr::TrackedControllerRole_RightHand).t;
            env->xform_system->set_local_transform(right_controller, rct);

            const std::vector<input_button_state> touchpad_button_states = {
                hmd->get_controller(vr::TrackedControllerRole_LeftHand).buttons[vr::k_EButton_SteamVR_Touchpad],
                hmd->get_controller(vr::TrackedControllerRole_RightHand).buttons[vr::k_EButton_SteamVR_Touchpad]
            };

            if (should_draw_pointer)
            {
                const vr_input_focus focus = processor->get_focus();
                if (focus.result.e != kInvalidEntity)
                {
                    const float hit_distance = focus.result.r.distance;
                    auto & m = pointer_mesh_component->mesh.get();
                    m = make_mesh_from_geometry(make_plane(0.010f, hit_distance, 24, 24), GL_STREAM_DRAW);

                    if (auto * tc = env->xform_system->get_local_transform(pointer))
                    {
                        // The mesh is in local space so we massage it through a transform 
                        auto t = hmd->get_controller(processor->get_dominant_hand()).t;
                        t = t * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float)POLYMER_PI / 2.f)); // coordinate
                        t = t * transform(float4(0, 0, 0, 1), float3(0, -(hit_distance * 0.5f), 0)); // translation
                        env->xform_system->set_local_transform(pointer, t);
                    }
                }
            }

            for (int i = 0; i < touchpad_button_states.size(); ++i)
            {
                // Draw arc on touchpad down
                if (touchpad_button_states[i].down)
                {
                    const transform t = hmd->get_controller(vr::ETrackedControllerRole(i + 1)).t;
                    arc_pointer.position = t.position;
                    arc_pointer.forward = -qzdir(t.orientation);

                    if (make_pointer_arc(arc_pointer, arc_curve))
                    {
                        set_visual_style(controller_render_style_t::arc);
                        should_draw_pointer = true;

                        auto & m = pointer_mesh_component->mesh.get();
                        m = make_mesh_from_geometry(make_parabolic_geometry(arc_curve, arc_pointer.forward, 0.1f, arc_pointer.lineThickness), GL_STREAM_DRAW);

                        if (auto * tc = env->xform_system->get_local_transform(pointer))
                        {
                            // the arc mesh is constructed in world space, so we reset its transform
                            env->xform_system->set_local_transform(pointer, {});
                        }
                    }
                }
                // Teleport on touchpad up
                else if (touchpad_button_states[i].released)
                {
                    should_draw_pointer = false;

                    // Target location is on the xz plane because of a linecast, so we re-add the current height of the player
                    target_location.y = hmd->get_hmd_pose().position.y;
                    const transform target_pose = { hmd->get_hmd_pose().orientation, target_location };

                    hmd->set_world_pose({}); // reset world pose
                    const transform hmd_pose = hmd->get_hmd_pose(); // hmd_pose is now in the HMD's own coordinate system
                    hmd->set_world_pose(target_pose * hmd_pose.inverse()); // set the new world pose

                    vr_teleport_event teleport_event;
                    teleport_event.world_position = target_pose.position;
                    teleport_event.timestamp = system_time_ns();
                    env->event_manager->send(teleport_event);
                }
            }
        }
    };

    //////////////////////////
    //   vr_imgui_surface   //
    //////////////////////////

    class vr_imgui_surface : public gui::imgui_surface
    {
        entity imgui_billboard;
        std::shared_ptr<polymer_fx_material> imgui_material;
    public:
        vr_imgui_surface(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window);
        void update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state);
        void update_renderloop();
        std::vector<entity> get_renderables() const;
    };

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
        polymer::event_manager_sync::connection input_handler_connection;
        bool focused{ false };
        void handle_event(const vr_input_event & event);
    public:
        vr_gizmo(entity_orchestrator * orch, environment * env, openvr_hmd * hmd, vr_input_processor * processor);
        void process(const float dt);
        std::vector<entity> get_renderables() const;
    };

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp

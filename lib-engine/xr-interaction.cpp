#include "xr-interaction.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"
#include "system-collision.hpp"

using namespace polymer::xr;

///////////////////////////////////////////
//   xr_input_processor implementation   //
///////////////////////////////////////////

xr_input_focus xr_input_processor::recompute_focus(const vr_controller & controller)
{
    const ray controller_ray = ray(controller.t.position, -qzdir(controller.t.orientation));
    const entity_hit_result box_result = the_scene->collision_system->raycast(controller_ray, raycast_type::box);

    if (box_result.r.hit)
    {
        // Refine if hit the mesh
        const entity_hit_result mesh_result = the_scene->collision_system->raycast(controller_ray, raycast_type::mesh);
        if (mesh_result.r.hit)
        {
            return { controller_ray, mesh_result, false }; // "hard focus"
        }
        else
        {
            // Otherwise hitting an outer bounding box is still considered focus
            return { controller_ray, box_result, true }; // "soft focus"
        }
    }
    return { controller_ray, {} };
}

xr_input_processor::xr_input_processor(entity_system_manager * esm, scene * the_scene, hmd_base * hmd) : the_scene(the_scene), hmd(hmd) {}
xr_input_processor::~xr_input_processor() {}

vr_controller xr_input_processor::get_controller(const vr_controller_role hand)
{
    return hmd->get_controller(hand);
}

vr_controller_role xr_input_processor::get_dominant_hand() const
{ 
    return dominant_hand; 
}

xr_input_focus xr_input_processor::get_focus() const 
{
    return last_focus; 
}

void xr_input_processor::set_fixed_dominant_hand(const vr_controller_role hand)
{
    if (hand == vr_controller_role::invalid) 
    {
        fixed_dominant_hand = false;
    }
    else
    {
        dominant_hand = hand;
        fixed_dominant_hand = true;
    }
}

void xr_input_processor::process(const float dt)
{
    //scoped_timer t("xr_input_processor::process()");

    // Generate button events
    for (auto hand : { vr_controller_role::left_hand, vr_controller_role::right_hand })
    {
        const vr_controller controller = hmd->get_controller(hand);
        const vr_input_source_t src = (hand == vr_controller_role::left_hand) ? vr_input_source_t::left_controller : vr_input_source_t::right_controller;

        for (auto b : controller.buttons)
        {
            if (b.second.pressed)
            {
                const xr_input_focus focus = recompute_focus(controller);
                xr_input_event press = make_event(xr_button_event::press, src, focus, controller);
                the_scene->event_manager->send(press);
                log::get()->engine_log->info("xr_input_processor xr_button_event::press for entity {}", focus.result.e);

                // Swap dominant hand based on last activated trigger button
                if (b.first == vr_button::trigger)
                {
                    if (!fixed_dominant_hand) 
                    {
                        dominant_hand = hand;
                    }
                }
            }
            else if (b.second.released)
            {
                const xr_input_focus focus = recompute_focus(controller);
                xr_input_event release = make_event(xr_button_event::release, src, focus, controller);
                the_scene->event_manager->send(release);
                log::get()->engine_log->info("xr_input_processor xr_button_event::release for entity {}", focus.result.e);
            }
        }
    }

    // Generate focus events for the dominant hand. todo - this can be rate-limited
    {
        const vr_controller controller = hmd->get_controller(dominant_hand);
        const vr_input_source_t src = (dominant_hand == vr_controller_role::left_hand) ? vr_input_source_t::left_controller : vr_input_source_t::right_controller;

        const xr_input_focus active_focus = recompute_focus(controller);

        // New focus, not invalid
        if (active_focus != last_focus && active_focus.result.e != kInvalidEntity)
        {
            xr_input_event focus_gained = make_event(xr_button_event::focus_begin, src, active_focus, controller);
            the_scene->event_manager->send(focus_gained);
            // todo - focus_end on old entity

            log::get()->engine_log->info("xr_input_processor xr_button_event::focus_begin for entity {}", active_focus.result.e);
        }

        // Last one valid, new one invalid
        if (last_focus.result.e != kInvalidEntity && active_focus.result.e == kInvalidEntity)
        {
            xr_input_event focus_lost = make_event(xr_button_event::focus_end, src, last_focus, controller);
            the_scene->event_manager->send(focus_lost);

            log::get()->engine_log->info("xr_input_processor xr_button_event::focus_end for entity {}", last_focus.result.e);
        }

        last_focus = active_focus;
    }
}

/////////////////////////////////////////////
//   xr_controller_system implementation   //
/////////////////////////////////////////////

xr_controller_system::xr_controller_system(entity_system_manager * esm, scene * env, hmd_base * hmd, xr_input_processor * processor)
    : the_scene(env), hmd(hmd), processor(processor)
{
    // fixme - the min/max teleportation bounds in world space are defined by this bounding box. 
    arc_pointer.xz_plane_bounds = aabb_3d({ -24.f, -0.01f, -24.f }, { +24.f, +0.01f, +24.f });

    laser_pointer_material = std::make_shared<polymer_procedural_material>();
    laser_pointer_material->shader = shader_handle("xr-laser");
    env->mat_library->register_material("laser-pointer-mat", laser_pointer_material);

    // Setup the pointer entity (which is re-used between laser/arc styles)
    pointer = env->track_entity(esm->create_entity());
    env->identifier_system->create(pointer, "xr-pointer");
    env->xform_system->create(pointer, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
    env->render_system->create(pointer, material_component(pointer, material_handle("laser-pointer-mat")));
    pointer_mesh_component = env->render_system->create(pointer, polymer::mesh_component(pointer, gpu_mesh_handle("xr-pointer")));

    controller_material[0] = std::make_shared<polymer_blinn_phong_standard>();
    env->mat_library->register_material("xr-controller-material-left", controller_material[0]);

    controller_material[1] = std::make_shared<polymer_blinn_phong_standard>();
    env->mat_library->register_material("xr-controller-material-right", controller_material[1] );

    // Setup left controller
    left_controller = env->track_entity(esm->create_entity());
    env->identifier_system->create(left_controller, "xr-controller-root-left");
    env->xform_system->create(left_controller, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
    env->render_system->create(left_controller, material_component(left_controller, material_handle("xr-controller-material-left")));
    env->render_system->create(left_controller, mesh_component(left_controller));

    // Setup right controller
    right_controller = env->track_entity(esm->create_entity());
    env->identifier_system->create(right_controller, "xr-controller-root-right");
    env->xform_system->create(right_controller, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
    env->render_system->create(right_controller, material_component(right_controller, material_handle("xr-controller-material-right")));
    env->render_system->create(right_controller, mesh_component(right_controller));

    // Setup render models for controllers when they are loaded
    hmd->controller_render_data_callback([this, env](cached_controller_render_data & data)
    {
        auto role = data.role;
        const std::string role_str = (role == vr_controller_role::left_hand) ? "-left" : "-right";

        if (role == vr_controller_role::left_hand)
        {
            create_handle_for_asset(std::string("xr-controller-mesh" + role_str).c_str(), make_mesh_from_geometry(data.mesh));
            auto lmc = env->render_system->get_mesh_component(left_controller);
            assert(lmc != nullptr);
            lmc->mesh = gpu_mesh_handle(std::string("xr-controller-mesh" + role_str).c_str());
            create_handle_for_asset("xr-controller-root-left-texture", std::move(data.tex));
            controller_material[0]->diffuse = texture_handle("xr-controller-root-left-texture");
        }
        else
        {
            create_handle_for_asset(std::string("xr-controller-mesh" + role_str).c_str(), make_mesh_from_geometry(data.mesh));
            auto rmc = env->render_system->get_mesh_component(right_controller);
            assert(rmc != nullptr);
            rmc->mesh = gpu_mesh_handle(std::string("xr-controller-mesh" + role_str).c_str());
            create_handle_for_asset("xr-controller-root-right-texture", std::move(data.tex));
            controller_material[1]->diffuse = texture_handle("xr-controller-root-right-texture");
        }
    });

    xr_input = env->event_manager->connect(this, [this](const xr_input_event & event)
    {
        handle_event(event);
    });

    laser_pointer_material->update_uniform_func = [this]()
    {
        auto & laser_shader = laser_pointer_material->compiled_shader->shader;
        laser_shader.bind();
        laser_shader.uniform("u_alpha", laser_alpha);
        laser_shader.uniform("u_color", laser_color);
        laser_shader.unbind();
    };
}

xr_controller_system::~xr_controller_system()
{
    // todo - xr_input disconnect
}

std::vector<entity> xr_controller_system::get_renderables() const
{
    if (render_styles.size())
    {
        return { pointer, left_controller, right_controller };
    }
    return { left_controller, right_controller };
}

void xr_controller_system::handle_event(const xr_input_event & event)
{
    // todo - can this entity be pointed at? (list for system)

    // Draw laser on focus of any type
    if (event.type == xr_button_event::focus_begin)
    {
        render_styles.push(controller_render_style_t::laser_to_entity);
        animator.cancel_all();
        tween_event & fade_in = animator.add_tween(&laser_alpha, 1.0f, laser_fade_seconds, tween::linear::ease_in_out);
        fade_in.on_finish = [this]() {};
    }
    else if (event.type == xr_button_event::focus_end)
    {
        animator.cancel_all();
        tween_event & fade_out = animator.add_tween(&laser_alpha, 0.0f, laser_fade_seconds, tween::linear::ease_in_out);
        fade_out.on_update = [this](const float t)
        {
            update_laser_geometry(laser_fixed_draw_distance); // keep drawing laser until it's faded out.
        };

        fade_out.on_finish = [this]()
        {
            while (!render_styles.empty()) render_styles.pop();
        };
    }
}

void xr::xr_controller_system::update_laser_geometry(const float distance)
{

    auto front_plane = make_plane(laser_line_thickness, distance, 4, 24, false);

    auto back_plane = make_plane(laser_line_thickness, distance, 4, 24, false);
    for (auto & v : back_plane.vertices) v = transform_coord(make_rotation_matrix({0, 0, 1.f}, (float) POLYMER_PI), v);
    for (auto & f : back_plane.faces) f = {f.z, f.y, f.x};

    auto laser_geom = concatenate_geometry(front_plane, back_plane);
    
    auto & m = pointer_mesh_component->mesh.get();
    m = make_mesh_from_geometry(laser_geom, GL_STREAM_DRAW);

    if (auto * tc = the_scene->xform_system->get_local_transform(pointer))
    {
        // The mesh is in local space so we massage it through a transform 
        auto t = hmd->get_controller(processor->get_dominant_hand()).t;
        t = t * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float)POLYMER_PI / 2.f)); // coordinate 
        t = t * transform(quatf(0, 0, 0, 1), float3(0, -(distance * 0.5f), 0)); // translation
        the_scene->xform_system->set_local_transform(pointer, t);
    }
}

void xr_controller_system::process(const float dt)
{
    animator.update(dt);

    // update left/right controller positions
    const transform lct = hmd->get_controller(vr_controller_role::left_hand).t;
    the_scene->xform_system->set_local_transform(left_controller, lct);
    const transform rct = hmd->get_controller(vr_controller_role::right_hand).t;
    the_scene->xform_system->set_local_transform(right_controller, rct);

    // touchpad state used for teleportation
    const std::vector<vr_button_state> touchpad_button_states = {
        hmd->get_controller(vr_controller_role::left_hand).buttons[vr_button::xy],
        hmd->get_controller(vr_controller_role::right_hand).buttons[vr_button::xy]
    };

    auto check_ignored = [this](const entity what)
    {
        bool result = false;
        for (const auto & e : ignored_entities)
        {
            if (what == e) return true;
        }
        return false;
    };

    if (render_styles.size() && render_styles.top() == controller_render_style_t::laser_to_entity)
    {
        const xr_input_focus focus = processor->get_focus();

        if (focus.result.e != kInvalidEntity && check_ignored(focus.result.e) == false)
        {
            // Hard focus ends on the object hit
            if (focus.soft == false)
            {
                const float hit_distance = focus.result.r.distance;

                // Ensure the distance is long enough to generate valid drawable geometry
                if (hit_distance >= 0.01f) update_laser_geometry(hit_distance);
            }
            else if (focus.soft == true)
            {
                // Soft focus has a fixed draw distance
                update_laser_geometry(laser_fixed_draw_distance);
            }
        }
    }

    for (int i = 0; i < touchpad_button_states.size(); ++i)
    {
        // Draw arc on touchpad down
        if (touchpad_button_states[i].down)
        {
            const transform t = hmd->get_controller(vr_controller_role(i + 1)).t;
            arc_pointer.position = t.position;
            arc_pointer.forward = -qzdir(t.orientation);

            if (make_pointer_arc(arc_pointer, arc_curve))
            {
                // Push arc style
                if (render_styles.empty())
                {
                    // From zero to one style
                    render_styles.push(controller_render_style_t::arc);
                }
                else if (render_styles.top() != controller_render_style_t::arc)
                {
                    // If we're already laser pointering, only add if the top isn't already an arc
                    render_styles.push(controller_render_style_t::arc);
                }

                // Cache alpha
                laser_alpha_on_teleport = laser_alpha;
                if (laser_alpha_on_teleport < 1.f) laser_alpha = 1.0;

                auto & m = pointer_mesh_component->mesh.get();
                m = make_mesh_from_geometry(make_parabolic_geometry(arc_curve, arc_pointer.forward, 0.1f, float3(laser_line_thickness)), GL_STREAM_DRAW);
                target_location = arc_curve.back(); // world-space hit point
                if (auto * tc = the_scene->xform_system->get_local_transform(pointer))
                {
                    // the arc mesh is constructed in world space, so we reset its transform
                    the_scene->xform_system->set_local_transform(pointer, {});
                }
            }
        }
        // Teleport on touchpad up
        else if (touchpad_button_states[i].released)
        {
            // Pop arc style. This check also prevents on-release behavior for 
            // scenarios where make_arc_pointer returns in invalid solution
            if (render_styles.size() && render_styles.top() == controller_render_style_t::arc)
            {
                render_styles.pop();
                laser_alpha = laser_alpha_on_teleport; // restore cached alpha

                // Target location is on the xz plane because of a linecast, so we re-add the current height of the player
                target_location.y = hmd->get_hmd_pose().position.y;
                const transform target_pose = { hmd->get_hmd_pose().orientation, target_location };

                hmd->set_world_pose({}); // reset world pose
                const transform hmd_pose = hmd->get_hmd_pose(); // hmd_pose is now in the HMD's own coordinate system
                hmd->set_world_pose(target_pose * hmd_pose.inverse()); // set the new world pose

                xr_teleport_event teleport_event;
                teleport_event.world_position = target_pose.position;
                teleport_event.timestamp = system_time_ns();
                the_scene->event_manager->send(teleport_event);
            }
        }
    }
}

void xr_controller_system::add_focus_ignore(const entity ignored_entity)
{
    ignored_entities.push_back(ignored_entity);
}

////////////////////////////////////////
//   xr_imgui_system implementation   //
////////////////////////////////////////

xr_imgui_system::xr_imgui_system(entity_system_manager * esm, scene * env, hmd_base * hmd, xr_input_processor * processor, const uint2 size, GLFWwindow * window)
    : the_scene(env), hmd(hmd), processor(processor), imgui_surface(size, window)
{
    // Setup the billboard entity
    auto mesh = make_fullscreen_quad_ndc_geom();
    for (auto & v : mesh.vertices) { v *= 0.15f; }

    create_handle_for_asset("imgui-billboard", make_mesh_from_geometry(mesh)); // gpu mesh
    create_handle_for_asset("imgui-billboard", std::move(mesh)); // cpu mesh

    imgui_material = std::make_shared<polymer_procedural_material>();
    imgui_material->shader = shader_handle("unlit-texture");
    env->mat_library->register_material("imgui", imgui_material);

    imgui_billboard = env->track_entity(esm->create_entity());
    env->identifier_system->create(imgui_billboard, "imgui-billboard");
    env->xform_system->create(imgui_billboard, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
    env->render_system->create(imgui_billboard, material_component(imgui_billboard, material_handle("imgui")));
    env->render_system->create(imgui_billboard, mesh_component(imgui_billboard, gpu_mesh_handle("imgui-billboard")));
    env->collision_system->create(imgui_billboard, geometry_component(imgui_billboard, cpu_mesh_handle("imgui-billboard")));

    xr_input = env->event_manager->connect(this, [this](const xr_input_event & event)
    {
        handle_event(event);
    });
}

xr_imgui_system::~xr_imgui_system()
{
    // fixme - xr_input must be disconnected
}

void xr_imgui_system::handle_event(const xr_input_event & event)
{
    if (event.focus.result.e == imgui_billboard)
    {
        if (event.type == xr_button_event::focus_begin) focused = true;
        else if (event.type == xr_button_event::focus_end) focused = false;
    }
}

void xr_imgui_system::set_surface_transform(const transform & t)
{
    // Update surface/billboard position
    if (auto * tc = the_scene->xform_system->get_local_transform(imgui_billboard))
    {
        the_scene->xform_system->set_local_transform(imgui_billboard, t);
    }
}

void xr_imgui_system::process(const float dt)
{
    // Here we shim/translate controller data into an `app_input_event` since imgui
    // is designed for mouse + keyboard-based interaction
    if (focused)
    {
        const xr_input_focus focus = processor->get_focus();
        const uint2 imgui_framebuffer_size = get_size();
        const float2 pixel_coord = { (1 - focus.result.r.uv.x) * imgui_framebuffer_size.x, focus.result.r.uv.y * imgui_framebuffer_size.y };

        const bool trigger_state = hmd->get_controller(processor->get_dominant_hand()).buttons[vr_button::trigger].down;
        app_input_event controller_event;
        controller_event.type = app_input_event::MOUSE;
        controller_event.action = trigger_state;
        controller_event.value = { 0, 0 };
        controller_event.cursor = pixel_coord;

        imgui->update_input(controller_event);
    }

    imgui_material->update_uniform_func = [this]()
    {
        auto & imgui_shader = imgui_material->compiled_shader->shader;
        imgui_shader.uniform("u_flip", 1.f);
        imgui_shader.texture("s_texture", 0, get_render_texture(), GL_TEXTURE_2D);
    };
}

std::vector<entity> xr_imgui_system::get_renderables() const
{
    return { imgui_billboard };
}

////////////////////////////////////////
//   xr_gizmo_system implementation   //
////////////////////////////////////////

xr_gizmo_system::xr_gizmo_system(entity_system_manager * esm, scene * the_scene, hmd_base * hmd, xr_input_processor * processor)
    : the_scene(the_scene), hmd(hmd), processor(processor)
{
    auto unlit_material = std::make_shared<polymer_procedural_material>();
    unlit_material->shader = shader_handle("unlit-vertex-color");
    the_scene->mat_library->register_material("unlit-vertex-color-material", unlit_material);

    gizmo_entity = the_scene->track_entity(esm->create_entity());
    the_scene->identifier_system->create(gizmo_entity, "gizmo-renderable");
    the_scene->xform_system->create(gizmo_entity, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
    the_scene->render_system->create(gizmo_entity, material_component(gizmo_entity, material_handle("unlit-vertex-color-material")));
    the_scene->render_system->create(gizmo_entity, mesh_component(gizmo_entity));
    the_scene->collision_system->create(gizmo_entity, geometry_component(gizmo_entity));

    // tinygizmo uses a callback to pass its world-space mesh back to users. The callback is triggered by
    // the process(...) function below. 
    gizmo_ctx.render = [this, the_scene](const tinygizmo::geometry_mesh & r)
    {
        const std::vector<tinygizmo::geometry_vertex> & verts = r.vertices;
        const std::vector<linalg::aliases::uint3> & tris = reinterpret_cast<const std::vector<linalg::aliases::uint3> &>(r.triangles);

        // For rendering
        if (auto * mc = the_scene->render_system->get_mesh_component(gizmo_entity))
        {
            auto & gizmo_gpu_mesh = mc->mesh.get();

            // Upload new gizmo mesh data to the gpu
            gizmo_gpu_mesh.set_vertices(verts, GL_DYNAMIC_DRAW);
            gizmo_gpu_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, position));
            gizmo_gpu_mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, normal));
            gizmo_gpu_mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, color));
            gizmo_gpu_mesh.set_elements(tris, GL_DYNAMIC_DRAW);
        }

        // For focus/defocus and pointing
        if (auto * gc = the_scene->collision_system->get_component(gizmo_entity))
        {
            if (verts.size() != transient_gizmo_geom.vertices.size()) transient_gizmo_geom.vertices.resize(verts.size());
            if (tris.size() != transient_gizmo_geom.faces.size()) transient_gizmo_geom.faces.resize(tris.size());

            // Verts are packed in a struct
            for (int i = 0; i < verts.size(); ++i)
            {
                auto & v = verts[i];
                transient_gizmo_geom.vertices[i] = {v.position.x, v.position.y, v.position.z};
            }

            // Faces can be memcpy'd directly
            std::memcpy(transient_gizmo_geom.faces.data(), tris.data(), tris.size() * sizeof(uint3));

            auto & gizmo_cpu_mesh = gc->geom.get();
            gizmo_cpu_mesh = transient_gizmo_geom;
        }
    };

    xr_input = the_scene->event_manager->connect(this, [this](const xr_input_event & event)
    {
        handle_event(event);
    });
}

xr_gizmo_system::~xr_gizmo_system()
{
    // @todo - xr_input must be disconnected
}

void xr_gizmo_system::handle_event(const xr_input_event & event)
{
    if (event.focus.result.e == gizmo_entity)
    {
        if (event.type == xr_button_event::focus_begin) focused = true;
        else if (event.type == xr_button_event::focus_end) focused = false;
    }
}

void xr_gizmo_system::process(const float dt)
{
    const auto view = view_data(0, hmd->get_eye_pose(vr_eye::left_eye), hmd->get_proj_matrix(vr_eye::left_eye, 0.075f, 128.f));
    const auto vfov = vfov_from_projection(view.projectionMatrix);

    gizmo_state.cam.near_clip = view.nearClip;
    gizmo_state.cam.far_clip = view.farClip;
    gizmo_state.cam.yfov = vfov;
    gizmo_state.cam.position = minalg::float3(view.pose.position.x, view.pose.position.y, view.pose.position.z);
    gizmo_state.cam.orientation = minalg::float4(view.pose.orientation.x, view.pose.orientation.y, view.pose.orientation.z, view.pose.orientation.w);
    
    if (focused)
    {   
        const xr_input_focus focus = processor->get_focus();
        gizmo_state.ray_origin = minalg::float3(focus.r.origin.x, focus.r.origin.y, focus.r.origin.z);
        gizmo_state.ray_direction = minalg::float3(focus.r.direction.x, focus.r.direction.y, focus.r.direction.z);
        gizmo_state.mouse_left = hmd->get_controller(processor->get_dominant_hand()).buttons[vr_button::trigger].down;
    }

    // Update
    gizmo_ctx.update(gizmo_state);

    // Draw gizmo @ transform
    tinygizmo::transform_gizmo("vr-gizmo", gizmo_ctx, xform);

    // Trigger render callback
    gizmo_ctx.draw();
}

std::vector<entity> xr_gizmo_system::get_renderables() const
{
    return { gizmo_entity };
}

void xr_gizmo_system::set_transform(const transform t)
{
    xform.position = {t.position.x, t.position.y, t.position.z};
    xform.orientation = {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w};
}

// @todo - transforms do not carry scale (yet)
transform xr_gizmo_system::get_transform() const
{
    transform t;
    t.position = float3(xform.position.x, xform.position.y, xform.position.z);
    t.orientation = quatf(xform.orientation.x, xform.orientation.y, xform.orientation.z, xform.orientation.w);
    return t;
}

void xr_gizmo_system::set_render_scale(const float scale)
{
	// @todo
}

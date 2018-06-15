#include "vr-interaction.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "system-render.hpp"
#include "system-collision.hpp"

/////////////////////////////////////////
//   vr_imgui_surface implementation   //
/////////////////////////////////////////

vr_imgui_surface::vr_imgui_surface(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window) 
    : imgui_surface(size, window)
{
    // Setup the billboard entity
    {
        auto mesh = make_fullscreen_quad_ndc_geom();
        for (auto & v : mesh.vertices) { v *= 0.15f; }

        create_handle_for_asset("billboard-mesh", make_mesh_from_geometry(mesh)); // gpu mesh
        create_handle_for_asset("billboard-mesh", std::move(mesh)); // cpu mesh

        imgui_billboard = env->track_entity(orch->create_entity());
        env->identifier_system->create(imgui_billboard, "imgui-billboard");
        env->xform_system->create(imgui_billboard, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::geometry_component billboard_geom(imgui_billboard);
        billboard_geom.geom = cpu_mesh_handle("billboard-mesh");
        env->collision_system->create(imgui_billboard, std::move(billboard_geom));

        imgui_material = std::make_shared<polymer_fx_material>();
        imgui_material->shader = shader_handle("textured");
        env->mat_library->create_material("imgui", imgui_material);

        polymer::material_component billboard_mat(imgui_billboard);
        billboard_mat.material = material_handle("imgui");
        env->render_system->create(imgui_billboard, std::move(billboard_mat));

        polymer::mesh_component billboard_mesh(imgui_billboard);
        billboard_mesh.mesh = gpu_mesh_handle("billboard-mesh");
        env->render_system->create(imgui_billboard, std::move(billboard_mesh));
    }

    // Setup the pointer entity
    {
        pointer = env->track_entity(orch->create_entity());
        env->identifier_system->create(pointer, "laser-pointer");
        env->xform_system->create(pointer, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::material_component pointer_mat(pointer);
        pointer_mat.material = material_handle(material_library::kDefaultMaterialId);
        env->render_system->create(pointer, std::move(pointer_mat));

        polymer::mesh_component pointer_mesh(pointer);
        pointer_mesh.mesh = gpu_mesh_handle("imgui-pointer");
        env->render_system->create(pointer, std::move(pointer_mesh));
    }
}

void vr_imgui_surface::update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state)
{
    // Update billboard position
    if (auto * tc = env->xform_system->get_local_transform(pointer))
    {
        env->xform_system->set_local_transform(imgui_billboard, billboard_origin);
    }

    transform t = pointer_transform;
    const ray controller_ray = ray(t.position, -qzdir(t.orientation));
    const entity_hit_result result = env->collision_system->raycast(controller_ray);

    if (auto * pc = env->render_system->get_mesh_component(pointer))
    {
        if (result.r.hit)
        {
            // scoped_timer t("raycast assign"); // tofix: this function is pretty slow
            geometry ray_geo = make_plane(0.010f, result.r.distance, 24, 24);
            auto & gpu_mesh = pc->mesh.get();
            gpu_mesh = make_mesh_from_geometry(ray_geo, GL_STREAM_DRAW);

            if (auto * tc = env->xform_system->get_local_transform(pointer))
            {
                t = t * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float)POLYMER_PI / 2.f)); // coordinate
                t = t * transform(float4(0, 0, 0, 1), float3(0, -(result.r.distance / 2.f), 0)); // translation
                env->xform_system->set_local_transform(pointer, t);
            }

            const uint2 sz = get_size();
            const float2 pixel_coord = { (1 - result.r.uv.x) * sz.x, result.r.uv.y * sz.y };

            app_input_event controller_event;
            controller_event.type = app_input_event::MOUSE;
            controller_event.action = trigger_state;
            controller_event.value = { 0, 0 };
            controller_event.cursor = pixel_coord;

            imgui->update_input(controller_event);

            should_draw_pointer = true;
        }
        else
        {
            should_draw_pointer = false;
        }
    }
}

void vr_imgui_surface::update_renderloop()
{
    imgui_material->use();
    auto & imgui_shader = imgui_material->compiled_shader->shader;
    imgui_shader.texture("s_texture", 0, get_render_texture(), GL_TEXTURE_2D);
    imgui_shader.unbind();
}

entity vr_imgui_surface::get_pointer() const
{
    return should_draw_pointer ? pointer : kInvalidEntity;
}

entity vr_imgui_surface::get_billboard() const
{
    return imgui_billboard;
}

///////////////////////////////////////////
//   vr_teleport_system implementation   //
///////////////////////////////////////////

vr_teleport_system::vr_teleport_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)  : hmd(hmd)
{
    nav_geometry = make_plane(48, 48, 2, 2);

    for (auto & v : nav_geometry.vertices)
    {
        // Flip nav mesh since it's not automatically the correct orientation to be a floor
        const float4x4 flip = make_rotation_matrix({ 1, 0, 0 }, (float)-POLYMER_PI / 2.f);
        v = transform_coord(flip, v);
    }

    pointer.navMeshBounds = compute_bounds(nav_geometry);

    // Create and track pointer entity (along with name + transform)
    teleportation_arc = env->track_entity(orch->create_entity());
    env->identifier_system->create(teleportation_arc, "teleportation-arc");
    env->xform_system->create(teleportation_arc, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

    // Attach material to arc
    polymer::material_component pointer_mat(teleportation_arc);
    pointer_mat.material = material_handle(material_library::kDefaultMaterialId);
    env->render_system->create(teleportation_arc, std::move(pointer_mat));

    // Attach empty mesh to arc
    polymer::mesh_component pointer_mesh(teleportation_arc);
    pointer_mesh.mesh = gpu_mesh_handle("teleportation-arc");
    cached_mesh = env->render_system->create(teleportation_arc, std::move(pointer_mesh));
    assert(cached_mesh != nullptr);
}

void vr_teleport_system::update(const uint64_t current_frame)
{
    std::vector<openvr_controller::button_state> states = {
        hmd->get_controller(vr::TrackedControllerRole_LeftHand)->pad,
        hmd->get_controller(vr::TrackedControllerRole_RightHand)->pad
    };

    for (int i = 0; i < states.size(); ++i)
    {
        const openvr_controller::button_state s = states[i];

        if (s.down)
        {
            const transform t = hmd->get_controller(vr::ETrackedControllerRole(i + 1))->get_pose(hmd->get_world_pose());
            pointer.position = t.position;
            pointer.forward = -qzdir(t.orientation);

            geometry g;
            if (make_parabolic_pointer(pointer, g, target_location))
            {
                should_draw = true;
                auto & m = cached_mesh->mesh.get();
                m = make_mesh_from_geometry(g, GL_STREAM_DRAW);
            }
        }

        if (s.released && should_draw)
        {
            should_draw = false;

            target_location.y = hmd->get_hmd_pose().position.y;
            const transform target_pose = { hmd->get_hmd_pose().orientation, target_location };

            hmd->set_world_pose({}); // reset world pose
            auto hmd_pose = hmd->get_hmd_pose(); // pose is now in the HMD's own coordinate system
            hmd->set_world_pose(target_pose * hmd_pose.inverse());

            vr_teleport_event event;
            event.world_position = target_pose.position;
            event.frame_count = current_frame;
        }
    }
}

entity vr_teleport_system::get_teleportation_arc()
{
    return should_draw ? teleportation_arc : kInvalidEntity;
}

/////////////////////////////////
//   vr_gizmo implementation   //
/////////////////////////////////

vr_gizmo::vr_gizmo(entity_orchestrator * orch, environment * env, material_library * library)
{
    gizmo_ctx.render = [&](const tinygizmo::geometry_mesh & r)
    {
        if (auto * mc = env->render_system->get_mesh_component(gizmo_entity))
        {
            auto & gizmo_gpu_mesh = mc->mesh.get();

            // Upload new gizmo mesh data to the gpu
            const std::vector<linalg::aliases::float3> & verts = reinterpret_cast<const std::vector<linalg::aliases::float3> &>(r.vertices);
            const std::vector<linalg::aliases::uint3> & tris = reinterpret_cast<const std::vector<linalg::aliases::uint3> &>(r.triangles);
            gizmo_gpu_mesh.set_vertices(verts, GL_DYNAMIC_DRAW);
            gizmo_gpu_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, position));
            gizmo_gpu_mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, normal));
            gizmo_gpu_mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, color));
            gizmo_gpu_mesh.set_elements(tris, GL_DYNAMIC_DRAW);
        }
    };
}

void vr_gizmo::handle_input(const app_input_event & e)
{

}

void vr_gizmo::update(const view_data view)
{

}

void vr_gizmo::render()
{

}

entity vr_gizmo::get_pointer() const
{ 
    return should_draw_pointer ? pointer : kInvalidEntity; 
}

entity vr_gizmo::get_gizmo() const
{ 
    return gizmo_entity; 
}

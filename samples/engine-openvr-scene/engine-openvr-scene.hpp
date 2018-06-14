#pragma once

#include "lib-engine.hpp"
#include "lib-polymer.hpp"
#include "physics-controller.hpp"

using namespace polymer;

class imgui_surface
{
    gl_framebuffer renderFramebuffer;
    gl_texture_2d renderTexture;
    uint2 framebufferSize;

protected:

    std::unique_ptr<gui::imgui_instance> imgui;

public:

    imgui_surface(const uint2 size, GLFWwindow * window) : framebufferSize(size)
    {
        imgui.reset(new gui::imgui_instance(window));
        renderTexture.setup(size.x, size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);
        renderFramebuffer.check_complete();
    }

    uint2 get_size() const { return framebufferSize; }
    gui::imgui_instance * get_instance() { return imgui.get(); }
    uint32_t get_render_texture() const { return renderTexture; }

    void begin_frame()
    {
        imgui->begin_frame(framebufferSize.x, framebufferSize.y);
    }

    void end_frame()
    {
        // Save framebuffer state
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint drawFramebuffer = 0, readFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glViewport(0, 0, (GLsizei)framebufferSize.x, (GLsizei)framebufferSize.y);

        glClear(GL_COLOR_BUFFER_BIT);
        imgui->end_frame();

        // Restore framebuffer state
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    }
};

class imgui_vr : public imgui_surface
{
    entity imgui_billboard;
    entity pointer;
    std::shared_ptr<polymer_fx_material> imgui_material;
    bool should_draw_pointer{ false };

public:

    imgui_vr(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window) : imgui_surface(size, window)
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

    void update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state)
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
                // scoped_timer t("raycast assign");
                geometry ray_geo = make_plane(0.010f, result.r.distance, 24, 24);
                auto & gpu_mesh = pc->mesh.get();
                gpu_mesh = make_mesh_from_geometry(ray_geo, GL_STREAM_DRAW);

                if (auto * tc = env->xform_system->get_local_transform(pointer))
                {
                    t = t * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float) POLYMER_PI / 2.f)); // coordinate
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

    // This function must be called in the render loop
    void update_renderloop()
    {
        imgui_material->use();
        auto & imgui_shader = imgui_material->compiled_shader->shader;
        imgui_shader.texture("s_texture", 0, get_render_texture(), GL_TEXTURE_2D);
        imgui_shader.unbind();
    }

    entity get_pointer() const
    {
        return should_draw_pointer ? pointer : kInvalidEntity;
    }

    entity get_billboard() const
    {
        return imgui_billboard;
    }
};

// You must create two of these for stereo rendering!
// std::unique_ptr<gizmo_vr> left_eye_gizmo;
// std::unique_ptr<gizmo_vr> right_eye_gizmo;

// This is a new implementation designed for the entity/material system. gl_gizmo
// in the other applications is drawn on top of everything else (a bit of a hack)
// and doesn't go through Polymer's actual scene renderer. To render in VR, we need to
// do everything properly.
class gizmo_vr
{
    entity gizmo_entity;
    entity pointer;
    
    std::shared_ptr<polymer_fx_material> gizmo_material;
    bool should_draw_pointer{ false };

    tinygizmo::gizmo_application_state gizmo_state;
    tinygizmo::gizmo_context gizmo_ctx;

public:

    gizmo_vr(entity_orchestrator * orch, environment * env, material_library * library)
    {
        // Use `imgui_vr` object as a reference in how to create and assign materials and components.

        // Create a new polymer_fx_material and assign it to `gizmo_material` variable above
        // Create a new gl_shader (you might need help with this step) -- skip at first
        // Attach the shader to the `gizmo_material`
        // Lastly, add this material to the material_library
        
        // Now, create all the components that you will need to render 1) the gizmo and 2) the pointer
        // The components you will need are: identifier, transform, mesh_component, material_component
        // and geometry_component.

        // 1) setup gizmo_entity here

        // 2) setup pointer here

        // This is tricky so I wrote it for you :)
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

    void handle_input(const app_input_event & e)
    {
        // Use `handle_input` in gl_gizmo.hpp as a reference for this function.

        // You must create your own `app_input_event` at the application layer.
        // We need to translate VR controller events into some kind of mouse/keyboard representation. 
        // So, this function will set a bunch of properties on `gizmo_state`
    }

    // Must call this for each eye instance
    void update(const float4x4 & eyeViewProjectionMatrix,
        const float nearClip,
        const float farClip,
        const float vFov,
        const transform & eyeTransform, 
        const polymer::float2 windowSize)
    {
        // Use `update` in gl_gizmo.hpp for what needs to update on `gizmo_state`

        // ALSO you will need to understand & copy the code from update() on the imgui_vr object to update
        // the pointer ray that should be drawn when we're pointing + interacting with the gizmo
    }

    // Call this somewhere in the application render loop
    void render()
    {
        gizmo_ctx.draw();
    }

    entity get_pointer() const { return should_draw_pointer ? pointer : kInvalidEntity; }
    entity get_gizmo() const { return gizmo_entity; }
};

namespace polymer
{
    struct vr_teleport_event { float3 world_position; uint64_t frame_count; };
    POLYMER_SETUP_TYPEID(vr_teleport_event);

    class vr_teleport_system final : public base_system
    {
        pointer_data pointer;
        geometry nav_geometry;
        float3 target_location;
        bool should_draw{ false };
        openvr_hmd * hmd{ nullptr };

        entity teleportation_arc{ kInvalidEntity };
        mesh_component * cached_mesh;

    public:

        vr_teleport_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd) : base_system(orch), hmd(hmd)
        {
            nav_geometry = make_plane(48, 48, 2, 2);

            for (auto & v : nav_geometry.vertices)
            {
                // Flip nav mesh since it's not automatically the correct orientation to be a floor
                const float4x4 flip = make_rotation_matrix({ 1, 0, 0 }, (float) -POLYMER_PI / 2.f);
                v = transform_coord(flip, v);
            }

            pointer.navMeshBounds = compute_bounds(nav_geometry);

            // Create and track pointer entity (along with name + transform)
            teleportation_arc = env->track_entity(orchestrator->create_entity());
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

        virtual bool create(entity e, poly_typeid hash, void * data) override final { return false; }

        virtual void destroy(entity e) override final {}

        void update(const uint64_t current_frame)
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

        entity get_teleportation_arc()
        {
            return should_draw ? teleportation_arc : kInvalidEntity;
        }

    };

    POLYMER_SETUP_TYPEID(vr_teleport_system);
}

struct viewport_t
{
    float2 bmin, bmax;
    GLuint texture;
};

struct sample_vr_app : public polymer_app
{
    std::unique_ptr<openvr_hmd> hmd;
    std::unique_ptr<gui::imgui_instance> desktop_imgui;
    std::unique_ptr<imgui_vr> vr_imgui;

    std::vector<viewport_t> viewports;
    std::vector<simple_texture_view> eye_views;

    gl_mesh empty_mesh;

    entity left_controller;
    entity right_controller;
    entity floor;

    float2 debug_pt;

    gl_shader_monitor shaderMonitor { "../../assets/" };

    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<vr_teleport_system> teleporter;

    render_payload payload;
    environment scene;

    renderable assemble_renderable(const entity e);

    uint64_t frame_count{ 0 };
    bool should_load{ true };

    sample_vr_app();
    ~sample_vr_app();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
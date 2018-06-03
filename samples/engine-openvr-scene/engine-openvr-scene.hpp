#pragma once

#include "lib-engine.hpp"
#include "lib-polymer.hpp"
#include "physics-controller.hpp"

using namespace polymer;

class imgui_surface
{
    std::unique_ptr<gui::imgui_instance> imgui;
    gl_framebuffer renderFramebuffer;
    gl_texture_2d renderTexture;
    uint2 framebufferSize;

public:

    imgui_surface(const uint2 size, GLFWwindow * window) : framebufferSize(size)
    {
        imgui.reset(new gui::imgui_instance(window));
        renderTexture.setup(size.x, size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);
        renderFramebuffer.check_complete();
    }

    gui::imgui_instance * get_instance() { return imgui.get(); }

    uint32_t get_render_texture() const { return renderTexture; }

    void begin_frame()
    {
        imgui->begin_frame(framebufferSize.x, framebufferSize.y);
    }

    uint2 get_size() const { return framebufferSize; }

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

public:

    imgui_vr(const uint2 size, GLFWwindow * window) : imgui_surface(size, window)
    {

    }

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
                const float4x4 flip = make_rotation_matrix({ 1, 0, 0 }, -POLYMER_PI / 2);
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
                        m = make_mesh_from_geometry(g);
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
            if (should_draw) return teleportation_arc;
            return kInvalidEntity;
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

    std::unique_ptr<imgui_surface> vr_imgui;
    std::shared_ptr<polymer_fx_material> imgui_material;

    std::vector<viewport_t> viewports;
    std::vector<simple_texture_view> eye_views;

    gl_mesh empty_mesh;

    entity left_controller;
    entity right_controller;
    entity imgui_billboard;
    entity pointer;
    entity floor;

    float2 debug_pt;

    gl_shader_monitor shaderMonitor { "../../assets/" };

    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<vr_teleport_system> teleporter;

    render_payload payload;
    environment scene;

    renderable assemble_renderable(const entity e);

    uint64_t frame_count{ 0 };

    sample_vr_app();
    ~sample_vr_app();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
#include "engine-openvr-scene.hpp"

renderable sample_vr_app::assemble_renderable(const entity e)
{
    renderable r;
    r.e = e;
    r.material = scene.render_system->get_material_component(e);
    r.mesh = scene.render_system->get_mesh_component(e);
    r.scale = scene.xform_system->get_local_transform(e)->local_scale;
    r.t = scene.xform_system->get_world_transform(e)->world_pose;
    return r;
}

sample_vr_app::sample_vr_app() : polymer_app(1280, 800, "sample-engine-openvr-scene")
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    desktop_imgui.reset(new gui::imgui_instance(window));
    gui::make_light_theme();

    try
    {
        hmd.reset(new openvr_hmd());
        glfwSwapInterval(0);

        orchestrator.reset(new entity_orchestrator());
        load_required_renderer_assets("../../assets", shaderMonitor);

        shaderMonitor.watch("textured",
            "../../assets/shaders/renderer/renderer_vert.glsl",
            "../../assets/shaders/renderer/textured_frag.glsl",
            "../../assets/shaders/renderer");

        shaderMonitor.watch("unlit-vertex-color",
            "../../assets/shaders/renderer/renderer_vert.glsl",
            "../../assets/shaders/renderer/unlit_vertex_color_frag.glsl",
            "../../assets/shaders/renderer");

        // Create required environment utilities
        scene.mat_library.reset(new polymer::material_library("../../assets/materials/"));
        scene.event_manager.reset(new polymer::event_manager_async());

        // Setup for the recommended eye target size
        const uint2 eye_target_size = hmd->get_recommended_render_target_size();
        renderer_settings settings;
        settings.renderSize = int2(eye_target_size.x, eye_target_size.y);
        settings.cameraCount = 2;

        // Create required systems
        scene.collision_system = orchestrator->create_system<collision_system>(orchestrator.get());
        scene.xform_system = orchestrator->create_system<transform_system>(orchestrator.get());
        scene.identifier_system = orchestrator->create_system<identifier_system>(orchestrator.get());
        scene.render_system = orchestrator->create_system<render_system>(settings, orchestrator.get());

        // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
        payload.skybox = scene.render_system->get_skybox();
        payload.sunlight = scene.render_system->get_implicit_sunlight();

        {
            create_handle_for_asset("floor-mesh", make_mesh_from_geometry(make_plane(48, 48, 24, 24))); // gpu mesh

            auto wiref_mat = std::make_shared<polymer_wireframe_material>();
            scene.mat_library->create_material("renderer-wireframe", wiref_mat);

            floor = scene.track_entity(orchestrator->create_entity());
            scene.identifier_system->create(floor, "floor-mesh");
            scene.xform_system->create(floor, transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, ((float) POLYMER_PI / 2.f)), { 0, -0.01f, 0 }), { 1.f, 1.f, 1.f });
            scene.render_system->create(floor, material_component(floor, material_handle("renderer-wireframe")));
            scene.render_system->create(floor, mesh_component(floor, gpu_mesh_handle("floor-mesh")));
        }

        input_processor.reset(new vr_input_processor(orchestrator.get(), &scene, hmd.get()));
        controller_system.reset(new vr_controller_system(orchestrator.get(), &scene, hmd.get(), input_processor.get()));
        gizmo_system.reset(new vr_gizmo(orchestrator.get(), &scene, hmd.get(), input_processor.get()));
        vr_imgui.reset(new vr_imgui_surface(orchestrator.get(), &scene, hmd.get(), input_processor.get(), { 256, 256 }, window));
        gui::make_light_theme();
    }
    catch (const std::exception & e)
    {
        std::cout << "Application Init Exception: " << e.what() << std::endl;
    }

    // Setup left/right eye debug view we see on the desktop window
    eye_views.push_back(simple_texture_view()); // for the left view
    eye_views.push_back(simple_texture_view()); // for the right view
}

sample_vr_app::~sample_vr_app()
{
    hmd.reset();
}

void sample_vr_app::on_window_resize(int2 size)
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
}

void sample_vr_app::on_input(const app_input_event & event) 
{
    desktop_imgui->update_input(event);
}

void sample_vr_app::on_update(const app_update_event & e)
{
    shaderMonitor.handle_recompile();

    hmd->update();
    scene.event_manager->process();

    input_processor->process(e.timestep_ms);
    controller_system->process(e.timestep_ms);
    gizmo_system->process(e.timestep_ms);
    vr_imgui->process(e.timestep_ms);

    // ImGui surface/billboard is attached to left controller
    auto left_controller_xform = hmd->get_controller(vr::TrackedControllerRole_LeftHand).t;
    left_controller_xform = left_controller_xform * transform(float4(0, 0, 0, 1), float3(0, 0, -.25f));
    left_controller_xform = left_controller_xform * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float) POLYMER_PI / 2.f), float3());
    left_controller_xform = left_controller_xform * transform(make_rotation_quat_axis_angle({ 0, 1, 0 }, (float) -POLYMER_PI), float3());
    vr_imgui->set_surface_transform(left_controller_xform);
}

void sample_vr_app::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Collect eye data for the render payload
    for (auto eye : { vr::Hmd_Eye::Eye_Left, vr::Hmd_Eye::Eye_Right })
    {
        const auto eye_pose = hmd->get_eye_pose(eye);
        const auto eye_projection = hmd->get_proj_matrix(eye, 0.075f, 64.f);
        payload.views.emplace_back(view_data(eye, eye_pose, eye_projection));
    }

    glDisable(GL_CULL_FACE);

    // Render scene using payload
    payload.render_set.clear();
    payload.render_set.push_back(assemble_renderable(floor));
    for (const entity & r : vr_imgui->get_renderables()) payload.render_set.push_back(assemble_renderable(r));
    for (const entity & r : controller_system->get_renderables()) payload.render_set.push_back(assemble_renderable(r));
    for (const entity & r : gizmo_system->get_renderables()) payload.render_set.push_back(assemble_renderable(r));
    scene.render_system->get_renderer()->render_frame(payload);

    const uint32_t left_eye_texture = scene.render_system->get_renderer()->get_color_texture(0);
    const uint32_t right_eye_texture = scene.render_system->get_renderer()->get_color_texture(1);

    // Render to the HMD
    hmd->submit(left_eye_texture, right_eye_texture);
    payload.views.clear();

    const aabb_2d rect{ { 0.f, 0.f },{ (float)width,(float)height } }; // Desktop window size
    const float mid = (rect.min().x + rect.max().x) / 2.f;
    const viewport_t leftViewport = { rect.min(),{ mid - 2.f, rect.max().y }, left_eye_texture };
    const viewport_t rightViewport = { { mid + 2.f, rect.min().y }, rect.max(), right_eye_texture };
    viewports.clear();
    viewports.push_back(leftViewport);
    viewports.push_back(rightViewport);

    if (viewports.size())
    {
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Draw to the desktop window
    for (int i = 0; i < viewports.size(); ++i)
    {
        const auto & v = viewports[i];
        glViewport(GLint(v.bmin.x), GLint(height - v.bmax.y), GLsizei(v.bmax.x - v.bmin.x), GLsizei(v.bmax.y - v.bmin.y));
        eye_views[i].draw(v.texture);
    }
    
    const auto headPose = hmd->get_hmd_pose();

    desktop_imgui->begin_frame();
    ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
    desktop_imgui->end_frame();

    vr_imgui->begin_frame();
    gui::imgui_fixed_window_begin("controls", ui_rect{ {0, 0,}, {256, 256} });
    ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
    ImGui::Text("Hit UV %f, %f", debug_pt.x, debug_pt.y);
    if (ImGui::Button("ImGui VR Button")) std::cout << "Click!" << std::endl;
    gui::imgui_fixed_window_end();
    vr_imgui->end_frame();

    glfwSwapBuffers(window);
    frame_count++;
    gl_check_error(__FILE__, __LINE__);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_vr_app app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

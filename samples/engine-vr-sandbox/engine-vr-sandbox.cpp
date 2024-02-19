#include "engine-vr-sandbox.hpp"

using namespace polymer::xr;

engine_vr_sandbox::engine_vr_sandbox() 
    : polymer_app(1280, 800, "sample-engine-vr-sandbox")
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    desktop_imgui.reset(new gui::imgui_instance(window));
    gui::make_light_theme();

    try
    {
        hmd.reset(new openvr_hmd());
        glfwSwapInterval(0);

        the_entity_system_manager.reset(new entity_system_manager());
        load_required_renderer_assets("../../assets", shaderMonitor);

        // Setup for the recommended eye target size
        const uint2 eye_target_size = hmd->get_recommended_render_target_size();

        the_scene.reset(*the_entity_system_manager, {eye_target_size.x, eye_target_size.y}, true);

        renderer_settings settings;
        settings.renderSize = int2(eye_target_size.x, eye_target_size.y);
        settings.cameraCount = 2;
        settings.performanceProfiling = true;
        the_scene.render_system->reconfigure(settings);

        // Setup hidden area mesh
        the_scene.render_system->get_renderer()->set_stencil_mask(0, hmd->get_stencil_mask(vr_eye::left_eye));
        the_scene.render_system->get_renderer()->set_stencil_mask(1, hmd->get_stencil_mask(vr_eye::right_eye));

        {
            create_handle_for_asset("floor-mesh", make_mesh_from_geometry(make_plane(48, 48, 24, 24))); // gpu mesh

            auto wiref_mat = std::make_shared<polymer_wireframe_material>();
            the_scene.mat_library->register_material("renderer-wireframe", wiref_mat);

            floor = the_scene.track_entity(the_entity_system_manager->create_entity());
            the_scene.identifier_system->create(floor, "floor-mesh");
            the_scene.xform_system->create(floor, transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, ((float) POLYMER_PI / 2.f)), { 0, -0.01f, 0 }), { 1.f, 1.f, 1.f });
            the_scene.render_system->create(floor, material_component(floor, material_handle("renderer-wireframe"))); 
            the_scene.render_system->create(floor, mesh_component(floor, gpu_mesh_handle("floor-mesh")));
        }

        input_processor.reset(new xr_input_processor(the_entity_system_manager.get(), &the_scene, hmd.get()));
        controller_system.reset(new xr_controller_system(the_entity_system_manager.get(), &the_scene, hmd.get(), input_processor.get()));
        gizmo_system.reset(new xr_gizmo_system(the_entity_system_manager.get(), &the_scene, hmd.get(), input_processor.get()));

        vr_imgui.reset(new xr_imgui_system(the_entity_system_manager.get(), &the_scene, hmd.get(), input_processor.get(), { 256, 256 }, window));

        for (const entity & r : vr_imgui->get_renderables())
        {
            input_processor->add_focusable(r);
        }

        the_scene.collision_system->queue_acceleration_rebuild();
    }
    catch (const std::exception & e)
    {
        std::cout << "Application Init Exception: " << e.what() << std::endl;
    }

    // Setup left/right eye debug view we see on the desktop window
    eye_views.push_back(simple_texture_view()); // for the left view
    eye_views.push_back(simple_texture_view()); // for the right view

    for (auto & e : the_scene.entity_list())
    {
        if (auto * cubemap = the_scene.render_system->get_cubemap_component(e)) payload.ibl_cubemap = cubemap;
    }

    for (auto & e : the_scene.entity_list())
    {
        if (auto * proc_skybox = the_scene.render_system->get_procedural_skybox_component(e))
        {
            payload.procedural_skybox = proc_skybox;
            if (auto sunlight = the_scene.render_system->get_directional_light_component(proc_skybox->sun_directional_light))
            {
                payload.sunlight = sunlight;
            }
        }
    }

    the_scene.resolver->add_search_path("../../assets/");
    the_scene.resolver->resolve();
}

engine_vr_sandbox::~engine_vr_sandbox()
{
    hmd.reset();
}

void engine_vr_sandbox::on_window_resize(int2 size)
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
}

void engine_vr_sandbox::on_input(const app_input_event & event) 
{
    desktop_imgui->update_input(event);
}

void engine_vr_sandbox::on_update(const app_update_event & e)
{
    shaderMonitor.handle_recompile();

    the_scene.event_manager->process();

	hmd->update();
    input_processor->process(e.timestep_ms);
    controller_system->process(e.timestep_ms);
    gizmo_system->process(e.timestep_ms);
    vr_imgui->process(e.timestep_ms);

    // ImGui surface/billboard is attached to left controller
    auto left_controller_xform = hmd->get_controller(vr_controller_role::left_hand).t;
    left_controller_xform = left_controller_xform * transform(quatf(0, 0, 0, 1), float3(0, 0, -.25f));
    left_controller_xform = left_controller_xform * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float) POLYMER_PI / 2.f), float3());
    left_controller_xform = left_controller_xform * transform(make_rotation_quat_axis_angle({ 0, 1, 0 }, (float) -POLYMER_PI), float3());
    vr_imgui->set_surface_transform(left_controller_xform);
}

void engine_vr_sandbox::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Collect eye data for the render payload, always remembering to clear the payload first
    payload.views.clear();
    for (auto eye : { vr_eye::left_eye, vr_eye::right_eye })
    {
        const auto eye_pose = hmd->get_eye_pose(eye);
        const auto eye_projection = hmd->get_proj_matrix(eye, 0.075f, 128.f);
        payload.views.emplace_back(view_data(static_cast<uint32_t>(eye), eye_pose, eye_projection));
    }

    glDisable(GL_CULL_FACE);

    // Render scene using payload
    payload.render_components.clear();
    payload.render_components.push_back(assemble_render_component(the_scene, floor));
    for (const entity & r : vr_imgui->get_renderables()) payload.render_components.push_back(assemble_render_component(the_scene, r));
    for (const entity & r : controller_system->get_renderables()) payload.render_components.push_back(assemble_render_component(the_scene, r));
    for (const entity & r : gizmo_system->get_renderables()) payload.render_components.push_back(assemble_render_component(the_scene, r));
    the_scene.render_system->get_renderer()->render_frame(payload);

    const uint32_t left_eye_texture = the_scene.render_system->get_renderer()->get_color_texture(0);
    const uint32_t right_eye_texture = the_scene.render_system->get_renderer()->get_color_texture(1);

    // Submit to the HMD for presentation
    hmd->submit(left_eye_texture, right_eye_texture);

    // Draw eye textures to the desktop window in left/right configuration
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

    for (int i = 0; i < viewports.size(); ++i)
    {
        const auto & v = viewports[i];
        glViewport(GLint(v.bmin.x), GLint(height - v.bmax.y), GLsizei(v.bmax.x - v.bmin.x), GLsizei(v.bmax.y - v.bmin.y));
        eye_views[i].draw(v.texture);
    }
    
    // Setup desktop imgui
    const auto headPose = hmd->get_hmd_pose();
    desktop_imgui->begin_frame();
    ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
    if (the_scene.render_system->get_renderer()->settings.performanceProfiling)
    {
        for (auto & t : the_scene.render_system->get_renderer()->gpuProfiler.get_data()) ImGui::Text("[Renderer GPU] %s %f ms", t.first.c_str(), t.second);
    }
    desktop_imgui->end_frame();

    // Setup vr imgui
    vr_imgui->begin_frame();
    gui::imgui_fixed_window_begin("controls", ui_rect{ {0, 0,}, {256, 256} });
    ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
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
        engine_vr_sandbox app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

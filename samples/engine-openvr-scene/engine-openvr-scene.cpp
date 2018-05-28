#include "engine-openvr-scene.hpp"

sample_vr_app::sample_vr_app() : polymer_app(1280, 800, "sample-engine-openvr-scene")
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    desktop_imgui.reset(new gui::imgui_instance(window));
    gui::make_light_theme();

    try
    {
        hmd.reset(new openvr_hmd());

        vr_imgui.reset(new imgui_surface({ 1024, 1024 }, window));

        orchestrator.reset(new entity_orchestrator());
        load_required_renderer_assets("../../assets/", shaderMonitor);

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


        glfwSwapInterval(0);
    }
    catch (const std::exception & e)
    {
        std::cout << "Application Init Exception: " << e.what() << std::endl;
    }

    // Setup left/right eye debug view we see on the desktop window
    eye_views.push_back(simple_texture_view());
    eye_views.push_back(simple_texture_view());
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
    vr_imgui->get_instance()->update_input(event);
}

void sample_vr_app::on_update(const app_update_event & e) 
{
    shaderMonitor.handle_recompile();

    hmd->update();

    hmd->get_controller(vr::TrackedControllerRole_LeftHand)->get_pose(hmd->get_world_pose());
    hmd->get_controller(vr::TrackedControllerRole_RightHand)->get_pose(hmd->get_world_pose());

    std::vector<openvr_controller::button_state> trackpadStates = {
        hmd->get_controller(vr::TrackedControllerRole_LeftHand)->pad,
        hmd->get_controller(vr::TrackedControllerRole_RightHand)->pad
    };

    std::vector<openvr_controller::button_state> triggerStates = {
        hmd->get_controller(vr::TrackedControllerRole_LeftHand)->trigger,
        hmd->get_controller(vr::TrackedControllerRole_RightHand)->trigger
    };
}

void sample_vr_app::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Collect eye data
    for (auto eye : { vr::Hmd_Eye::Eye_Left, vr::Hmd_Eye::Eye_Right })
    {
        const auto eye_pose = hmd->get_eye_pose(eye);
        const auto eye_projection = hmd->get_proj_matrix(eye, 0.05, 32.f);
        payload.views.emplace_back(view_data(eye, eye_pose, eye_projection));
    }

    // Render scene using payload
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
        glViewport(v.bmin.x, height - v.bmax.y, v.bmax.x - v.bmin.x, v.bmax.y - v.bmin.y);
        eye_views[i].draw(v.texture);
    }

    desktop_imgui->begin_frame();
    const auto headPose = hmd->get_hmd_pose();
    ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
    desktop_imgui->end_frame();

    glfwSwapBuffers(window);

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

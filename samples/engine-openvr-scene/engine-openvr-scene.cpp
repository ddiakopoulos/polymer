#include "engine-openvr-scene.hpp"

sample_vr_app::sample_vr_app() : polymer_app(1280, 800, "sample-engine-openvr-scene")
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    imgui.reset(new gui::imgui_instance(window));

    try
    {
        hmd.reset(new openvr_hmd());
        const uint2 targetSize = hmd->get_recommended_render_target_size();
        glfwSwapInterval(0);
    }
    catch (const std::exception & e)
    {
        std::cout << "OpenVR Exception: " << e.what() << std::endl;
    }
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
    imgui->update_input(event);
}

void sample_vr_app::on_update(const app_update_event & e) 
{
    shaderMonitor.handle_recompile();

    if (hmd)
    {
        //scene.leftController->update(hmd->get_controller(vr::TrackedControllerRole_LeftHand)->get_pose(hmd->get_world_pose()));
        //scene.rightController->update(hmd->get_controller(vr::TrackedControllerRole_RightHand)->get_pose(hmd->get_world_pose()));

        std::vector<openvr_controller::button_state> trackpadStates = {
            hmd->get_controller(vr::TrackedControllerRole_LeftHand)->pad,
            hmd->get_controller(vr::TrackedControllerRole_RightHand)->pad
        };

        std::vector<openvr_controller::button_state> triggerStates = {
            hmd->get_controller(vr::TrackedControllerRole_LeftHand)->trigger,
            hmd->get_controller(vr::TrackedControllerRole_RightHand)->trigger
        };
    }


}

void sample_vr_app::on_draw()
{
    glfwMakeContextCurrent(window);

    imgui->begin_frame();

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    /*
    aabb_2d rect{ { 0.f, 0.f },{ (float)width,(float)height } };
    const float mid = (rect.min().x + rect.max().x) / 2.f;
    viewport_t leftviewport = { rect.min(),{ mid - 2.f, rect.max().y }, renderer->get_eye_texture(Eye::LeftEye) };
    viewport_t rightViewport = { { mid + 2.f, rect.min().y }, rect.max(), renderer->get_eye_texture(Eye::RightEye) };
    viewports.clear();
    viewports.push_back(leftviewport);
    viewports.push_back(rightViewport);
    */

    if (viewports.size())
    {
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    for (auto & v : viewports)
    {
        glViewport(v.bmin.x, height - v.bmax.y, v.bmax.x - v.bmin.x, v.bmax.y - v.bmin.y);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, v.texture);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f(+1, -1);
        glTexCoord2f(1, 1); glVertex2f(+1, +1);
        glTexCoord2f(0, 1); glVertex2f(-1, +1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

    if (hmd)
    {
        const auto headPose = hmd->get_hmd_pose();
        ImGui::Text("Head Pose: %f, %f, %f", headPose.position.x, headPose.position.y, headPose.position.z);
    }

    imgui->end_frame();

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

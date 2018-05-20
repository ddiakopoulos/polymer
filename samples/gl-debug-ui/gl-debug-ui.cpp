/*
 * File: samples/gl-debug-ui.cpp
 * 
 * 
 * 
 * 
 * 
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "gl-gizmo.hpp"
#include "gl-nvg.hpp"
#include "gl-imgui.hpp"
#include "gl-renderable-grid.hpp"
#include "shader-library.hpp"
#include "environment.hpp"

using namespace polymer;
using namespace gui;

struct sample_gl_debug_ui final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    gl_renderable_grid grid{ 1.f, 24, 24 };

    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_gizmo> gizmo;

    tinygizmo::rigid_transform gizmo_selection;

    sample_gl_debug_ui();
    ~sample_gl_debug_ui();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_debug_ui::sample_gl_debug_ui() : polymer_app(1280, 720, "sample-gl-debug-ui")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gizmo.reset(new gl_gizmo());

    imgui.reset(new gui::imgui_instance(window, true)); 
    gui::make_light_theme();

    cam.look_at({ 0, 5, 5 }, { 0, 0.1f, -0.1f });
    flycam.set_camera(&cam);
}

sample_gl_debug_ui::~sample_gl_debug_ui() {}

void sample_gl_debug_ui::on_window_resize(int2 size) {}

void sample_gl_debug_ui::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.handle_input(event);
    gizmo->handle_input(event);
    imgui->update_input(event);
}

void sample_gl_debug_ui::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    gizmo->update(cam, float2(width, height));
}

void sample_gl_debug_ui::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    // Draw the floor grid
    grid.draw(viewProjectionMatrix);

    tinygizmo::transform_gizmo("debug-gizmo", gizmo->gizmo_ctx, gizmo_selection);
    const transform gizmo_pose = to_linalg(gizmo_selection);

    imgui->begin_frame();

    gui::imgui_fixed_window_begin("sample-debug-ui", { { 0, 0 },{ 320, height } });
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Gizmo Position {%.3f, %.3f, %.3f}", gizmo_pose.position.x, gizmo_pose.position.y, gizmo_pose.position.z);
    if (ImGui::Button("Reset Gizmo"))
    {
        gizmo_selection = {};
    }
    gui::imgui_fixed_window_end();

    // Render imgui
    imgui->end_frame();

    // Render the gizmo
    gizmo->draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_debug_ui app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
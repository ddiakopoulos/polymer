/*
 * File: samples/gl-spline-2d.cpp
 */

#include "lib-polymer.hpp"
#include "gl-loaders.hpp"
#include "gl-nvg.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"
#include "shader-library.hpp"
#include "environment.hpp"

using namespace polymer;
using namespace gui;

struct sample_gl_spline_2d final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_nvg_surface> surface;
    std::unique_ptr<simple_texture_view> view;

    sample_gl_spline_2d();
    ~sample_gl_spline_2d();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_spline_2d::sample_gl_spline_2d() : polymer_app(1280, 720, "sample-gl-spline-2d")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gl_nvg_surface::font_data data;
    data.text_font_name = "droid-sans";
    data.text_font_binary = read_file_binary("../../assets/fonts/source_code_pro_regular.ttf");

    surface.reset(new gl_nvg_surface(1, { (float) width, (float) height }, data));

    view.reset(new simple_texture_view());

    imgui.reset(new gui::imgui_instance(window, true));
    gui::make_light_theme();
}

sample_gl_spline_2d::~sample_gl_spline_2d() {}

void sample_gl_spline_2d::on_window_resize(int2 size) {}

void sample_gl_spline_2d::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    imgui->update_input(event);
}

void sample_gl_spline_2d::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
}

void sample_gl_spline_2d::on_draw()
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

    // Render the offscreen nvg surface
    {
        std::string text = "Polymer Engine";

        NVGcontext * nvg = surface->pre_draw(window, 0);
        const float2 size = surface->surface_size();

        nvgSave(nvg);

        nvgBeginPath(nvg);
        nvgRect(nvg, 0, 0, size.x, size.y);
        nvgFillColor(nvg, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
        nvgFill(nvg);

        surface->draw_text_quick(text, 120, float2(size.x / 2, size.y / 2), nvgRGBAf(1, 1, 1, 1));

        nvgRestore(nvg);

        surface->post_draw();
    }

    // Reset state changed by nanovg
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    view->draw(surface->surface_texture(0));

    imgui->begin_frame();
    gui::imgui_fixed_window_begin("debug-ui", { { 0, 0 },{ 320, height } });
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    gui::imgui_fixed_window_end();
    imgui->end_frame();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_spline_2d app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
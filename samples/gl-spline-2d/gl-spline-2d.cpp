/*
 * File: samples/gl-spline-2d.cpp
 */

#include "lib-polymer.hpp"
#include "gl-loaders.hpp"
#include "gl-nvg.hpp"
#include "gl-imgui.hpp"
#include "gl-texture-view.hpp"
#include "shader-library.hpp"
#include "splines.hpp"
#include "scene.hpp"

using namespace polymer;
using namespace gui;

struct sample_gl_spline_2d final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_nvg_surface> surface;
    std::unique_ptr<simple_texture_view> view;

    sample_gl_spline_2d();
    ~sample_gl_spline_2d();

    bezier_spline curve;
    std::vector<float3> control_points;

    float current_solution{ std::numeric_limits<float>::signaling_NaN() };

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

    if (event.type == app_input_event::MOUSE)
    {
        if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT && event.action == GLFW_RELEASE)
        {
            if (control_points.size() < 4)
            {
                control_points.push_back(float3(event.cursor.x, event.cursor.y, 0));
            }
        }
    }

    if (event.type == app_input_event::KEY)
    {
        if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
        {
            control_points.clear();
            curve = {};
        }
    }

    if (event.type == app_input_event::CURSOR)
    {
        if (control_points.size() == 4)
        {
            // https://pomax.github.io/bezierinfo/#yforx
            // The root finder is based on normal x/y coordinates,
            // so we can "trick" it by giving it "t" values as x
            // values, and "x" values as y values. Since it won't
            // even look at the x dimension, we can also just leave it.
            bezier_spline copy;
            const auto ctrl_pts = curve.get_control_points();
            copy.set_control_points(
                float3(ctrl_pts[0].x, ctrl_pts[0].x - event.cursor.x, 0),
                float3(ctrl_pts[1].x, ctrl_pts[1].x - event.cursor.x, 0),
                float3(ctrl_pts[2].x, ctrl_pts[2].x - event.cursor.x, 0),
                float3(ctrl_pts[3].x, ctrl_pts[3].x - event.cursor.x, 0));

            auto y_coeffs = copy.get_cubic_coefficients(1);

            std::vector<double> solutions = { 0.f, 0.f, 0.f };
            int num_solutions = solve_cubic(y_coeffs.x, y_coeffs.y, y_coeffs.z, y_coeffs.w, solutions[0], solutions[1], solutions[2]);
            //std::cout << "Num Solutions: " << num_solutions << std::endl;
            //std::cout << "A:             " << solutions[0] << std::endl;
            //std::cout << "B:             " << solutions[1] << std::endl;
            //std::cout << "C:             " << solutions[2] << std::endl;

            for (auto s : solutions)
            {
                if (s >= -1.f && s <= 1.f)
                {
                    current_solution = (float) s;
                    std::cout << "Solution: " << s << std::endl;
                }
            }
        }
    }
}

void sample_gl_spline_2d::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (control_points.size() == 4)
    {
        curve.set_control_points(control_points[0], control_points[1], control_points[2], control_points[3]);
    }
}

void sample_gl_spline_2d::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(16.f / 255.f, 13.f / 255.f, 40.f / 255.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    {
        NVGcontext * nvg = surface->pre_draw(window, 0);
        const float2 size = surface->surface_size();

        nvgSave(nvg);

        // Draw control points
        for (auto & pt : control_points)
        {
            nvgBeginPath(nvg);
            nvgEllipse(nvg, pt.x, pt.y, 24, 24);
            nvgFillColor(nvg, nvgRGBAf(238.f / 255.f, 91.f / 255.f, 94.f / 255.f, 1.f));
            nvgFill(nvg);
        }

        // Evaluate the curve
        for (float t = 0.f; t <= 1.f; t += 0.05f)
        {
            auto pt = curve.evaluate(t);
            nvgBeginPath(nvg);
            nvgEllipse(nvg, pt.x, pt.y, 8, 8);
            nvgFillColor(nvg, nvgRGBAf(252.f / 255.f, 231.f / 255.f, 169.f / 255.f, 1.f));
            nvgFill(nvg);
        }

        if (current_solution != std::numeric_limits<float>::signaling_NaN())
        {
            auto pt = curve.evaluate(std::abs(current_solution));
            nvgBeginPath(nvg);
            nvgEllipse(nvg, pt.x, pt.y, 16, 16);
            nvgFillColor(nvg, nvgRGBAf(184.f / 255.f, 55.f / 255.f, 125.f / 255.f, 1.f));
            nvgFill(nvg);

            surface->draw_text_quick(std::to_string(std::abs(current_solution)), 28, float2(pt.x, pt.y - 58), nvgRGBAf(1, 1, 1, 1));
        }

        nvgRestore(nvg);

        surface->post_draw();
    }

    // Reset state changed by nanovg
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    view->draw(surface->surface_texture(0));

    //imgui->begin_frame();
    //gui::imgui_fixed_window_begin("debug-ui", { { 0, 0 },{ 320, height } });
    //ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    //gui::imgui_fixed_window_end();
    //imgui->end_frame();

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
/*
 * File: samples/gl-debug-ui.cpp
 * This sample demonstrates the use of Polymer's lowest-level OpenGL API. Polymer
 * uses sevel OpenGL 4.5 features under the hood, namely the direct state access (DSA)
 * extension to simplify the implementation of GL wrapper types. This sample uses 
 * `lib-model-io` to load an obj file and apply a rendering technique known as matcap 
 * shading, a texture-only based approach without any actual scene lighting.
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "shader-library.hpp"
#include "environment.hpp"

using namespace polymer;

struct sample_gl_debug_ui final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

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

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_gl_debug_ui::~sample_gl_debug_ui() {}

void sample_gl_debug_ui::on_window_resize(int2 size) {}

void sample_gl_debug_ui::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.handle_input(event);
}

void sample_gl_debug_ui::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
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
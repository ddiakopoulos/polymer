/*
 * File: samples/
 */

#include "index.hpp"
#include "gl-loaders.hpp"

using namespace polymer;

struct sample_gl_camera_trajectory final : public polymer_app
{
    perspective_camera cam;

    sample_gl_camera_trajectory();
    ~sample_gl_camera_trajectory();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_camera_trajectory::sample_gl_camera_trajectory() 
    : polymer_app(1280, 720, "sample-gl-camera-trajectory")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
}

sample_gl_camera_trajectory::~sample_gl_camera_trajectory() {}

void sample_gl_camera_trajectory::on_window_resize(int2 size) {}

void sample_gl_camera_trajectory::on_input(const app_input_event & event)
{

}

void sample_gl_camera_trajectory::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
}

void sample_gl_camera_trajectory::on_draw()
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
        sample_gl_camera_trajectory app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

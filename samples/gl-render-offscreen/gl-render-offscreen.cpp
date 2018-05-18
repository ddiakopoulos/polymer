/*
 * File: samples/gl-render-offscreen.cpp
 * This sample demonstrates how to setup and render to a framebuffer. This framebuffer
 * is then drawn as a full-screen quad using the `simple_texture_view` utility class. 
 * The rendered meshes are all generated procedurally using Polymer's built-in mesh
 * classes. For illustrative reasons, the CPU geometry is not attached to a gl_mesh (GPU)
 * but instead rendered through the use of Polymer's `renderer_debug` found in lib-engine.
 * Lastly, the user can click a mesh to highlight it, showing how to raycast against CPU-side
 * geometry. 
 */

#include "index.hpp"
#include "gl-camera.hpp"
#include "gl-texture-view.hpp"

using namespace polymer;

struct sample_gl_render_offscreen final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<simple_texture_view> view;

    gl_texture_2d renderTextureRGBA;
    gl_texture_2d renderTextureDepth;
    gl_framebuffer renderFramebuffer;

    sample_gl_render_offscreen();
    ~sample_gl_render_offscreen();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_render_offscreen::sample_gl_render_offscreen() : polymer_app(1280, 720, "sample-gl-render-offscreen")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    view.reset(new simple_texture_view());

    renderTextureRGBA.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    renderTextureDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTextureRGBA, 0);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderTextureDepth, 0);
    renderFramebuffer.check_complete();

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_gl_render_offscreen::~sample_gl_render_offscreen()
{ 

}

void sample_gl_render_offscreen::on_window_resize(int2 size)
{ 

}

void sample_gl_render_offscreen::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_gl_render_offscreen::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);;
}

void sample_gl_render_offscreen::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render to framebuffer
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
        const float4x4 viewMatrix = cam.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
    }

    // Render to default framebuffer (the screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    view->draw(renderTextureRGBA);

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_render_offscreen app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
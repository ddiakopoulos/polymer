#pragma once

#include "lib-engine.hpp"
#include "lib-polymer.hpp"
#include "physics-controller.hpp"

using namespace polymer;

class imgui_surface
{
    std::unique_ptr<gui::imgui_instance> imgui;
    gl_framebuffer renderFramebuffer;
    gl_texture_2d renderTexture;
    uint2 framebufferSize;

public:

    imgui_surface(const uint2 size, GLFWwindow * window) : framebufferSize(size)
    {
        imgui.reset(new gui::imgui_instance(window));
        renderTexture.setup(size.x, size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);
        renderFramebuffer.check_complete();
    }

    gui::imgui_instance * get_instance() { return imgui.get(); }

    void begin_frame()
    {
        imgui->begin_frame(framebufferSize.x, framebufferSize.y);
    }

    void end_frame()
    {
        // Save framebuffer state
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint drawFramebuffer = 0, readFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glViewport(0, 0, (GLsizei)framebufferSize.x, (GLsizei)framebufferSize.y);

        glClear(GL_COLOR_BUFFER_BIT);
        imgui->end_frame();

        // Restore framebuffer state
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    }
};

struct viewport_t
{
    float2 bmin, bmax;
    GLuint texture;
};

struct sample_vr_app : public polymer_app
{
    std::unique_ptr<openvr_hmd> hmd;
    std::unique_ptr<gui::imgui_instance> desktop_imgui;
    std::unique_ptr<imgui_surface> vr_imgui;

    std::vector<viewport_t> viewports;
    std::vector<simple_texture_view> eye_views;

    gl_shader_monitor shaderMonitor { "../../assets/" };

    std::unique_ptr<entity_orchestrator> orchestrator;

    render_payload payload;
    environment scene;

    sample_vr_app();
    ~sample_vr_app();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
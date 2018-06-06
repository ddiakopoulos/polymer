#pragma once

#ifndef asset_browser_hpp
#define asset_browser_hpp

#include "material.hpp"
#include "renderer-pbr.hpp"
#include "asset-handle-utils.hpp"
#include "gizmo-controller.hpp"
#include "environment.hpp"
#include "editor-inspector-ui.hpp"
#include "arcball.hpp"
#include "gl-texture-view.hpp"
#include "win32.hpp"
#include "asset-import.hpp"

struct asset_browser_window final : public glfw_window
{
    std::unique_ptr<gui::imgui_instance> auxImgui;

    asset_browser_window(gl_context * context,
        int w, int h,
        const std::string & title,
        int samples) : glfw_window(context, w, h, title, samples)
    {
        glfwMakeContextCurrent(window);

        auxImgui.reset(new gui::imgui_instance(window, true));
        gui::make_light_theme();
    }

    virtual void on_input(const polymer::app_input_event & e) override final
    {
        if (e.window == window) auxImgui->update_input(e);
        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) return;
    }

    virtual void on_drop(std::vector<std::string> names) override final
    {
        for (auto path : names)
        {
            // ...
        }
    }

    virtual void on_window_close() override final
    {
        glfwMakeContextCurrent(window);
        auxImgui.reset();

        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }

    void run()
    {
        if (!window) return;
        if (!glfwWindowShouldClose(window))
        {
            glfwMakeContextCurrent(window);

            int width, height;
            glfwGetWindowSize(window, &width, &height);

            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auxImgui->begin_frame();
            gui::imgui_fixed_window_begin("asset-browser", { { 0, 0 },{ width, height } });
            gui::imgui_fixed_window_end();
            auxImgui->end_frame();

            glFlush();

            glfwSwapBuffers(window);
        }
    }

    ~asset_browser_window()
    {
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
    }
};

#endif // end asset_browser_hpp

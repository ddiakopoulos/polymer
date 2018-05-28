#pragma once

#include "lib-engine.hpp"
#include "lib-polymer.hpp"
#include "physics-controller.hpp"

using namespace polymer;

struct viewport_t
{
    float2 bmin, bmax;
    GLuint texture;
};

struct sample_vr_app : public polymer_app
{
    std::unique_ptr<openvr_hmd> hmd;
    std::unique_ptr<gui::imgui_instance> imgui;

    gl_shader_monitor shaderMonitor { "../../assets/" };
    std::vector<viewport_t> viewports;

    sample_vr_app();
    ~sample_vr_app();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
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
    std::unique_ptr<gui::imgui_instance> desktop_imgui;
    std::unique_ptr<vr_imgui_surface> vr_imgui;

    std::vector<viewport_t> viewports;
    std::vector<simple_texture_view> eye_views;

    gl_mesh empty_mesh;
    entity floor;

    float2 debug_pt;

    gl_shader_monitor shaderMonitor { "../../assets/" };

    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<vr_input_processor> input_processor;
    std::unique_ptr<vr_controller_system> controller_system;

    render_payload payload;
    environment scene;

    renderable assemble_renderable(const entity e);

    uint64_t frame_count{ 0 };
    bool should_load{ true };

    sample_vr_app();
    ~sample_vr_app();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
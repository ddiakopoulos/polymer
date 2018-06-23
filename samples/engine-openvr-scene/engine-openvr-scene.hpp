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
    gl_shader_monitor shaderMonitor{ "../../assets/" };

    std::unique_ptr<openvr_hmd> hmd;
    std::unique_ptr<gui::imgui_instance> desktop_imgui;
    std::unique_ptr<entity_orchestrator> orchestrator;

    std::unique_ptr<polymer::xr::xr_input_processor> input_processor;
    std::unique_ptr<polymer::xr::xr_controller_system> controller_system;
    std::unique_ptr<polymer::xr::xr_imgui_system> vr_imgui;
    std::unique_ptr<polymer::xr::xr_gizmo_system> gizmo_system;

    std::vector<viewport_t> viewports;
    std::vector<simple_texture_view> eye_views;

    uint64_t frame_count{ 0 };
    float2 debug_pt;
    entity floor;

    render_payload payload;
    environment scene;

    sample_vr_app();
    ~sample_vr_app();

    renderable assemble_renderable(const entity e);
    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
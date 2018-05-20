/*
 * File: samples/gl-debug-ui.cpp
 * 
 * 
 * 
 * 
 * 
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "gl-gizmo.hpp"
#include "gl-nvg.hpp"
#include "gl-imgui.hpp"
#include "gl-renderable-grid.hpp"
#include "shader-library.hpp"
#include "environment.hpp"

using namespace polymer;
using namespace gui;

constexpr const char textured_vert[] = R"(#version 330
    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inNormal;
    layout(location = 2) in vec3 inVertexColor;
    layout(location = 3) in vec2 inTexcoord;
    uniform mat4 u_mvp;
    out vec2 v_texcoord;
    void main()
    {
	    gl_Position = u_mvp * vec4(inPosition.xyz, 1);
        v_texcoord = inTexcoord;
    }
)";

constexpr const char textured_frag[] = R"(#version 330
    uniform sampler2D s_texture;
    in vec2 v_texcoord;
    out vec4 f_color;
    void main()
    {
        f_color = texture(s_texture, vec2(v_texcoord.x, v_texcoord.y));
    }
)";


struct sample_gl_debug_ui final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    gl_renderable_grid grid{ 1.f, 24, 24 };
    gl_mesh quad_mesh;
    gl_shader nvg_surface_shader;

    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_gizmo> gizmo;
    std::unique_ptr<gl_nvg_surface> surface;

    tinygizmo::rigid_transform gizmo_selection;

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

    gl_nvg_surface::font_data data;
    data.text_font_name = "droid-sans";
    data.text_font_binary = read_file_binary("../../assets/fonts/source_code_pro_regular.ttf");

    surface.reset(new gl_nvg_surface(1, { 1024.f, 1024.f }, data));

    gizmo.reset(new gl_gizmo());

    imgui.reset(new gui::imgui_instance(window, true)); 
    gui::make_light_theme();

    quad_mesh = make_plane_mesh(2.f, 1.f, 4, 4, true);
    nvg_surface_shader = gl_shader(textured_vert, textured_frag);

    cam.look_at({ 0, 5, 5 }, { 0, 0.1f, -0.1f });
    flycam.set_camera(&cam);
}

sample_gl_debug_ui::~sample_gl_debug_ui() {}

void sample_gl_debug_ui::on_window_resize(int2 size) {}

void sample_gl_debug_ui::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.handle_input(event);
    gizmo->handle_input(event);
    imgui->update_input(event);
}

void sample_gl_debug_ui::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    gizmo->update(cam, float2(width, height));
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

    // Render the offscreen nvg surface
    {
        std::string text = "POLYMER";
    
        NVGcontext * nvg = surface->pre_draw(window, 0);
        const float2 size = surface->surface_size();
    
        nvgSave(nvg);
    
        nvgBeginPath(nvg);
        nvgRect(nvg, 0, 0, size.x, size.y);
        nvgFillColor(nvg, nvgRGBAf(0.1f, 0.1f, 0.1f, 1.f));
        nvgFill(nvg);
    
        surface->draw_text_quick(text, (size.x), (size.y), nvgRGBAf(1, 1, 1, 1));
    
        nvgRestore(nvg);
    
        surface->post_draw();
    }
    
    // Reset state changed by nanovg
    glViewport(0, 0, width, height);

    // Render the offscreen nvg surface in the world
    {
        nvg_surface_shader.bind();
        nvg_surface_shader.uniform("u_mvp", viewProjectionMatrix);
        nvg_surface_shader.texture("s_texture", 0, surface->surface_texture(0), GL_TEXTURE_2D);
        quad_mesh.draw_elements();
        nvg_surface_shader.unbind();
    }

    // Draw the floor grid
    grid.draw(viewProjectionMatrix);

    tinygizmo::transform_gizmo("debug-gizmo", gizmo->gizmo_ctx, gizmo_selection);
    const transform gizmo_pose = to_linalg(gizmo_selection);

    imgui->begin_frame();

    gui::imgui_fixed_window_begin("sample-debug-ui", { { 0, 0 },{ 320, height } });
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Gizmo Position {%.3f, %.3f, %.3f}", gizmo_pose.position.x, gizmo_pose.position.y, gizmo_pose.position.z);
    if (ImGui::Button("Reset Gizmo"))
    {
        gizmo_selection = {};
    }
    gui::imgui_fixed_window_end();

    // Render imgui
    imgui->end_frame();

    // Render the gizmo
    gizmo->draw();

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
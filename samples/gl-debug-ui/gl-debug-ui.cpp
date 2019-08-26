/*
* File: samples/gl-debug-ui.cpp
* This sample demonstrates a variety of rendering utilities. First, it makes
* use of a `gl_gizmo`, helpful for grabbing and orienting scene objects. Secondly,
* it shows how to configure and render a Dear ImGui instance as a debug user interface.
* Thirdly, NanoVG is used to render an offscreen surface (`gl_nvg_surface`) with some
* basic text. This surface is then drawn on a small quad, but also used as a cookie
* texture such that it can be projected on any arbitrary geometry using projective texturing.
* The gizmo uses hotkeys (ctrl-w, ctrl-e, ctrl-r) to control position, orientation, and scaling.
*/

#include "lib-polymer.hpp"

#include "gl-loaders.hpp"
#include "gl-gizmo.hpp"
#include "gl-nvg.hpp"
#include "gl-imgui.hpp"
#include "gl-renderable-grid.hpp"

#include "camera-controllers.hpp"
#include "shader-library.hpp"
#include "scene.hpp"

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

class gl_projective_texture
{
    gl_shader shader;

public:

    gl_projective_texture() = default;

    void set_shader(gl_shader && s) { shader = std::move(s); }
    gl_shader & get_shader() { return shader; }

    const float4x4 get_view_projection_matrix(const float4x4 & modelViewMatrix, bool isOrthographic = false)
    {
        if (isOrthographic)
        {
            constexpr float halfSize = 1.0 * 0.5f;
            return (make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize) * modelViewMatrix);
        }
        return (make_projection_matrix(to_radians(45.f), 1.0f, 0.1f, 16.f) * modelViewMatrix);
    }

    // Transforms a position into projective texture space.
    // This matrix combines the light view, projection and bias matrices.
    const float4x4 get_projector_matrix(const float4x4 & modelViewMatrix, bool isOrthographic = false)
    {
        // Bias matrix is a constant.
        // It performs a linear transformation to go from the [�1, 1]
        // range to the [0, 1] range. Having the coordinates in the [0, 1]
        // range is necessary for the values to be used as texture coordinates.
        constexpr float4x4 biasMatrix = {
            { 0.5f,  0.0f,  0.0f,  0.0f },
            { 0.0f,  0.5f,  0.0f,  0.0f },
            { 0.0f,  0.0f,  0.5f,  0.0f },
            { 0.5f,  0.5f,  0.5f,  1.0f }
        };

        return biasMatrix * get_view_projection_matrix(modelViewMatrix, isOrthographic);
    }
};

struct sample_gl_debug_ui final : public polymer_app
{
    camera_controller_orbit cam;

    gl_renderable_grid grid{ 0.5f, 24, 24 };
    int which_cookie = 0;

    gl_mesh box_mesh;
    gl_mesh quad_mesh;
    gl_shader nvg_surface_shader;
    gl_shader surface_shader;
    gl_shader projector_shader;
    gl_texture_2d cookie;

    gl_projective_texture projector;

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

    quad_mesh = make_plane_mesh(2.f, 2.f, 4, 4, true);
    nvg_surface_shader = gl_shader(textured_vert, textured_frag);

    box_mesh = make_cube_mesh();

    projector_shader = gl_shader(
        read_file_text("../../assets/shaders/prototype/projector_multiply_vert.glsl"),
        read_file_text("../../assets/shaders/prototype/projector_multiply_frag.glsl"));

    cookie = load_image("../../assets/textures/projector/hexagon_select.png", false);
    glTextureParameteriEXT(cookie, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTextureParameteriEXT(cookie, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    projector.set_shader(std::move(projector_shader));

    gizmo_selection.position = { 0, 6, -2 };
}

sample_gl_debug_ui::~sample_gl_debug_ui() {}

void sample_gl_debug_ui::on_window_resize(int2 size) {}

void sample_gl_debug_ui::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    cam.handle_input(event);
    gizmo->handle_input(event);
    imgui->update_input(event);
}

void sample_gl_debug_ui::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    cam.update(e.timestep_ms);

    // Update tinygizmo, but wrap orbit camera in a regular perspective camera
    {
        perspective_camera persp_cam;
        persp_cam.nearclip = cam.near_clip;
        persp_cam.farclip = cam.far_clip;
        persp_cam.vfov = cam.yfov;
        persp_cam.pose = cam.get_transform();
        gizmo->update(persp_cam, { static_cast<float>(width), static_cast<float>(height) });
    }
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
    const float4x4 viewProjectionMatrix = (projectionMatrix * viewMatrix);

    // Render the offscreen nvg surface
    {
        std::string text = "Polymer Engine";

        NVGcontext * nvg = surface->pre_draw(window, 0);
        const float2 size = surface->surface_size();

        nvgSave(nvg);

        nvgBeginPath(nvg);
        nvgRect(nvg, 0, 0, size.x, size.y);
        nvgFillColor(nvg, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
        nvgFill(nvg);

        surface->draw_text_quick(text, 120, float2(size.x / 2, size.y / 2), nvgRGBAf(1, 1, 1, 1));

        nvgRestore(nvg);

        surface->post_draw();
    }

    // Reset state changed by nanovg
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    const float4x4 boxModel = (make_translation_matrix({ 0, 6, -10 }) * make_scaling_matrix(float3(8.f, 4.f, 0.1f)));

    // Render the offscreen nvg surface in the world as a small quad to the left
    {
        const float4x4 nvgSurfaceModel = (make_translation_matrix({ -4, 2, 0 }) * make_rotation_matrix({ 0, 1, 0 }, (float)POLYMER_PI / 2.f));
        nvg_surface_shader.bind();
        nvg_surface_shader.uniform("u_mvp", (viewProjectionMatrix * nvgSurfaceModel));
        nvg_surface_shader.texture("s_texture", 0, surface->surface_texture(0), GL_TEXTURE_2D);
        quad_mesh.draw_elements();
        nvg_surface_shader.unbind();
    }

    // The gizmo controls the location and orientation of the projected texture
    tinygizmo::transform_gizmo("projector-gizmo", gizmo->gizmo_ctx, gizmo_selection);
    const transform gizmo_pose = to_linalg(gizmo_selection);

    // Now render a large billboard in the scene, projected with the cookie texture.
    {
        const uint32_t cookie_tex = (which_cookie == 0) ? surface->surface_texture(0) : cookie;

        auto & shader = projector.get_shader();

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0, -1.0);

        const float4x4 projectorModelViewMatrix = (inverse(gizmo_pose.matrix()) * boxModel);
        const float4x4 projectorMatrix = projector.get_projector_matrix(projectorModelViewMatrix, false);

        shader.bind();
        shader.uniform("u_viewProj", viewProjectionMatrix);
        shader.uniform("u_projectorMatrix", projectorMatrix);
        shader.uniform("u_modelMatrix", boxModel);
        shader.uniform("u_modelMatrixIT", inverse(transpose(boxModel)));
        shader.texture("s_cookieTex", 0, cookie_tex, GL_TEXTURE_2D);
        box_mesh.draw_elements();
        shader.unbind();

        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Draw the floor grid
    grid.draw(viewProjectionMatrix);

    imgui->begin_frame();

    // Add some widgets to ImGui
    gui::imgui_fixed_window_begin("sample-debug-ui", { { 0, 0 },{ 320, height } });
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Projector Position {%.3f, %.3f, %.3f}", gizmo_pose.position.x, gizmo_pose.position.y, gizmo_pose.position.z);
    if (ImGui::Button("Reset Gizmo"))
    {
        gizmo_selection = {};
    }
    ImGui::SliderInt("Texture", &which_cookie, 0, 1);

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
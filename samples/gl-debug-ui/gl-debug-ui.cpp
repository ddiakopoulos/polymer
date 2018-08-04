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

struct interaction_state
{
    float3 origin{ 0, 0, 0 };

    float zoom{ 0.f };

    float d_theta{ 0.f };
    float d_phi{ 0.f };

    float pan_x{ 0.f };
    float pan_y{ 0.f };
    float pan_z{ 0.f };

    float yaw{ 0.f };
    float pitch{ 0.f };
};

struct frame
{
    float3 zDir;
    float3 xDir;
    float3 yDir;
};

inline frame make_frame(float3 eyePoint, float3 target, float3 worldUp = { 0,1,0 })
{
    frame f;
    f.zDir = normalize(eyePoint - target);
    f.xDir = normalize(cross(worldUp, f.zDir));
    f.yDir = cross(f.zDir, f.xDir);
    return f;
}

struct orbit_camera
{
    float yfov{ 1.f }, near_clip{ 0.01f }, far_clip{ 128.f };

    float3 eye{ 0, 10, 10 };
    float3 target{ 0, 0.01f, 0.01f };

    interaction_state current_state;
    interaction_state last_state;

    bool ml = 0, mr = 0;
    float2 last_cursor{ 0.f, 0.f };
    float2 cursor_location{ 0.f, 0.f }; // mouseX, mouseY
    float focus = 1.f;

    frame f;

    // orbit camera controller
    // min, max for all values
    // set target
    // inertia
    // auto rotate around target
    // theta, phi rename
    // speed factors

    orbit_camera()
    {
        float3 dir = normalize(eye - target);
        current_state.pitch = asinf(dir.y);
        current_state.yaw = acosf(dir.x); // Note the - signs

        std::cout << "Initial Direction: " << dir << std::endl;

        std::cout << "Setting Initial Yaw: " << current_state.yaw << std::endl;
        std::cout << "Setting Initial Pitch: " << current_state.pitch << std::endl;
        const float3 direction_vec = normalize(vector_from_yaw_pitch(current_state.yaw, -current_state.pitch));

        f = make_frame(eye, target);
    }

    float3 vector_from_yaw_pitch(float yaw, float pitch)
    {
        return { cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw) };
    }

    void handle_input(const app_input_event & e)
    {
        if (e.type == app_input_event::Type::SCROLL)
        {
            current_state.zoom = (-e.value[1]) * 0.5; // deltas
            cursor_location = e.cursor;
            eye += (f.zDir * current_state.zoom);
        }
        else if (e.type == app_input_event::MOUSE)
        {
            if (e.value[0] == GLFW_MOUSE_BUTTON_LEFT) ml = e.is_down();
            else if (e.value[0] == GLFW_MOUSE_BUTTON_RIGHT) mr = e.is_down();
        }
        else if (e.type == app_input_event::CURSOR)
        {
            const float2 delta_cursor = e.cursor - last_cursor;
            if (mr)
            {
                if (e.mods & GLFW_MOD_SHIFT)
                {
                    float scaleFactor = (10.f * std::tan(yfov * 0.5f) * 2.0f) * 0.005;
                    current_state.pan_x += delta_cursor.x * scaleFactor;
                    current_state.pan_y += delta_cursor.y * scaleFactor;

                    const float3 flat_z = normalize(f.zDir * float3(1, 0, 1));
                    const float3 flat_x = normalize(f.xDir * float3(1, 0, 1));

                    eye += (flat_x * (delta_cursor.x * scaleFactor)) + (flat_z * (delta_cursor.y * scaleFactor));
                }
                else
                {
                    // Delta with scale
                    auto delta_x = delta_cursor.x * 0.005f;
                    auto delta_y = delta_cursor.y * 0.005f;

                    // Accumulate
                    current_state.yaw += delta_x;
                    current_state.pitch += delta_y;

                    // Clamp
                    current_state.pitch = clamp(current_state.pitch, float(-POLYMER_PI / 2.f) + 0.01f, float(POLYMER_PI / 2.f) - 0.01f);
                    current_state.yaw = fmodf(current_state.yaw, float(POLYMER_TAU));

                    // the lookat vector
                    // right = normalize(cross(lookat, up));
                    const float3 direction_vec = normalize(vector_from_yaw_pitch(current_state.yaw, current_state.pitch));
                    auto new_target = (direction_vec);
                    std::cout << "New Decomposed Vec: " << direction_vec << std::endl;
                    //std::cout << "new target: " << new_target << std::endl;
                    //target = new_target;

                    f = make_frame(new_target, target);
                }
            }

            last_cursor = e.cursor;
        }
    }

    float4x4 get_view_matrix()
    {
        transform view;
        view.position = eye;
        view.orientation = normalize(make_rotation_quat_from_rotation_matrix({ f.xDir, f.yDir, f.zDir }));

        return view.view_matrix();
    }

    float4x4 get_projection_matrix(const float aspect) { return linalg::perspective_matrix(yfov, aspect, near_clip, far_clip); }
    float4x4 get_viewproj_matrix(const float aspect) { return mul(get_projection_matrix(aspect), get_view_matrix()); }

    void update(const float timestep)
    {
        last_state = current_state;
    }
};

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
            return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), modelViewMatrix);
        }
        return mul(make_projection_matrix(to_radians(45.f), 1.0f, 0.1f, 16.f), modelViewMatrix);
    }

    // Transforms a position into projective texture space.
    // This matrix combines the light view, projection and bias matrices.
    const float4x4 get_projector_matrix(const float4x4 & modelViewMatrix, bool isOrthographic = false)
    {
        // Bias matrix is a constant.
        // It performs a linear transformation to go from the [–1, 1]
        // range to the [0, 1] range. Having the coordinates in the [0, 1]
        // range is necessary for the values to be used as texture coordinates.
        constexpr float4x4 biasMatrix = {
            { 0.5f,  0.0f,  0.0f,  0.0f },
            { 0.0f,  0.5f,  0.0f,  0.0f },
            { 0.0f,  0.0f,  0.5f,  0.0f },
            { 0.5f,  0.5f,  0.5f,  1.0f }
        };

        return mul(biasMatrix, get_view_projection_matrix(modelViewMatrix, isOrthographic));
    }
};

struct sample_gl_debug_ui final : public polymer_app
{
    orbit_camera cam;

    //perspective_camera cam;
    //fps_camera_controller flycam;
    gl_renderable_grid grid{ 1.f, 24, 24 };
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

    //cam.look_at({ 0, 5, 5 }, { 0, 0.1f, -0.1f });
    ///flycam.set_camera(&cam);
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
    //gizmo->update(cam, { static_cast<float>(width), static_cast<float>(height) });
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

    const float4x4 boxModel = mul(make_translation_matrix({ 0, 6, -10 }), make_scaling_matrix(float3(8.f, 4.f, 0.1f)));

    // Render the offscreen nvg surface in the world as a small quad to the left
    {
        const float4x4 nvgSurfaceModel = mul(make_translation_matrix({ -4, 2, 0 }), make_rotation_matrix({ 0, 1, 0 }, (float)POLYMER_PI / 2.f));
        nvg_surface_shader.bind();
        nvg_surface_shader.uniform("u_mvp", mul(viewProjectionMatrix, nvgSurfaceModel));
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

        const float4x4 projectorModelViewMatrix = mul(inverse(gizmo_pose.matrix()), boxModel);
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
    //gizmo->draw();

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
/*
 * File: samples/gl-camera-trajectory.cpp
 * This sample demonstrates how to move a camera along a spline trajectory
 * using parallel transport frames. The input is given by four bezier control
 * points, interactively editable using gizmos. The top-left contains a preview 
 * of the camera along the spline. Left/right arrows keys can be used to step 
 * along discrete frames. 
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "gl-camera.hpp"
#include "gl-renderable-grid.hpp"
#include "gl-gizmo.hpp"
#include "gl-mesh-util.hpp"
#include "gl-procedural-mesh.hpp"
#include "gl-texture-view.hpp"

using namespace polymer;

constexpr const char basic_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 2) in vec3 inColor;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char basic_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(color + u_color, 1);
    }
)";

constexpr const char skybox_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    uniform mat4 u_viewProj;
    uniform mat4 u_modelMatrix;
    out vec3 v_normal;
    out vec3 v_world;
    void main()
    {
        vec4 worldPosition = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPosition;
        v_world = worldPosition.xyz;
        v_normal = normal;
    }
)";

constexpr const char skybox_frag[] = R"(#version 330
    in vec3 v_normal, v_world;
    out vec4 f_color;
    uniform vec3 u_bottomColor;
    uniform vec3 u_topColor;
    void main()
    {
        float h = normalize(v_world).y;
        f_color = vec4(mix(u_bottomColor, u_topColor, max(pow(max(h, 0.0), 0.8), 0.0)), 1.0);
    }
)";

class transport_frames
{
    std::vector<float4x4> frames;

public:

    void recompute(std::array<transform, 4> & controlPoints, uint32_t segments = 128)
    {
        frames = make_parallel_transport_frame_bezier(controlPoints, segments);
    }

    void reset() { frames.clear(); }

    std::vector<float4x4> & get() { return frames; }

    float4x4 get_transform(const size_t idx) const
    {
        assert(frames.size() > 0);
        if (idx >= 0) return frames[idx];
        else return frames[frames.size()];
    }

    std::vector<gl_mesh> debug_mesh() const
    {
        assert(frames.size() > 0);
        std::vector<gl_mesh> axisMeshes;

        for (int i = 0; i < frames.size(); ++i)
        {
            const float4x4 m = get_transform(i);
            const float3 xdir = normalize(mul(m, float4(1, 0, 0, 0)).xyz());
            const float3 ydir = normalize(mul(m, float4(0, 1, 0, 0)).xyz());
            const float3 zdir = normalize(mul(m, float4(0, 0, 1, 0)).xyz());
            axisMeshes.push_back(make_axis_mesh(xdir, ydir, zdir));
        }
        return axisMeshes;
    }
};

struct sample_gl_camera_trajectory final : public polymer_app
{
    perspective_camera debug_cam;
    perspective_camera follow_cam;

    fps_camera_controller fly_controller;

    gl_renderable_grid grid{ 1.f, 32, 32};
    transport_frames frames;

    std::unique_ptr<gl_gizmo> gizmo;
    tinygizmo::rigid_transform gizmo_ctrl_point[4];
    std::array<transform, 4> control_points;

    gl_mesh axis_mesh;
    gl_mesh sphere_mesh;
    gl_shader basic_shader;
    gl_shader sky_shader;

    uint32_t playback_index = 0;

    gl_texture_2d renderTextureRGBA;
    gl_texture_2d renderTextureDepth;
    gl_framebuffer renderFramebuffer;

    std::unique_ptr<gl_texture_view_2d> view;

    sample_gl_camera_trajectory();
    ~sample_gl_camera_trajectory();

    void render_scene(const GLuint framebuffer, const perspective_camera cam);
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

    view.reset(new gl_texture_view_2d(true));

    renderTextureRGBA.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    renderTextureDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTextureRGBA, 0);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderTextureDepth, 0);
    renderFramebuffer.check_complete();

    gizmo.reset(new gl_gizmo());

    sphere_mesh = make_sphere_mesh(1.f);
    axis_mesh = make_mesh_from_geometry(make_axis());
    basic_shader = gl_shader(basic_vert, basic_frag);
    sky_shader = gl_shader(skybox_vert, skybox_frag);

    control_points[0].position = { -3, 2, 0 };
    control_points[1].position = { -1, 4, 0 };
    control_points[2].position = { +1, 2, 0 };
    control_points[3].position = { +3, 4, 0 };

    gizmo_ctrl_point[0] = from_linalg(control_points[0]);
    gizmo_ctrl_point[1] = from_linalg(control_points[1]);
    gizmo_ctrl_point[2] = from_linalg(control_points[2]);
    gizmo_ctrl_point[3] = from_linalg(control_points[3]);

    frames.recompute(control_points, 32);

    debug_cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    fly_controller.set_camera(&debug_cam);
}

sample_gl_camera_trajectory::~sample_gl_camera_trajectory() {}

void sample_gl_camera_trajectory::on_window_resize(int2 size) {}

void sample_gl_camera_trajectory::on_input(const app_input_event & event)
{
    fly_controller.handle_input(event);
    gizmo->handle_input(event);

    if (event.type == app_input_event::KEY && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_KEY_LEFT) playback_index--;
        if (event.value[0] == GLFW_KEY_RIGHT) playback_index++;
    }
}

void sample_gl_camera_trajectory::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    fly_controller.update(e.timestep_ms);
    gizmo->update(debug_cam, float2(width, height));

    const auto frameMatrix = frames.get_transform(playback_index);
    follow_cam.pose.position = frameMatrix[3].xyz();
    follow_cam.pose.orientation = make_rotation_quat_from_pose_matrix(frameMatrix);
    playback_index = playback_index % (frames.get().size() - 1);
}

void sample_gl_camera_trajectory::render_scene(const GLuint framebuffer, const perspective_camera cam)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glViewport(0, 0, width, height);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    // Draw the sky
    glDisable(GL_DEPTH_TEST);
    {
        // Largest non-clipped sphere
        const float4x4 world = mul(make_translation_matrix(cam.get_eye_point()), scaling_matrix(float3(cam.farclip * .99f)));

        sky_shader.bind();
        sky_shader.uniform("u_viewProj", viewProjectionMatrix);
        sky_shader.uniform("u_modelMatrix", world);
        sky_shader.uniform("u_bottomColor", float3(52.0f / 255.f, 62.0f / 255.f, 82.0f / 255.f));
        sky_shader.uniform("u_topColor", float3(81.0f / 255.f, 101.0f / 255.f, 142.0f / 255.f));
        sphere_mesh.draw_elements();
        sky_shader.unbind();
    }

    glEnable(GL_DEPTH_TEST);

    // Draw the floor
    grid.draw(viewProjectionMatrix);
}

void sample_gl_camera_trajectory::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render into framebuffer
    render_scene(renderFramebuffer, follow_cam);

    // Render onto default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < 4; ++i)
    {
        if (tinygizmo::transform_gizmo("ctrl-gizmo-" + std::to_string(i), gizmo->gizmo_ctx, gizmo_ctrl_point[i]))
        {
            control_points[0] = to_linalg(gizmo_ctrl_point[0]);
            control_points[1] = to_linalg(gizmo_ctrl_point[1]);
            control_points[2] = to_linalg(gizmo_ctrl_point[2]);
            control_points[3] = to_linalg(gizmo_ctrl_point[3]);
            frames.recompute(control_points, 32);
        }
    }

    {
        const float4x4 viewProjectionMatrix = mul(debug_cam.get_projection_matrix(float(width) / float(height)), debug_cam.get_view_matrix());

        basic_shader.bind();

        if (frames.get().size() > 0)
        {
            auto meshes = frames.debug_mesh();
            for (int i = 0; i < frames.get().size(); ++i)
            {
                const auto frameMatrix = frames.get_transform(i);
                basic_shader.uniform("u_mvp", mul(viewProjectionMatrix, make_translation_matrix(frameMatrix[3].xyz())));
                meshes[i].draw_elements(); // axes drawn directly from matrix
            }
        }

        basic_shader.unbind();
    }

    gizmo->draw();

    const aabb_2d view_rect = { {10, 10}, {320, 180} };
    view->draw(view_rect, float2(width, height), renderTextureRGBA);

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

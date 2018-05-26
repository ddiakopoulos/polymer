/*
 * File: samples/
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "gl-camera.hpp"
#include "gl-renderable-grid.hpp"
#include "gl-gizmo.hpp"
#include "gl-mesh-util.hpp"
#include "gl-procedural-mesh.hpp"

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
        f_color = vec4(color, 1);
    }
)";

class trajectory_follower
{
    std::vector<float4x4> frames;

public:

    void compute(std::array<transform, 4> & controlPoints, uint32_t segments = 128)
    {
        frames = make_parallel_transport_frame_bezier(controlPoints, segments);
    }

    void reset() { frames.clear(); }

    std::vector<float4x4> & get_frames() { return frames; }

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
            auto m = get_transform(i);

            const float3 qxdir = normalize(mul(m, float4(1, 0, 0, 0)).xyz());
            const float3 qydir = normalize(mul(m, float4(0, 1, 0, 0)).xyz());
            const float3 qzdir = normalize(mul(m, float4(0, 0, 1, 0)).xyz());

            axisMeshes.push_back(make_axis_mesh(qxdir, qydir, qzdir));
        }
        return axisMeshes;
    }
};

struct sample_gl_camera_trajectory final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    gl_renderable_grid grid{ 1.f, 24, 24 };
    trajectory_follower follower;

    std::unique_ptr<gl_gizmo> gizmo;
    tinygizmo::rigid_transform gizmo_ctrl_point[4];
    std::array<transform, 4> control_points;

    gl_mesh axis_mesh;
    gl_shader basic_shader;

    sample_gl_camera_trajectory();
    ~sample_gl_camera_trajectory();

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

    gizmo.reset(new gl_gizmo());

    axis_mesh = make_mesh_from_geometry(make_axis());

    basic_shader = gl_shader(basic_vert, basic_frag);

    control_points[0].position = { -3, 2, 0 };
    control_points[1].position = { -1, 4, 0 };
    control_points[2].position = { +1, 2, 0 };
    control_points[3].position = { +3, 4, 0 };

    gizmo_ctrl_point[0] = from_linalg(control_points[0]);
    gizmo_ctrl_point[1] = from_linalg(control_points[1]);
    gizmo_ctrl_point[2] = from_linalg(control_points[2]);
    gizmo_ctrl_point[3] = from_linalg(control_points[3]);

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_gl_camera_trajectory::~sample_gl_camera_trajectory() {}

void sample_gl_camera_trajectory::on_window_resize(int2 size) {}

void sample_gl_camera_trajectory::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
    gizmo->handle_input(event);

    if (event.type == app_input_event::KEY && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_KEY_SPACE)
        {

        }
    }

}

void sample_gl_camera_trajectory::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    gizmo->update(cam, float2(width, height));
}

void sample_gl_camera_trajectory::on_draw()
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

    grid.draw(viewProjectionMatrix);

    for (int i = 0; i < 4; ++i)
    {
        if (tinygizmo::transform_gizmo("ctrl-gizmo-" + std::to_string(i), gizmo->gizmo_ctx, gizmo_ctrl_point[i]))
        {
            control_points[0] = to_linalg(gizmo_ctrl_point[0]);
            control_points[1] = to_linalg(gizmo_ctrl_point[1]);
            control_points[2] = to_linalg(gizmo_ctrl_point[2]);
            control_points[3] = to_linalg(gizmo_ctrl_point[3]);
            follower.compute(control_points, 32);
        }
    }

    {
        basic_shader.bind();

        if (follower.get_frames().size() > 0)
        {
            auto meshes = follower.debug_mesh();
            for (int i = 0; i < follower.get_frames().size(); ++i)
            {
                auto modelMatrix = follower.get_transform(i);
                basic_shader.uniform("u_mvp", mul(viewProjectionMatrix, make_translation_matrix(modelMatrix[3].xyz())));
                meshes[i].draw_elements(); // axes drawn directly from matrix
            }
        }

        basic_shader.unbind();
    }

    gizmo->draw();

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

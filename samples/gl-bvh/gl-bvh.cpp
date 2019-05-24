#include "lib-polymer.hpp"
#include "gl-camera.hpp"
#include "gl-texture-view.hpp"
#include "gl-gizmo.hpp"
#include "bvh.hpp"

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
        f_color = vec4(u_color, 1);
    }
)";

struct debug_sphere
{
    transform p;
    float radius;
    aabb_3d get_bounds() const
    {
        const float3 rad3 = float3(radius, radius, radius);
        return { p.transform_coord(-rad3), p.transform_coord(rad3) };
    }
};

struct sample_gl_bvh final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;
    uniform_random_gen gen;

    bool show_debug = false;

    std::unique_ptr<gl_shader> shader;
    std::vector<debug_sphere> spheres;
    gl_mesh sphereMesh;
    gl_mesh boxMesh;

    octree<debug_sphere> octree{ 8,{ { -24, -24, -24 },{ +24, +24, +24 } } };
    std::vector<node_container<debug_sphere>> nodes;

    std::unique_ptr<gl_gizmo> gizmo;
    tinygizmo::rigid_transform xform;

    sample_gl_bvh();
    ~sample_gl_bvh() {}

    void on_window_resize(int2 size) override {}
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_bvh::sample_gl_bvh() : polymer_app(1280, 720, "sample-gl-bvh")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    gizmo.reset(new gl_gizmo());
    xform.position = { 0.1f, 0.1f, 0.1f };

    shader.reset(new gl_shader(basic_vert, basic_frag));

    sphereMesh = make_sphere_mesh(1.f);
    boxMesh = make_cube_mesh();
    boxMesh.set_non_indexed(GL_LINES);

    for (int i = 0; i < 512; ++i)
    {
        const float3 position = { gen.random_float(48.f) - 24.f, gen.random_float(48.f) - 24.f, gen.random_float(48.f) - 24.f };
        const float radius = gen.random_float(0.225f);

        debug_sphere s;
        s.p = transform(quatf(linalg::identity), position);
        s.radius = radius;

        spheres.push_back(std::move(s));
    }
}

void sample_gl_bvh::on_input(const app_input_event & event)
{
    flycam.handle_input(event);

    if (gizmo) gizmo->handle_input(event);

    if (event.type == app_input_event::KEY && event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
    {
        show_debug = !show_debug;
    }
}

void sample_gl_bvh::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
}

void sample_gl_bvh::on_draw()
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

    if (gizmo) gizmo->update(cam, { static_cast<float>(width), static_cast<float>(height) });
    tinygizmo::transform_gizmo("bvh-gizmo", gizmo->gizmo_ctx, xform);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = (projectionMatrix * viewMatrix);

    shader->bind();
    shader->unbind();

    if (gizmo) gizmo->draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_bvh app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

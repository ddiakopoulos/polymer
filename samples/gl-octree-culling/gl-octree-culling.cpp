/*
 * File: samples/gl-octree-culling.cpp
 */

#include "index.hpp"
#include "gl-camera.hpp"
#include "gl-texture-view.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

using namespace polymer;

constexpr const char simple_colored_vert[] = R"(#version 330
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

constexpr const char simple_colored_frag[] = R"(#version 330
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

struct sample_gl_octree_culling final : public polymer_app
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

    sample_gl_octree_culling();
    ~sample_gl_octree_culling();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_octree_culling::sample_gl_octree_culling() : polymer_app(1280, 720, "sample-gl-octree-culling")
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

    shader.reset(new gl_shader(simple_colored_vert, simple_colored_frag));

    sphereMesh = make_sphere_mesh(1.f);
    boxMesh = make_cube_mesh();
    boxMesh.set_non_indexed(GL_LINES);

    for (int i = 0; i < 2048; ++i)
    {
        const float3 position = { gen.random_float(48.f) - 24.f, gen.random_float(48.f) - 24.f, gen.random_float(48.f) - 24.f };
        const float radius = gen.random_float(0.125f);

        debug_sphere s;
        s.p = transform(float4(0, 0, 0, 1), position);
        s.radius = radius;

        spheres.push_back(std::move(s));
    }

    {
        scoped_timer create("create octree");
        for (auto & s : spheres)
        {
            node_container<debug_sphere> container = { s, s.get_bounds() };
            octree.create(container);
            nodes.push_back(std::move(container));
        }
    }
}

sample_gl_octree_culling::~sample_gl_octree_culling()
{ 

}

void sample_gl_octree_culling::on_window_resize(int2 size)
{ 

}

void sample_gl_octree_culling::on_input(const app_input_event & event)
{
    flycam.handle_input(event);

    if (gizmo) gizmo->handle_input(event);

    if (event.type == app_input_event::KEY && event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
    {
        show_debug = !show_debug;
    }
}

void sample_gl_octree_culling::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
}

void sample_gl_octree_culling::on_draw()
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

    if (gizmo) gizmo->update(cam, float2(width, height));
    tinygizmo::transform_gizmo("octree-gizmo", gizmo->gizmo_ctx, xform);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    if (show_debug)
    {
        octree_debug_draw<debug_sphere>(octree, shader.get(), &boxMesh, &sphereMesh, viewProjectionMatrix, nullptr, float3());
    }

    {
        float3 xformPosition = { xform.position.x, xform.position.y, xform.position.z };
        auto & n = nodes[0];
        n.object.p.position = xformPosition;
        n.worldspaceBounds = n.object.get_bounds();
        octree.update(n);
    }

    const frustum cullingFrustum(viewProjectionMatrix);

    shader->bind();

    /*
    for (auto & s : spheres)
    {
        const auto sphereModel = mul(s.p.matrix(), make_scaling_matrix(s.radius));
        shader->uniform("u_color", cullingFrustum.contains(s.p.position) ? float3(1, 0, 0) : float3(0, 0, 0));
        shader->uniform("u_mvp", mul(viewProjectionMatrix, sphereModel));
        sphereMesh.draw_elements();
    }
    */

    std::vector<octant<debug_sphere> *> visibleNodes;
    {
        //scoped_timer t("octree cull");
        octree.cull(cullingFrustum, visibleNodes, nullptr, false);
    }

    uint32_t visibleObjects = 0;

    for (octant<debug_sphere> * node : visibleNodes)
    {
        // Draw bounding in white around this node
        const float4x4 boxModelMatrix = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
        shader->uniform("u_color", float3(1, 1, 1));
        shader->uniform("u_mvp", mul(viewProjectionMatrix, boxModelMatrix));
        boxMesh.draw_elements();

        // Draw the contents of the node as red spheres
        for (auto obj : node->objects)
        {
            const auto & object = obj.object;
            const float4x4 sphereModelMatrix = mul(object.p.matrix(), make_scaling_matrix(object.radius));
            shader->uniform("u_color", float3(1, 0, 0));
            shader->uniform("u_mvp", mul(viewProjectionMatrix, sphereModelMatrix));
            sphereMesh.draw_elements();
        }

        visibleObjects += node->objects.size();
    }

    shader->unbind();

    if (gizmo) gizmo->draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_octree_culling app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
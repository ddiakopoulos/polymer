#include "lib-polymer.hpp"

#include "camera-controllers.hpp"

#include "gl-texture-view.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"

#include <sstream>

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


using namespace gui;

struct sample_gl_obb final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;
    uniform_random_gen gen;

    std::unique_ptr<imgui_instance> imgui;

    std::unique_ptr<gl_shader> debug_shader;
    gl_mesh sphereMesh;
    gl_mesh boxMesh;

    std::vector<float3> points;

    sample_gl_obb();
    ~sample_gl_obb() {}

    oriented_bounding_box the_obb;
    void regen_pointcloud();

    void on_window_resize(int2 size) override {}
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_obb::sample_gl_obb() : polymer_app(1280, 720, "sample-gl-obb", 4)
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    imgui.reset(new gui::imgui_instance(window, true));
    gui::make_light_theme();

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    cam.farclip = 256;
    flycam.set_camera(&cam);

    debug_shader.reset(new gl_shader(basic_vert, basic_frag));

    sphereMesh = make_sphere_mesh(1.f);

    boxMesh = make_cube_mesh();
    boxMesh.set_non_indexed(GL_LINES);

    regen_pointcloud();
}

void sample_gl_obb::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
    imgui->update_input(event);

    if (event.type == app_input_event::KEY && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_KEY_SPACE)
        {
            regen_pointcloud();
        }
    }
}

void sample_gl_obb::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
}

void sample_gl_obb::regen_pointcloud()
{
    points.clear();

    uniform_random_gen rand;
    for (int i = 0; i < 256; i++)
    {
        float dist_multiplier_x = rand.random_float(1.f, 5.f);
        float dist_multiplier_y = rand.random_float(1.f, 5.f);
        float dist_multiplier_z = rand.random_float(1.f, 5.f);

        float3 random_position = { 
                   rand.random_float(-1, 1) * dist_multiplier_x,
                   rand.random_float(-1, 1) * dist_multiplier_y,
                   rand.random_float(-1, 1) * dist_multiplier_z };

        points.push_back(random_position);
    }

    the_obb = make_oriented_bounding_box(points);

}
void sample_gl_obb::on_draw()
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

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = (projectionMatrix * viewMatrix);

    imgui->begin_frame();

    debug_shader->bind();
    for (auto & p : points)
    {
        const auto point_model = make_translation_matrix(p) * make_scaling_matrix(0.05f);
        debug_shader->uniform("u_mvp", viewProjectionMatrix * point_model);
        debug_shader->uniform("u_color", float3(1, 1, 1));
        sphereMesh.draw_elements();
    }

    {
        const float4x4 obb_model = make_translation_matrix(the_obb.center) * make_rotation_matrix(the_obb.orientation) * make_scaling_matrix(the_obb.half_ext * 2.f);
        ImGui::Text("Position %f %f %f", the_obb.center.x(), the_obb.center.y(), the_obb.center.z());
        ImGui::Text("Scale %f %f %f", the_obb.half_ext.x(), the_obb.half_ext.y(), the_obb.half_ext.z());
        ImGui::Text("Orientation %f %f %f %f", the_obb.orientation.x, the_obb.orientation.y, the_obb.orientation.z, the_obb.orientation.w);
        debug_shader->uniform("u_mvp", viewProjectionMatrix * obb_model);
        debug_shader->uniform("u_color", float3(1, 0, 1));
        boxMesh.draw_elements();
    }

    debug_shader->unbind();

    imgui->end_frame();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_obb app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#include "polymer-core/lib-polymer.hpp"

#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-app-base/wrappers/gl-gizmo.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"

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

struct debug_object
{
    transform p;
    float radius;
    aabb_3d get_bounds() const
    {
        const float3 rad3 = float3(radius, radius, radius);
        return { p.transform_coord(-rad3), p.transform_coord(rad3) };
    }
};

using namespace gui;

struct sample_gl_bvh final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;
    uniform_random_gen gen;

    bool show_debug = true;

    std::unique_ptr<imgui_instance> imgui;

    std::vector<bvh_node_data> bvh_objects;
    std::unique_ptr<gl_shader> debug_shader;
    std::vector<debug_object> scene_objects;
    gl_mesh sphereMesh;
    gl_mesh boxMesh;

    bvh_tree scene_accelerator;
    bvh_node * selected_node{ nullptr };
    bvh_node_data * selected_object{ nullptr };

    uint32_t bvhInsertIdx = 0;

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

    imgui.reset(new gui::imgui_instance(window, true));
    gui::make_light_theme();

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    gizmo.reset(new gl_gizmo());
    xform.position = { 0.1f, 0.1f, 0.1f };

    debug_shader.reset(new gl_shader(basic_vert, basic_frag));

    sphereMesh = make_sphere_mesh(1.f);
    boxMesh = make_cube_mesh();
    boxMesh.set_non_indexed(GL_LINES);

    auto spiral = make_spiral(16.f, 2.f);
    for (auto & v : spiral.vertices)
    {
        debug_object s1;
        s1.radius = 0.075f;
        s1.p.position = v * 10;
        scene_objects.push_back(std::move(s1));
    }

    /*
    debug_object s1;
    s1.radius = 0.5f;
    s1.p.position = { 2, -2, 2 };
    scene_objects.push_back(std::move(s1));
    
    debug_object s2;
    s2.radius = 0.5f;
    s2.p.position = { -2, -1, 2 };
    scene_objects.push_back(std::move(s2));
    
    debug_object s3;
    s3.radius = 0.5f;
    s3.p.position = { 2, 0, -2 };
    scene_objects.push_back(std::move(s3));
    
    debug_object s4;
    s4.radius = 0.5f;
    s4.p.position = { -2, 1, -2 };
    scene_objects.push_back(std::move(s4));
    
    debug_object s5;
    s5.radius = 0.5f;
    s5.p.position = { -0, 4, -0 };
    scene_objects.push_back(std::move(s5));
    */

    for (int i = 0; i < scene_objects.size(); ++i)
    {
        auto & sphere = scene_objects[i];

        bvh_node_data obj;
        obj.bounds = sphere.get_bounds();
        obj.user_data = &sphere;
        bvh_objects.emplace_back(std::move(obj));
    }

    for (int i = 0; i < bvh_objects.size(); ++i)
    {
        scene_accelerator.add(&bvh_objects[i]);
    }
    
    scene_accelerator.build();

    //std::stringstream output;
    //scene_accelerator.debug_print_tree(output);
    //std::cout << output.str() << std::endl;
}

uint32_t node_index{ 0 };
void sample_gl_bvh::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
    imgui->update_input(event);
    if (gizmo) gizmo->handle_input(event);

    if (event.type == app_input_event::KEY && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_KEY_UP)    node_index++;
        if (event.value[0] == GLFW_KEY_DOWN)  node_index--;
        if (event.value[0] == GLFW_KEY_SPACE) show_debug = !show_debug;
        if (event.value[0] == GLFW_KEY_1)
        {
            scene_accelerator.add(&bvh_objects[bvhInsertIdx++]);
            scene_accelerator.refit();

            std::stringstream output;
            scene_accelerator.debug_print_tree(output);
            std::cout << output.str() << std::endl;
        }
    }

    if (event.type == app_input_event::MOUSE && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            const ray r = cam.get_world_ray(event.cursor, float2(static_cast<float>(width), static_cast<float>(height)));

            if (length(r.direction) > 0)
            {
                std::vector<std::pair<bvh_node_data*, float>> hit_results;
                if (scene_accelerator.intersect(r, hit_results)) selected_object = hit_results[0].first;
                else selected_object = nullptr;
            }
        }
    }
}

void sample_gl_bvh::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
}

uint64_t frame_count = 0;

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

    std::vector<bvh_node *> all_nodes;
    scene_accelerator.get_flat_node_list(all_nodes, nullptr);
    
    std::vector<bvh_node *> selected_subtree;
    if (all_nodes.size())
    {
        selected_node = all_nodes[node_index];
        scene_accelerator.get_flat_node_list(selected_subtree, selected_node);
    }

    debug_shader->bind();

    for (int i = 0; i < selected_subtree.size(); ++i)
    {
        bvh_node * node = selected_subtree[i];
        if (node->type == bvh_node_type::leaf)
        {
            if (node->object)
            {
                auto the_sphere = static_cast<debug_object*>(node->object->user_data);

                if (the_sphere)
                {
                    const auto leaf_model = make_translation_matrix(node->bounds.center()) * make_scaling_matrix(node->bounds.size());

                    if (selected_object == node->object)
                    {
                        debug_shader->uniform("u_color", float3(1, 0, 1));
                    }
                    else
                    {
                        debug_shader->uniform("u_color", float3(1, 0, 0));
                    }
                   
                    debug_shader->uniform("u_mvp", viewProjectionMatrix * leaf_model);
                    boxMesh.draw_elements();
                }
            }
        }
        else
        {
            float3 eps = { gen.random_float(0.01f), gen.random_float(0.01f), gen.random_float(0.01f) };
            const auto internal_node_model = make_translation_matrix(node->bounds.center()) * make_scaling_matrix(node->bounds.size());

            debug_shader->uniform("u_mvp", viewProjectionMatrix * internal_node_model);

            if (node->type == bvh_node_type::root)
            {
                debug_shader->uniform("u_color", float3(1, 1, 1));
                boxMesh.draw_elements();
            }
            else if (node->type == bvh_node_type::internal)
            {
                debug_shader->uniform("u_color", float3(1, 1, 0));
                boxMesh.draw_elements();
            }

        }
    }

    debug_shader->unbind();

    if (gizmo) gizmo->draw();

    imgui->begin_frame();
    imgui->end_frame();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 

    frame_count++;
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

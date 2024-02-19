/*
 * File: samples/engine-performance.cpp
 */

#include "polymer-core/lib-polymer.hpp"
#include "polymer-engine/lib-engine.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"

using namespace polymer;
using namespace gui;

constexpr const char basic_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    out vec4 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char basic_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec4 u_color;
    void main()
    {
        f_color = vec4(u_color);
    }
)";


struct sample_engine_performance final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;

    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_system_manager> the_entity_system_manager;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    bool show_debug_view{ false };
    std::unique_ptr<gl_shader> boxDebugShader;
    gl_mesh boxDebugMesh;

    std::vector<entity> new_entities;
    render_payload payload;
    scene scene;

    sample_engine_performance();
    ~sample_engine_performance();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_performance::sample_engine_performance() : polymer_app(1920, 1080, "sample-engine-performance", 4)
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    imgui.reset(new gui::imgui_instance(window, true));

    shaderMonitor.reset(new gl_shader_monitor("../../assets/"));
    fullscreen_surface.reset(new simple_texture_view());
    the_entity_system_manager.reset(new entity_system_manager());

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    scene.reset(*the_entity_system_manager, {width, height}, true);

    boxDebugShader.reset(new gl_shader(basic_vert, basic_frag));
    boxDebugMesh = make_cube_mesh();
    boxDebugMesh.set_non_indexed(GL_LINES);

    std::vector<std::string> geometry_options
    {
        "tetrahedron-uniform",
        "cube-uniform",
        "capsule-uniform",
        "cylinder-hollow-twosides",
        "sphere-uniform",
        "cone-uniform",
        "torus-knot",
        "pyramid",
        "hexagon-uniform",
        "cube-rounded",
    };

    uniform_random_gen rand;

    for (uint32_t entity_index = 0; entity_index < 16384; ++entity_index) // 16384
    {
        float dist_multiplier = 128.f;
        const float3 random_position = float3(rand.random_float(-1, 1) * dist_multiplier, rand.random_float(-1, 1) * dist_multiplier, rand.random_float(-1, 1) * dist_multiplier);
        const float3 random_axis = normalize(float3(rand.random_float(), rand.random_float(), rand.random_float()));
        const quatf random_quat = make_rotation_quat_axis_angle(random_axis, rand.random_float_sphere());
        const transform pose = transform(normalize(random_quat), random_position);
        const float3 scale = float3(rand.random_float(0.1f, 2.5f));
        const std::string name = "pickable-" + std::to_string(entity_index);

        auto geometry = geometry_options[rand.random_int(0, (int32_t) geometry_options.size() - 1)];
        const entity e = make_standard_scene_object(the_entity_system_manager.get(), &scene,
            name, pose, scale, material_handle(material_library::kDefaultMaterialId), geometry, geometry);

        new_entities.push_back(e);
    }

    payload.clear_color = float4(0.85f, 0.85f, 0.85f, 1.f);

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    cam.farclip = 128.f;

    scene.resolver->add_search_path("../../assets/");
    scene.resolver->resolve();
}

sample_engine_performance::~sample_engine_performance() {}

void sample_engine_performance::on_window_resize(int2 size) {}

void sample_engine_performance::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
    imgui->update_input(event);

    if (event.type == app_input_event::MOUSE && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            const ray r = cam.get_world_ray(event.cursor, float2(static_cast<float>(event.windowSize.x), static_cast<float>(event.windowSize.y)));

            if (length(r.direction) > 0)
            {
                entity_hit_result result = scene.collision_system->raycast(r);
                if (result.r.hit)
                {
                    auto mat_comp = scene.render_system->get_material_component(result.e);
                    mat_comp->material = "renderer-wireframe";
                }
            }
        }
    }
}

void sample_engine_performance::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
}

void sample_engine_performance::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    imgui->begin_frame();

    const uint32_t viewIndex = 0;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    const frustum camera_frustum(viewProjectionMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    payload.views.clear();
    payload.views.emplace_back(view_data(viewIndex, cam.pose, projectionMatrix));

    payload.render_components.clear();

    {
        simple_cpu_timer t;
        t.start();
        const std::vector<entity> visible_entity_list = scene.collision_system->get_visible_entities(camera_frustum);
        t.stop();

        ImGui::Text("Frustum Cull Took %f ms", (float)t.elapsed_ms());
        ImGui::Text("Visible Entities %i", visible_entity_list.size());

        {
            for (size_t i = 0; i < visible_entity_list.size(); ++i)
            {
                const auto e = visible_entity_list[i];
                payload.render_components.emplace_back(assemble_render_component(scene, e));
            }
        }
    }

    scene.render_system->get_renderer()->render_frame(payload);

    if (show_debug_view)
    {
        std::vector<bvh_node *> selected_subtree;
        scene.collision_system->static_accelerator->get_flat_node_list(selected_subtree);

        boxDebugShader->bind();

        for (int i = 0; i < selected_subtree.size(); ++i)
        {
            bvh_node * node = selected_subtree[i];
            if (node->type == bvh_node_type::leaf)
            {
                if (node->object)
                {
                    const auto so = static_cast<scene_object*>(node->object->user_data);

                    if (so)
                    {
                        const auto leaf_model = make_translation_matrix(node->bounds.center()) * make_scaling_matrix(node->bounds.size());
                        boxDebugShader->uniform("u_color", float4(1, 0, 0, 1));
                        boxDebugShader->uniform("u_mvp", viewProjectionMatrix * leaf_model);
                        boxDebugMesh.draw_elements();
                    }
                }
            }
            else
            {
                const auto internal_node_model = make_translation_matrix(node->bounds.center()) * make_scaling_matrix(node->bounds.size());

                boxDebugShader->uniform("u_mvp", viewProjectionMatrix * internal_node_model);

                if (node->type == bvh_node_type::root)
                {
                    boxDebugShader->uniform("u_color", float4(1, 1, 1, 1));
                    boxDebugMesh.draw_elements();
                }
                else if (node->type == bvh_node_type::internal)
                {
                    boxDebugShader->uniform("u_color", float4(1, 1, 0, 0.025f));
                    boxDebugMesh.draw_elements();
                }

            }
        }

        boxDebugShader->unbind();
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fullscreen_surface->draw(scene.render_system->get_renderer()->get_color_texture(viewIndex));

    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Checkbox("Show Debug", &show_debug_view);
    for (auto & t : scene.render_system->get_renderer()->cpuProfiler.get_data()) ImGui::Text("CPU: %s - %f", t.first.c_str(), t.second);
    for (auto & t : scene.render_system->get_renderer()->gpuProfiler.get_data()) ImGui::Text("GPU: %s - %f", t.first.c_str(), t.second);
    imgui->end_frame();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_engine_performance app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

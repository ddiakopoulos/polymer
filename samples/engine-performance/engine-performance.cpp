/*
 * File: samples/engine-ecs-stress.cpp
 * Modified `engine-scene` sample
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "environment.hpp"
#include "renderer-util.hpp"
#include "gl-imgui.hpp"

using namespace polymer;
using namespace gui;

struct sample_engine_performance final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<imgui_instance> imgui;
    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    std::vector<entity> new_entities;
    render_payload payload;
    environment scene;

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
    orchestrator.reset(new entity_orchestrator());

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    scene.reset(*orchestrator, {width, height}, true);

    std::vector<std::string> geometry_options
    {
        "tetrahedron-uniform",
        "cube-uniform",
        "capsule-uniform",
        "cylinder-hollow-twosides",
        "dome",
        "sphere-uniform",
        "cone-uniform",
        "torus-knot",
        "pyramid",
        "hexagon-uniform",
        "cube-rounded",
    };

    uniform_random_gen rand;

    // Configuring an entity at runtime programmatically
    for (uint32_t entity_index = 0; entity_index < 16384; ++entity_index)
    {
        float dist_multiplier = 256.f;
        const float3 random_position = float3(rand.random_float() * dist_multiplier, rand.random_float() * dist_multiplier, rand.random_float() * dist_multiplier);
        const float3 random_axis = normalize(float3(rand.random_float(), rand.random_float(), rand.random_float()));
        const quatf random_quat = make_rotation_quat_axis_angle(random_axis, rand.random_float_sphere());
        const transform pose = transform(normalize(random_quat), random_position);
        const float3 scale = float3(rand.random_float(0.1f, 3.0f));
        const std::string name = "debug-icosahedron-" + std::to_string(entity_index);

        auto geometry = geometry_options[rand.random_int(0, (int32_t) geometry_options.size() - 1)];
        const entity e = make_standard_scene_object(orchestrator.get(), &scene,
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

    payload.views.clear();
    payload.views.emplace_back(view_data(viewIndex, cam.pose, projectionMatrix));

    payload.render_components.clear();

    {
        const std::vector<entity> visible_entity_list = scene.collision_system->get_visible_entities(camera_frustum);
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

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fullscreen_surface->draw(scene.render_system->get_renderer()->get_color_texture(viewIndex));

    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    for (auto & t : scene.render_system->get_renderer()->cpuProfiler.get_data())
    {
        ImGui::Text("CPU: %s - %f", t.first.c_str(), t.second);
    }

    for (auto & t : scene.render_system->get_renderer()->gpuProfiler.get_data())
    {
        ImGui::Text("GPU: %s - %f", t.first.c_str(), t.second);
    }

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

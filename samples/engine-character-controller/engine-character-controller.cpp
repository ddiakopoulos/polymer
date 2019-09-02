/*
 * File: samples/engine-character-controller.cpp
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "scene.hpp"
#include "renderer-util.hpp"

using namespace polymer;

struct sample_engine_character_controller final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;

    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_system_manager> the_entity_system_manager;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    scene the_scene;

    sample_engine_character_controller();
    ~sample_engine_character_controller();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_character_controller::sample_engine_character_controller() : polymer_app(1280, 720, "sample-engine-character-controller")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    shaderMonitor.reset(new gl_shader_monitor("../../assets/"));
    fullscreen_surface.reset(new simple_texture_view());
    the_entity_system_manager.reset(new entity_system_manager());

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    the_scene.reset(*the_entity_system_manager, {width, height}, true);

    create_handle_for_asset("capsule-avatar", make_mesh_from_geometry(make_capsule(24, 1, 2))); 
    create_handle_for_asset("capsule-avatar", make_capsule(24, 1, 2));

    const entity player = make_standard_scene_object(the_entity_system_manager.get(), &the_scene,
        "player", {}, float3(1.f), material_handle(material_library::kDefaultMaterialId), "capsule-avatar", "capsule-avatar");

    render_component debug_icosahedron_renderable = assemble_render_component(the_scene, player);
    payload.render_components.push_back(debug_icosahedron_renderable);

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    for (auto & e : the_scene.entity_list())
    {
        if (auto * cubemap = the_scene.render_system->get_cubemap_component(e)) payload.ibl_cubemap = cubemap;
    }

    for (auto & e : the_scene.entity_list())
    {
        if (auto * proc_skybox = the_scene.render_system->get_procedural_skybox_component(e))
        {
            payload.procedural_skybox = proc_skybox;
            if (auto sunlight = the_scene.render_system->get_directional_light_component(proc_skybox->sun_directional_light))
            {
                payload.sunlight = sunlight;
            }
        }
    }

    the_scene.resolver->add_search_path("../../assets/");
    the_scene.resolver->resolve();
}

sample_engine_character_controller::~sample_engine_character_controller() {}

void sample_engine_character_controller::on_window_resize(int2 size) {}

void sample_engine_character_controller::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_engine_character_controller::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
}

void sample_engine_character_controller::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    const uint32_t viewIndex = 0;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    payload.views.clear();
    payload.views.emplace_back(view_data(viewIndex, cam.pose, projectionMatrix));
    the_scene.render_system->get_renderer()->render_frame(payload);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.25f, 0.25f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fullscreen_surface->draw(the_scene.render_system->get_renderer()->get_color_texture(viewIndex));

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_engine_character_controller app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

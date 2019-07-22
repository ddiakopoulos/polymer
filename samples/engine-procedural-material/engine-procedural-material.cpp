/*
 * File: samples/engine-scene.cpp
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "scene.hpp"
#include "renderer-util.hpp"

using namespace polymer;

struct sample_engine_procedural_material final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<gl_shader_monitor> shader_monitor;
    std::unique_ptr<entity_system_manager> the_entity_system_manager;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    scene the_scene;

    sample_engine_procedural_material();
    ~sample_engine_procedural_material();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_procedural_material::sample_engine_procedural_material() : polymer_app(1280, 720, "sample-engine-procedural-material")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    shader_monitor.reset(new gl_shader_monitor("../../assets/")); // polymer root
    shader_monitor->add_search_path("assets/"); // local

    fullscreen_surface.reset(new simple_texture_view());
    the_entity_system_manager.reset(new entity_system_manager());

    load_required_renderer_assets("../../assets/", *shader_monitor);

    the_scene.reset(*the_entity_system_manager, {width, height}, true);

    auto gpu_mesh = create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(4)));
    auto cpu_mesh = create_handle_for_asset("debug-icosahedron", make_icosasphere(4));

    shader_monitor->watch("ikeda-shader",
        "../../assets/shaders/renderer/renderer_vert.glsl",
        "assets/ikeda_frag.glsl",
        "../../assets/shaders/renderer");

    {
        const entity debug_icosa = the_scene.track_entity(the_entity_system_manager->create_entity());

        the_scene.identifier_system->create(debug_icosa, "debug-icosahedron");
        the_scene.xform_system->create(debug_icosa, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::mesh_component mesh_component(debug_icosa);
        mesh_component.mesh = gpu_mesh;
        the_scene.render_system->create(debug_icosa, std::move(mesh_component));

        auto ikeda_material = std::make_shared<polymer_procedural_material>();
        ikeda_material->shader = shader_handle("ikeda-shader");

        ikeda_material->update_uniform_func = [this, ikeda_material]()
        {
            auto & shader = ikeda_material->compiled_shader->shader;
            shader.bind();
            shader.unbind();
        };
        auto ikeda_material_handle = the_scene.mat_library->register_material("ikeda-material", ikeda_material);

        // Create material component with a default (normal-mapped) material
        polymer::material_component material_component(debug_icosa);
        material_component.material = ikeda_material_handle;
        the_scene.render_system->create(debug_icosa, std::move(material_component));

        payload.render_components.push_back(assemble_render_component(the_scene, debug_icosa));
    }

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
    the_scene.resolver->add_search_path("assets/");
    the_scene.resolver->resolve();
}

sample_engine_procedural_material::~sample_engine_procedural_material() {}

void sample_engine_procedural_material::on_window_resize(int2 size) {}

void sample_engine_procedural_material::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_engine_procedural_material::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shader_monitor->handle_recompile();
}

void sample_engine_procedural_material::on_draw()
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
        sample_engine_procedural_material app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

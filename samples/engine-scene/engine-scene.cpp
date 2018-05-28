/*
 * File: samples/engine-scene.cpp
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "environment.hpp"
#include "renderer-util.hpp"

using namespace polymer;

struct sample_engine_scene final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<asset_resolver> resolver;
    std::unique_ptr<pbr_renderer> renderer;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    environment scene;

    sample_engine_scene();
    ~sample_engine_scene();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_scene::sample_engine_scene() : polymer_app(1280, 720, "sample-engine-scene")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    shaderMonitor.reset(new gl_shader_monitor("../../assets/"));
    fullscreen_surface.reset(new simple_texture_view());
    orchestrator.reset(new entity_orchestrator());

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    // Initial renderer settings
    renderer_settings settings;
    settings.renderSize = int2(width, height);
    scene.collision_system = orchestrator->create_system<collision_system>(orchestrator.get());
    scene.xform_system = orchestrator->create_system<transform_system>(orchestrator.get());
    scene.identifier_system = orchestrator->create_system<identifier_system>(orchestrator.get());
    scene.render_system = orchestrator->create_system<render_system>(settings, orchestrator.get());

    auto radianceBinary = read_file_binary("../../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    payload.ibl_radianceCubemap = create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    payload.ibl_irradianceCubemap = create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
    payload.skybox = scene.render_system->get_skybox();
    payload.sunlight = scene.render_system->get_implict_sunlight();

    scene.mat_library.reset(new polymer::material_library("../../assets/sample-material.json"));

    // Resolve asset_handles to resources on disk
    resolver.reset(new asset_resolver());
    resolver->resolve("../../assets/", &scene, scene.mat_library.get());

    // Creating an entity programmatically
    {
        create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(3))); // gpu mesh
        create_handle_for_asset("debug-icosahedron", make_icosasphere(3)); // cpu mesh

        // Create a debug entity
        const entity debug_icosa = scene.track_entity(orchestrator->create_entity());

        scene.identifier_system->create(debug_icosa, "debug-icosahedron");

        // Create mesh component for the gpu mesh
        polymer::mesh_component mesh_component(debug_icosa);
        mesh_component.mesh = gpu_mesh_handle("debug-icosahedron");
        scene.render_system->create(debug_icosa, std::move(mesh_component));

        polymer::material_component material_component(debug_icosa);
        material_component.material = material_handle(material_library::kDefaultMaterialId);
        scene.render_system->create(debug_icosa, std::move(material_component));

        scene.xform_system->create(debug_icosa, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::geometry_component geom_component(debug_icosa);
        geom_component.geom = cpu_mesh_handle("debug-icosahedron");
        scene.collision_system->create(debug_icosa, std::move(geom_component));

        renderable debug_icosahedron_renderable;
        debug_icosahedron_renderable.e = debug_icosa;
        debug_icosahedron_renderable.material = scene.render_system->get_material_component(debug_icosa);
        debug_icosahedron_renderable.mesh = scene.render_system->get_mesh_component(debug_icosa);
        debug_icosahedron_renderable.scale = scene.xform_system->get_local_transform(debug_icosa)->local_scale;
        debug_icosahedron_renderable.t = scene.xform_system->get_world_transform(debug_icosa)->world_pose;

        payload.render_set.push_back(debug_icosahedron_renderable);
    }

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_engine_scene::~sample_engine_scene() {}

void sample_engine_scene::on_window_resize(int2 size) {}

void sample_engine_scene::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_engine_scene::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
}

void sample_engine_scene::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    const uint32_t viewIndex = 0;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    payload.views.emplace_back(view_data(viewIndex, cam.pose, projectionMatrix));
    scene.render_system->get_renderer()->render_frame(payload);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.25f, 0.25f, 1.0f);

    fullscreen_surface->draw(scene.render_system->get_renderer()->get_color_texture(viewIndex));
    payload.views.clear();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_engine_scene app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

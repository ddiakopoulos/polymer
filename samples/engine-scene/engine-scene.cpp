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
    resolver.reset(new asset_resolver());

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    // Initial renderer settings
    renderer_settings settings;
    settings.renderSize = int2(width, height);

    // Setup the required systems
    scene.collision_system = orchestrator->create_system<collision_system>(orchestrator.get());
    scene.xform_system = orchestrator->create_system<transform_system>(orchestrator.get());
    scene.identifier_system = orchestrator->create_system<identifier_system>(orchestrator.get());
    scene.render_system = orchestrator->create_system<render_system>(settings, orchestrator.get());
    scene.mat_library.reset(new polymer::material_library("../../assets/sample-material.json"));
    scene.event_manager.reset(new polymer::event_manager_async());

    auto radianceBinary = read_file_binary("../../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    payload.ibl_radianceCubemap = create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    payload.ibl_irradianceCubemap = create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
    payload.procedural_skybox = scene.render_system->get_procedural_skybox();
    payload.sunlight = scene.render_system->get_implicit_sunlight();

    // Resolve asset_handles to resources on disk. In the case of this sample, the assets
    // are created programmatically so no asset resolution needs to be performed.
    resolver->resolve("../../assets/", &scene, scene.mat_library.get());

    create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(3))); // gpu mesh
    create_handle_for_asset("debug-icosahedron", make_icosasphere(3)); // cpu mesh

    // This describes how to configure a renderable entity programmatically, at runtime.
    {
        // Create a new entity to represent an icosahedron that we will render
        const entity debug_icosa = scene.track_entity(orchestrator->create_entity());

        // Give the icosa a name and default transform and scale
        scene.identifier_system->create(debug_icosa, "debug-icosahedron");
        scene.xform_system->create(debug_icosa, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        // Create mesh component for the gpu mesh.
        polymer::mesh_component mesh_component(debug_icosa);
        mesh_component.mesh = gpu_mesh_handle("debug-icosahedron");
        scene.render_system->create(debug_icosa, std::move(mesh_component));

        // Create a geometry for the cpu mesh. This type of mesh is used for raycasting
        // and collision, so not strictly required for this sample.
        polymer::geometry_component geom_component(debug_icosa);
        geom_component.geom = cpu_mesh_handle("debug-icosahedron");
        scene.collision_system->create(debug_icosa, std::move(geom_component));

        // Create material component with a default (normal-mapped) material
        polymer::material_component material_component(debug_icosa);
        material_component.material = material_handle(material_library::kDefaultMaterialId);
        scene.render_system->create(debug_icosa, std::move(material_component));
    
        // Assemble a render_component (gather components so the renderer does not have to interface
        // with many systems). Ordinarily this assembly is done per-frame in the update loop, but
        // this is a fully static scene.
        render_component debug_icosahedron_renderable = assemble_render_component(scene, debug_icosa);
        payload.render_components.push_back(debug_icosahedron_renderable);
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

    const uint32_t viewIndex = 0;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    payload.views.clear();
    payload.views.emplace_back(view_data(viewIndex, cam.pose, projectionMatrix));
    scene.render_system->get_renderer()->render_frame(payload);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.25f, 0.25f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fullscreen_surface->draw(scene.render_system->get_renderer()->get_color_texture(viewIndex));

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

/*
 * File: samples/engine-scene.cpp
 */

#include "index.hpp"
#include "engine.hpp"

using namespace polymer;

struct sample_engine_scene final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    gl_shader_monitor shaderMonitor{ "../assets/" };

    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<asset_resolver> resolver;
    std::unique_ptr<pbr_render_system> renderer;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    poly_scene scene;

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

    shaderMonitor.watch("wireframe",
        "../../assets/shaders/wireframe_vert.glsl",
        "../../assets/shaders/wireframe_frag.glsl",
        "../../assets/shaders/wireframe_geom.glsl",
        "../../assets/shaders/renderer");

    shaderMonitor.watch("blinn-phong",
        "../../assets/shaders/renderer/forward_lighting_vert.glsl",
        "../../assets/shaders/renderer/forward_lighting_blinn_phong_frag.glsl",
        "../../assets/shaders/renderer");

    shaderMonitor.watch("sky-hosek",
        "../../assets/shaders/sky_vert.glsl", 
        "../../assets/shaders/sky_hosek_frag.glsl");

    shaderMonitor.watch("depth-prepass",
        "../../assets/shaders/renderer/depth_prepass_vert.glsl",
        "../../assets/shaders/renderer/depth_prepass_frag.glsl",
        "../../assets/shaders/renderer");

    shaderMonitor.watch("post-tonemap",
        "../../assets/shaders/renderer/post_tonemap_vert.glsl",
        "../../assets/shaders/renderer/post_tonemap_frag.glsl");

    shaderMonitor.watch("cascaded-shadows",
        "../../assets/shaders/renderer/shadowcascade_vert.glsl",
        "../../assets/shaders/renderer/shadowcascade_frag.glsl",
        "../../assets/shaders/renderer/shadowcascade_geom.glsl",
        "../../assets/shaders/renderer");

    for (auto & shader : shader_handle::list())
    {
        std::cout << "Shader Handle: " << shader.name << std::endl;
    }

    fullscreen_surface.reset(new simple_texture_view());

    orchestrator.reset(new entity_orchestrator());

    // Initial renderer settings
    renderer_settings settings;
    settings.renderSize = int2(width, height);
    renderer.reset(new pbr_render_system(orchestrator.get(), settings));

    scene.render_system = orchestrator->create_system<pbr_render_system>(orchestrator.get(), settings);
    scene.collision_system = orchestrator->create_system<collision_system>(orchestrator.get());
    scene.xform_system = orchestrator->create_system<transform_system>(orchestrator.get());
    scene.name_system = orchestrator->create_system<name_system>(orchestrator.get());

    auto radianceBinary = read_file_binary("../../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    payload.ibl_radianceCubemap = create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    payload.ibl_irradianceCubemap = create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    // Create a sun entity
    entity sunlight = orchestrator->create_entity();

    // Setup the skybox; link internal parameters to a directional light entity owned by the
    // render system. 
    scene.skybox.reset(new gl_hosek_sky());
    scene.skybox->onParametersChanged = [&]
    {
        scene.render_system->directional_lights[sunlight].data.direction = scene.skybox->get_sun_direction();
        scene.render_system->directional_lights[sunlight].data.color = float3(1.f, 1.0f, 1.0f);
        scene.render_system->directional_lights[sunlight].data.amount = 1.0f;
    };

    // Set initial values on the skybox with the sunlight entity we just created
    scene.skybox->onParametersChanged();

    // Only need to set the skybox on the |render_payload| once (unless we clear the payload)
    payload.skybox = scene.skybox.get();

    scene.mat_library.reset(new polymer::material_library("../../assets/sample-material.json"));

    // Resolve asset_handles to resources on disk
    resolver.reset(new asset_resolver());
    resolver->resolve("../../assets/", &scene, scene.mat_library.get());

    // Debugging
    {
        // Create a debug entity
        const entity debug_sphere = orchestrator->create_entity();

        // Create assets and assign them to handles
        create_handle_for_asset("debug-sphere", make_sphere_mesh(1.f));
        create_handle_for_asset("debug-sphere", make_sphere(1.0));

        // Create mesh component for the gpu mesh
        polymer::mesh_component mesh_component(debug_sphere);
        mesh_component.mesh = gpu_mesh_handle("debug-sphere");
        scene.render_system->meshes[debug_sphere] = mesh_component;

        // Create material component
        polymer::material_component mat_component(debug_sphere);
        mat_component.material = material_handle("sample-material");
        scene.render_system->materials[debug_sphere] = mat_component;

        // Create geom component for the cpu geometry (for collision + raycasting)
        polymer::geometry_component geom_component(debug_sphere);
        geom_component.geom = cpu_mesh_handle("debug-sphere");
        scene.collision_system->meshes[debug_sphere] = geom_component;

        // Create transform component
        scene.xform_system->create(debug_sphere, Pose(), { 1.f, 1.f, 1.f });

        scene.name_system->set_name(debug_sphere, "debug object: sphere");

        payload.render_set.push_back(debug_sphere);
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
    shaderMonitor.handle_recompile();
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
    scene.render_system->render_frame(payload);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.25f, 0.25f, 1.0f);

    fullscreen_surface->draw(scene.render_system->get_color_texture(viewIndex));
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
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
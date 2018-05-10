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

    std::unique_ptr<asset_resolver> resolver;

    std::unique_ptr<renderer_standard> renderer;
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

    fullscreen_surface.reset(new simple_texture_view());

    // Initial renderer settings
    renderer_settings settings;
    settings.renderSize = int2(width, height);
    renderer.reset(new renderer_standard(settings));

    // Setup default objects/state on scene
    scene.skybox.reset(new gl_hosek_sky());
    scene.skybox->onParametersChanged = [&]
    {
        payload.sunlight.direction = scene.skybox->get_sun_direction();
        payload.sunlight.color = float3(1.f, 1.0f, 1.0f);
        payload.sunlight.amount = 1.0f;
    };
    scene.skybox->onParametersChanged(); // call for initial set

    // Set skybox on the `render_payload` only once
    payload.skybox = scene.skybox.get();

    scene.materialLib.reset(new polymer::material_library("../../assets/materials.json"));

    // Resolve asset_handles to resources on disk
    resolver.reset(new asset_resolver());
    resolver->resolve("../../assets/", &scene, scene.materialLib.get());

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

    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    const Pose cameraPose = cam.pose;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

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
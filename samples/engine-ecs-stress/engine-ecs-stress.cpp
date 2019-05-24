/*
 * File: samples/engine-ecs-stress.cpp
 * Modified `engine-scene` sample
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "environment.hpp"
#include "renderer-util.hpp"

using namespace polymer;

struct sample_engine_ecs final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_orchestrator> orchestrator;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    environment scene;

    sample_engine_ecs();
    ~sample_engine_ecs();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_ecs::sample_engine_ecs() : polymer_app(1280, 720, "sample-ecs-stress")
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

    scene.reset(*orchestrator, {width, height}, true);

    create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(1))); // gpu mesh
    create_handle_for_asset("debug-icosahedron", make_icosasphere(1)); // cpu mesh

    {   
        uniform_random_gen rand;
        scoped_timer create_timer("create 16384 entities");

        std::vector<entity> new_entities;

        // Configuring an entity at runtime programmatically
        for (uint32_t entity_index = 0; entity_index < 16384; ++entity_index)
        {
            // Create a new entity to represent an icosahedron that we will render
            const entity debug_icosa = scene.track_entity(orchestrator->create_entity());

            const float rnd1 = rand.random_float() * 100.f;
            const float rnd2 = rand.random_float() * 100.f;
            const float rnd3 = rand.random_float() * 100.f;
            const float rnd_scale = rand.random_float(0.1f, 0.5f);

            // Give the icosa a name and default transform and scale
            scene.identifier_system->create(debug_icosa, "debug-icosahedron-" + std::to_string(entity_index));
            scene.xform_system->create(debug_icosa, transform(float3(rnd1, rnd2, rnd3)), float3(rnd_scale));

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

            new_entities.push_back(debug_icosa);
        }

        // Second pass to assemble render components separately, since `assemble_render_component` will
        // grab pointers to components that were probably shuffled around as we inserted
        // a bunch of them into the underlying component pool in transform_system.
        for (uint32_t i = 0; i < new_entities.size(); ++i)
        {
            const auto e = new_entities[i];

            // Assemble a render_component (gather components so the renderer does not have to interface
            // with many systems). Ordinarily this assembly is done per-frame in the update loop, but
            // this is a fully static scene.
            payload.render_components.emplace_back(assemble_render_component(scene, e));
        }
    }

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    scene.resolver->resolve("../../assets/");
}

sample_engine_ecs::~sample_engine_ecs() {}

void sample_engine_ecs::on_window_resize(int2 size) {}

void sample_engine_ecs::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_engine_ecs::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
}

void sample_engine_ecs::on_draw()
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

    // Optional debug output
    for (auto & t : scene.render_system->get_renderer()->cpuProfiler.get_data())
    {
        std::cout << "[render_system CPU] " << t.first.c_str() << " - " << t.second << "ms" << std::endl;
    }

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_engine_ecs app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

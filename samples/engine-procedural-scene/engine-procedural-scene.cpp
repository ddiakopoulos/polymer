/*
 * File: samples/engine-scene.cpp
 */

#include "lib-polymer.hpp"
#include "lib-engine.hpp"

#include "ecs/core-ecs.hpp"
#include "scene.hpp"
#include "renderer-util.hpp"

using namespace polymer;

struct sample_engine_procedural_scene final : public polymer_app
{
    perspective_camera cam;
    fps_camera_controller flycam;

    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<entity_system_manager> the_entity_system_manager;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    scene scene;

    sample_engine_procedural_scene();
    ~sample_engine_procedural_scene();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_engine_procedural_scene::sample_engine_procedural_scene() : polymer_app(1280, 720, "sample-engine-procedural-scene")
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

    scene.reset(*the_entity_system_manager, {width, height}, true);

    create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(3))); // gpu mesh
    create_handle_for_asset("debug-icosahedron", make_icosasphere(3)); // cpu mesh

    // This describes how to configure a renderable entity programmatically, at runtime.
    {
        // Create a new entity to represent an icosahedron that we will render
        const entity debug_icosa = scene.track_entity(the_entity_system_manager->create_entity());

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

    // Add a default skybox
    if (auto proc_skybox = scene.render_system->get_procedural_skybox_component())
    {
        payload.procedural_skybox = proc_skybox;
        if (auto sunlight = scene.render_system->get_directional_light_component(proc_skybox->sun_directional_light))
        {
            payload.sunlight = sunlight;
        }
    }

    // Add a default cubemap
    if (auto ibl_cubemap = scene.render_system->get_cubemap_component())
    {
        payload.ibl_cubemap = ibl_cubemap;
    }

    scene.resolver->add_search_path("../../assets/");
    scene.resolver->resolve();
}

sample_engine_procedural_scene::~sample_engine_procedural_scene() {}

void sample_engine_procedural_scene::on_window_resize(int2 size) {}

void sample_engine_procedural_scene::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_engine_procedural_scene::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
}

void sample_engine_procedural_scene::on_draw()
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
        sample_engine_procedural_scene app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

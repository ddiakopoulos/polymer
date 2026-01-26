/*
 * File: samples/engine-procedural-scene.cpp
 */

#include "polymer-core/lib-polymer.hpp"
#include "polymer-engine/lib-engine.hpp"
#include "polymer-engine/scene.hpp"

#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"

using namespace polymer;

struct sample_engine_procedural_scene final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;

    std::unique_ptr<gl_shader_monitor> shaderMonitor;
    std::unique_ptr<simple_texture_view> fullscreen_surface;

    render_payload payload;
    scene the_scene;

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

    load_required_renderer_assets("../../assets/", *shaderMonitor);

    the_scene.reset({width, height}, true);

    create_handle_for_asset("debug-icosahedron", make_mesh_from_geometry(make_icosasphere(3))); // gpu mesh
    create_handle_for_asset("debug-icosahedron", make_icosasphere(3)); // cpu mesh

    // Create a renderable mesh entity using the new factory method (1 line instead of 15+!)
    // Auto-registers with collision system automatically.
    base_object & icosa = the_scene.instantiate_mesh(
        "debug-icosahedron",           // name
        transform(float3(0, 0, 0)),    // pose
        float3(1.f, 1.f, 1.f),         // scale
        "debug-icosahedron"            // mesh/geometry handle
    );

    // Assemble render_component for the static scene
    auto assemble_render_component = [](base_object & obj) {
        render_component r;
        r.material = obj.get_component<material_component>();
        r.mesh = obj.get_component<mesh_component>();
        r.world_matrix = obj.get_component<transform_component>()->get_world_transform().matrix();
        r.render_sort_order = 0;
        return r;
    };
    payload.render_components.push_back(assemble_render_component(icosa));

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    // Find IBL cubemap and procedural skybox from scene graph
    for (auto & [e, obj] : the_scene.get_graph().graph_objects)
    {
        if (auto * cubemap = obj.get_component<ibl_component>())
        {
            payload.ibl_cubemap = cubemap;
        }

        if (auto * proc_skybox = obj.get_component<procedural_skybox_component>())
        {
            payload.procedural_skybox = proc_skybox;

            // Find the sun directional light
            if (proc_skybox->sun_directional_light != kInvalidEntity)
            {
                base_object & sun_obj = the_scene.get_graph().get_object(proc_skybox->sun_directional_light);
                if (auto * sunlight = sun_obj.get_component<directional_light_component>())
                {
                    payload.sunlight = sunlight;
                }
            }
        }
    }

    the_scene.resolver->add_search_path("../../assets/");
    the_scene.resolver->resolve();
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

    // Update all objects (calls on_update on each enabled object)
    the_scene.update(e.timestep_ms / 1000.0f);
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
    the_scene.get_renderer()->render_frame(payload);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.25f, 0.25f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fullscreen_surface->draw(the_scene.get_renderer()->get_color_texture(viewIndex));

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

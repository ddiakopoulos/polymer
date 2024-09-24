/*
 * File: samples/gl-particles-hull.cpp
 * This sample shows a work-in-progress CPU-based particle system with emitters
 * and modifiers. Particles are rendered in screen-space using instanced billboards. 
 * A point emitter is used as input to Polymer's quick_hull algorithm. The resulting 
 * convex hull is rendered using a wireframe geometry shader. Furthermore,
 * the convex hull computation is offloaded to a secondary thread through the
 * use of std::future and std::async. 
 */

 #include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-gfx-gl/gl-renderable-grid.hpp"
#include "polymer-gfx-gl/gl-particle-system.hpp"
#include "polymer-gfx-gl/gl-procedural-mesh.hpp"
#include "polymer-gfx-gl/gl-mesh-util.hpp"

#include "polymer-app-base/wrappers/gl-nvg.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-app-base/wrappers/gl-gizmo.hpp"
#include "polymer-app-base/camera-controllers.hpp"

#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/shader-library.hpp"

#include "polymer-model-io/model-io.hpp"

#include <future>

using namespace polymer;

constexpr const char skybox_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    uniform mat4 u_viewProj;
    uniform mat4 u_modelMatrix;
    out vec3 v_normal;
    out vec3 v_world;
    void main()
    {
        vec4 worldPosition = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPosition;
        v_world = worldPosition.xyz;
        v_normal = normal;
    }
)";

constexpr const char skybox_frag[] = R"(#version 330
    in vec3 v_normal, v_world;
    out vec4 f_color;
    uniform vec3 u_bottomColor;
    uniform vec3 u_topColor;
    void main()
    {
        float h = normalize(v_world).y;
        f_color = vec4(mix(u_bottomColor, u_topColor, max(pow(max(h, 0.0), 0.8), 0.0)), 1.0);
    }
)";


struct sample_gl_particle_hull final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;
    app_update_event last_update;
    gl_renderable_grid grid{ 0.5f, 16, 16 };

    std::unique_ptr<gl_shader_monitor> shaderMonitor;

    gl_particle_system particle_system;
    point_emitter pt_emitter;
    std::shared_ptr<gravity_modifier> grav_mod;
    std::shared_ptr<color_modifier> color_mod;
    std::shared_ptr<ground_modifier> ground_mod;

    gl_mesh convex_hull_mesh;
    geometry convex_hull_model;
    std::future<quickhull::convex_hull> hullFuture;

    gl_mesh sphere_mesh;
    gl_shader basic_shader;
    gl_shader sky_shader;

    bool pause{ false };
    bool draw_hull {true};
    uint64_t frame_count = 0;

    sample_gl_particle_hull();
    ~sample_gl_particle_hull();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_particle_hull::sample_gl_particle_hull() : polymer_app(1280, 720, "sample-gl-particle-hull", 4)
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    sphere_mesh = make_sphere_mesh(1.f);
    sky_shader = gl_shader(skybox_vert, skybox_frag);

    color_mod = std::make_shared<color_modifier>();
    grav_mod = std::make_shared<gravity_modifier>(float3(0, -1, 0));
    ground_mod = std::make_shared<ground_modifier>(plane(float4(0, 1, 0, 0)));

    particle_system.add_modifier(grav_mod);
    particle_system.add_modifier(color_mod);
    particle_system.add_modifier(ground_mod);

    pt_emitter.pose.position = float3(0, 2, 0);
    pt_emitter.multiplier = 2;

    gl_texture_2d particle_tex = load_image("../../assets/textures/particle_alt_large.png");
    glTextureParameteriEXT(particle_tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTextureParameteriEXT(particle_tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    particle_system.set_particle_texture(std::move(particle_tex));

    shaderMonitor.reset(new gl_shader_monitor("../../assets"));

    shaderMonitor->watch("particle-shader",
        "../../assets/shaders/renderer/particle_system_vert.glsl",
        "../../assets/shaders/renderer/particle_system_frag.glsl");

    shaderMonitor->watch("wireframe",
        "../../assets/shaders/wireframe_vert.glsl",
        "../../assets/shaders/wireframe_frag.glsl",
        "../../assets/shaders/wireframe_geom.glsl",
        "../../assets/shaders/renderer");

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_gl_particle_hull::~sample_gl_particle_hull() {}

void sample_gl_particle_hull::on_window_resize(int2 size) {}

void sample_gl_particle_hull::on_input(const app_input_event & event)
{
    flycam.handle_input(event);

    // Pause simulation
    if (event.type == app_input_event::KEY && event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
    {
        pause = !pause;
    }

    if (event.type == app_input_event::KEY && event.value[0] == GLFW_KEY_H && event.action == GLFW_RELEASE)
    {
        draw_hull = !draw_hull;
    }

    // Export particle pointcloud convex hull to disk as *.obj mesh
    else if (event.type == app_input_event::KEY && event.value[0] == GLFW_KEY_E && event.action == GLFW_RELEASE)
    {
        export_obj_model("convex_hull", "gl-particles-hull.obj", convex_hull_model);
    }
}

void sample_gl_particle_hull::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
    shaderMonitor->handle_recompile();
    last_update = e;

    if (!pause) 
    {
        pt_emitter.emit(particle_system);
    }
}

void sample_gl_particle_hull::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (!pause) 
    {
        particle_system.update(last_update.timestep_ms);
    }

    //color_mod->camera_position = cam.get_eye_point();

    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    // Draw the sky
    glDisable(GL_DEPTH_TEST);
    {
        const float4x4 world = (make_translation_matrix(cam.get_eye_point()) * matrix_xform::scaling(float3(cam.farclip * .99f)));
        sky_shader.bind();
        sky_shader.uniform("u_viewProj", viewProjectionMatrix);
        sky_shader.uniform("u_modelMatrix", world);
        sky_shader.uniform("u_bottomColor", float3(100.0f / 255.f, 100.0f / 255.f, 100.0f / 255.f));
        sky_shader.uniform("u_topColor", float3(81.0f / 255.f, 128.0f / 255.f, 160.0f / 255.f));
        sphere_mesh.draw_elements();
        sky_shader.unbind();
    }
    glEnable(GL_DEPTH_TEST);

    grid.draw(viewProjectionMatrix);

    // Draw the particle system
    shader_handle particle_shader_h("particle-shader");
    gl_shader & shader = particle_shader_h.get()->get_variant()->shader;
    particle_system.draw(viewMatrix, projectionMatrix, shader, false);

    if (hullFuture.valid())
    {
        auto status = hullFuture.wait_for(std::chrono::milliseconds(0));
        if (status != std::future_status::timeout)
        {
            auto the_hull = hullFuture.get();

            auto vertices = the_hull.getVertexBuffer();
            auto indices = the_hull.getIndexBuffer();
            std::vector<uint3> faces;
            for (int idx = 0; idx < indices.size(); idx += 3) faces.push_back({ (uint32_t)indices[idx + 0], (uint32_t)indices[idx + 1] ,(uint32_t)indices[idx + 2] });
    
            convex_hull_model.vertices = vertices;
            convex_hull_model.faces = faces;

            convex_hull_mesh.set_vertices(convex_hull_model.vertices, GL_STREAM_DRAW);
            convex_hull_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(float3), ((float*)0) + 0);
            convex_hull_mesh.set_elements(convex_hull_model.faces, GL_STREAM_DRAW);

            hullFuture = {};
        }
    }

    if (!hullFuture.valid())
    {
        auto particles = particle_system.get();

        hullFuture = std::async([=]() {
            scoped_timer t("compute convex hull");
            std::vector<float3> positions;
            for (const auto & p : particles) positions.push_back(p.position);
            quickhull::quick_hull convex_hull(positions);
            return convex_hull.compute(true, false, 0.0005f);
        });
    }

    if (draw_hull)
    {
        glEnable(GL_BLEND);
        shader_handle wireframe_shader_h("wireframe");
        gl_shader & wireframe_shader = wireframe_shader_h.get()->get_variant()->shader;
        wireframe_shader.bind();
        wireframe_shader.uniform("u_color", float4(0, 1, 1, 0.25f));
        wireframe_shader.uniform("u_eyePos", cam.get_eye_point());
        wireframe_shader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        wireframe_shader.uniform("u_modelMatrix", Identity4x4);
        convex_hull_mesh.draw_elements();
        wireframe_shader.unbind();
    }

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
    frame_count++;
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_particle_hull app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * File: samples/gl-render-offscreen.cpp
 * This sample demonstrates how to setup and render to a framebuffer. This framebuffer
 * is then drawn as a full-screen quad using the `simple_texture_view` utility class. 
 * The rendered meshes are all generated procedurally using Polymer's built-in mesh
 * classes. A user can then click a mesh to highlight it, showing how to peform a
 * simple raycast against CPU-resident geometry. 
 */

#include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-gfx-gl/gl-renderable-grid.hpp"

#include "polymer-app-base/wrappers/gl-nvg.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-app-base/wrappers/gl-gizmo.hpp"
#include "polymer-app-base/camera-controllers.hpp"

#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/shader-library.hpp"

using namespace polymer;

struct sample_object
{
    transform t;
    float3 scale;
    gl_mesh mesh;
    geometry geometry;
};

raycast_result raycast(const sample_object & obj, const ray & worldRay)
{
    ray localRay = obj.t.inverse() * worldRay;
    localRay.origin /= obj.scale;
    localRay.direction /= obj.scale;
    float outT = 0.0f;
    float3 outNormal = { 0, 0, 0 };
    float2 outUv{ -1, -1 };
    const bool hit = intersect_ray_mesh(localRay, obj.geometry, &outT, &outNormal, &outUv);
    return{ hit, outT, outNormal, outUv };
}

struct sample_gl_render_offscreen final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;

    std::unique_ptr<simple_texture_view> view;
    gl_shader_monitor shader_mon{ "../../assets/" };
    shader_handle wireframe_handle{ "wireframe" };
    gl_renderable_grid grid{ 0.5f, 24, 24 };

    std::vector<sample_object> objects;
    sample_object * selected_object{ nullptr };

    gl_texture_2d renderTextureRGBA;
    gl_texture_2d renderTextureDepth;
    gl_framebuffer renderFramebuffer;

    sample_gl_render_offscreen();
    ~sample_gl_render_offscreen() {}

    void on_window_resize(int2 size) override {}
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_render_offscreen::sample_gl_render_offscreen() : polymer_app(1280, 720, "sample-gl-render-offscreen")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    view.reset(new simple_texture_view());

    renderTextureRGBA.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    renderTextureDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTextureRGBA, 0);
    glNamedFramebufferTexture2DEXT(renderFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderTextureDepth, 0);
    renderFramebuffer.check_complete();

    // Assign a new shader_handle with id "wireframe"
    shader_mon.watch("wireframe",
        "../../assets/shaders/wireframe_vert.glsl",
        "../../assets/shaders/wireframe_frag.glsl",
        "../../assets/shaders/wireframe_geom.glsl",
        "../../assets/shaders/renderer");

    const geometry procedural_capsule = make_capsule(12, 0.5f, 2);
    const geometry procedural_cylinder = make_cylinder(0.5f, 0.5f, 2, 12, 24, false);
    const geometry procedural_sphere = make_sphere(1.f);
    const geometry procedural_cube = make_cube();
    const geometry procedural_torus = make_torus();
    const geometry procedural_icosahedron = make_icosahedron();
    const geometry procedural_tetrahedron = make_tetrahedron();

    sample_object capsule;
    capsule.t = transform({ -2, 1.5f, 0 });
    capsule.scale = float3(1, 1, 1);
    capsule.geometry = procedural_capsule;
    capsule.mesh = make_mesh_from_geometry(procedural_capsule);

    sample_object cylinder;
    cylinder.t = transform({ +2, 1.5f, 0 });
    cylinder.scale = float3(1, 1, 1);
    cylinder.geometry = procedural_cylinder;
    cylinder.mesh = make_mesh_from_geometry(procedural_cylinder);

    objects.emplace_back(std::move(capsule));
    objects.emplace_back(std::move(cylinder));

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

void sample_gl_render_offscreen::on_input(const app_input_event & event)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.handle_input(event);


    if (event.type == app_input_event::MOUSE && event.action == GLFW_RELEASE)
    {
        if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            float best_t = std::numeric_limits<float>::max();
            sample_object * hit{ nullptr };

            const ray r = cam.get_world_ray(event.cursor, float2(static_cast<float>(width), static_cast<float>(height)));

            if (length(r.direction) > 0)
            {
                for (auto & object : objects)
                {
                    raycast_result result = raycast(object, r);
                    if (result.hit)
                    {
                        if (result.distance < best_t)
                        {
                            best_t = result.distance;
                            hit = &object;
                        }
                    }
                }
                selected_object = hit;
            }
        }
    }
}

void sample_gl_render_offscreen::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    shader_mon.handle_recompile();
    flycam.update(e.timestep_ms);
}

void sample_gl_render_offscreen::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render to framebuffer
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
        const float4x4 viewMatrix = cam.get_view_matrix();
        const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

        auto default_wireframe_variant = wireframe_handle.get()->get_variant();
        auto & wireframe_prog = default_wireframe_variant->shader;

        wireframe_prog.bind();
        for (auto & object : objects)
        {
            const float4x4 modelMatrix = object.t.matrix() * make_scaling_matrix(object.scale);
            wireframe_prog.uniform("u_color", selected_object == &object ? float4(1, 0, 0, 0.5f) : float4(1, 1, 1, 0.5));
            wireframe_prog.uniform("u_eyePos", cam.get_eye_point());
            wireframe_prog.uniform("u_viewProjMatrix", viewProjectionMatrix);
            wireframe_prog.uniform("u_modelMatrix", modelMatrix);
            object.mesh.draw_elements();
        }
        wireframe_prog.unbind();

        grid.draw(viewProjectionMatrix);
    }

    // Render to default framebuffer (the screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    view->draw(renderTextureRGBA);

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_render_offscreen app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
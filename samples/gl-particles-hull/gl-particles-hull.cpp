/*
 * File: samples/gl-particles-hull.cpp
 */

#include "index.hpp"
#include "gl-loaders.hpp"
#include "gl-camera.hpp"
#include "gl-renderable-grid.hpp"
#include "gl-gizmo.hpp"
#include "gl-mesh-util.hpp"
#include "gl-procedural-mesh.hpp"
#include "gl-texture-view.hpp"
#include "gl-particle-system.hpp"

using namespace polymer;

constexpr const char textured_vert[] = R"(#version 330
    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inNormal;
    layout(location = 2) in vec2 inTexcoord;
    uniform mat4 u_mvp;
    out vec2 v_texcoord;
    void main()
    {
	    gl_Position = u_mvp * vec4(inPosition.xyz, 1);
        v_texcoord = inTexcoord;
    }
)";

constexpr const char textured_frag[] = R"(#version 330
    uniform sampler2D s_texture;
    in vec2 v_texcoord;
    out vec4 f_color;
    void main()
    {
        f_color = texture(s_texture, vec2(v_texcoord.x, v_texcoord.y));
    }
)";

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
    fps_camera_controller flycam;

    gl_mesh sphere_mesh;
    gl_shader texured_shader;
    gl_shader sky_shader;

    sample_gl_particle_hull();
    ~sample_gl_particle_hull();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_particle_hull::sample_gl_particle_hull() : polymer_app(1280, 720, "sample-gl-particle-hull")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    sphere_mesh = make_sphere_mesh(1.f);
    texured_shader = gl_shader(textured_vert, textured_frag);
    sky_shader = gl_shader(skybox_vert, skybox_frag);

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

sample_gl_particle_hull::~sample_gl_particle_hull() {}

void sample_gl_particle_hull::on_window_resize(int2 size) {}

void sample_gl_particle_hull::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_gl_particle_hull::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    flycam.update(e.timestep_ms);
}

void sample_gl_particle_hull::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

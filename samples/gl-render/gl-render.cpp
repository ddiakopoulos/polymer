/*
 * File: samples/gl-render.cpp
 * This sample demonstrates the use of Polymer's lowest-level OpenGL API. Polymer
 * uses sevel OpenGL 4.5 features under the hood, namely the direct state access (DSA)
 * extension to simplify the implementation of GL wrapper types. This sample uses 
 * `lib-model-io` to load an obj file and apply a rendering technique known as matcap 
 * shading, a texture-only based approach without any actual scene lighting.
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

#include "polymer-model-io/model-io.hpp"

using namespace polymer;

struct sample_gl_render final : public polymer_app
{
    perspective_camera cam;
    std::unique_ptr<arcball_controller> arcball;
    app_input_event lastEvent;
    bool deltaMotion{ false };

    transform modelPose;
    gl_mesh model;
    gl_shader matcapShader;
    gl_texture_2d matcapTexture;

    sample_gl_render();
    ~sample_gl_render();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

// Polymer also contains a method `make_mesh_from_geometry` which is a canonical library version of this
// function. That function handles all common vertex attribute types and assigns them to a layout we use
// throughout other samples/applications. This is used purely for reference/englightenment. 
void upload_mesh(const runtime_mesh & cpu, gl_mesh & gpu, bool indexed = true)
{
    const uint32_t components = 6;
    
    std::vector<float> buffer;
    buffer.reserve(cpu.vertices.size());

    for (int v = 0; v < cpu.vertices.size(); v++)
    {
        buffer.push_back(cpu.vertices[v].x);
        buffer.push_back(cpu.vertices[v].y);
        buffer.push_back(cpu.vertices[v].z);
        buffer.push_back(cpu.normals[v].x);
        buffer.push_back(cpu.normals[v].y);
        buffer.push_back(cpu.normals[v].z);
    }

    gpu.set_vertices(buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);
    gpu.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + 0);
    gpu.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + 3);
    if (indexed) gpu.set_elements(cpu.faces, GL_STATIC_DRAW);
    else gpu.set_non_indexed(GL_LINES);
}

void draw_mesh_matcap(gl_shader & shader, gl_mesh & mesh, const gl_texture_2d & tex, 
    const float4x4 & model, const float4x4 & view, const float4x4 & proj)
{
    shader.bind();
    shader.uniform("u_modelMatrix", model);
    shader.uniform("u_viewProj", proj * view);
    shader.uniform("u_modelViewMatrix", view * model);
    shader.uniform("u_modelMatrixIT", inverse(transpose(model)));
    shader.texture("u_matcapTexture", 0, tex, GL_TEXTURE_2D);
    mesh.draw_elements();
    shader.unbind();
}

sample_gl_render::sample_gl_render() : polymer_app(1280, 720, "sample-gl-render")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    matcapTexture = load_image("../../assets/textures/matcap/chemical_carpaint_blue.png");

    matcapShader = gl_shader(
        read_file_text("../../assets/shaders/prototype/matcap_vert.glsl"),
        read_file_text("../../assets/shaders/prototype/matcap_frag.glsl"));

    auto imported_mesh_table = import_model("../../assets/models/runtime/torus-knot.mesh");
    if (imported_mesh_table.size() == 0) throw std::runtime_error("model not found?");

    runtime_mesh & m = imported_mesh_table.begin()->second;
    rescale_geometry(m);
    upload_mesh(m, model);

    arcball.reset(new arcball_controller({ static_cast<float>(width), static_cast<float>(height) }));

    cam.look_at({ 0, 0, 2 }, { 0, 0.1f, 0 });
}

sample_gl_render::~sample_gl_render() {}

void sample_gl_render::on_window_resize(int2 size) {}

void sample_gl_render::on_input(const app_input_event & event)
{
    deltaMotion = (length(lastEvent.cursor - event.cursor) > 0);

    if (event.type == app_input_event::Type::MOUSE && event.is_down())
    {
        arcball->mouse_down(event.cursor);
    }

    if (event.type == app_input_event::Type::CURSOR && event.drag)
    {
        arcball->mouse_drag(event.cursor);
    }

    lastEvent = event;
}

void sample_gl_render::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
}

void sample_gl_render::on_draw()
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

    const transform cameraPose = cam.pose;
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    if (lastEvent.drag && deltaMotion)
    {
        modelPose.orientation = safe_normalize(modelPose.orientation * arcball->currentQuat);
    }

    draw_mesh_matcap(matcapShader, model, matcapTexture, modelPose.matrix(), viewMatrix, projectionMatrix);

    deltaMotion = false;

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_render app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#include "polymer-core/lib-polymer.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-gfx-gl/gl-renderable-meshline.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

using namespace polymer;

inline std::vector<float3> create_curve(uniform_random_gen & gen, float r_min = 3.f, float r_max = 12.f)
{
    std::vector<float3> curve;

    float3 p0 = float3(0, 0, 0);
    float3 p1 = p0 + float3(0.5f - gen.random_float(), 0.5f - gen.random_float(), 0.5f - gen.random_float());
    float3 p2 = p1 + float3(0.5f - gen.random_float(), 0.5f - gen.random_float(), 0.5f - gen.random_float());
    float3 p3 = p2 + float3(0.5f - gen.random_float(), 0.5f - gen.random_float(), 0.5f - gen.random_float());

    p0 *= r_min + gen.random_float() * r_max;
    p1 *= r_min + gen.random_float() * r_max;
    p2 *= r_min + gen.random_float() * r_max;
    p3 *= r_min + gen.random_float() * r_max;

    cubic_bezier spline(p0, p1, p2, p3, 128);

    constexpr size_t num_samples = 128;
    for (size_t i = 0; i < num_samples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(num_samples - 1);
        float arc_t = spline.get_length_parameter(t);
        float3 point = spline.evaluate(arc_t);
        curve.push_back(point);
        curve.push_back(point);
    }

    return curve;
}

struct sample_gl_meshline final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;
    uniform_random_gen gen;

    std::vector<float3> colors;
    std::vector<float> sizes;
    std::vector<std::shared_ptr<gl_meshline>> lines;

    float rotation_angle = 0.0f;

    sample_gl_meshline();
    ~sample_gl_meshline() {}

    void on_window_resize(int2 size) override {}
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_meshline::sample_gl_meshline() : polymer_app(1920, 1080, "sample-gl-meshline", 4)
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    cam.farclip = 128.f;
    cam.look_at({0, 8, 24}, {0, 0, 0});
    flycam.set_camera(&cam);

    auto asset_base = global_asset_dir::get()->get_asset_dir();

    colors.emplace_back(237.0f / 255.f, 106.0f / 255.f, 90.0f / 255.f);
    colors.emplace_back(244.0f / 255.f, 241.0f / 255.f, 187.0f / 255.f);
    colors.emplace_back(155.0f / 255.f, 193.0f / 255.f, 188.0f / 255.f);
    colors.emplace_back(92.0f / 255.f, 164.0f / 255.f, 169.0f / 255.f);
    colors.emplace_back(230.0f / 255.f, 235.0f / 255.f, 224.0f / 255.f);
    colors.emplace_back(240.0f / 255.f, 182.0f / 255.f, 127.0f / 255.f);
    colors.emplace_back(254.0f / 255.f, 95.0f / 255.f, 85.0f / 255.f);
    colors.emplace_back(214.0f / 255.f, 209.0f / 255.f, 177.0f / 255.f);
    colors.emplace_back(199.0f / 255.f, 239.0f / 255.f, 207.0f / 255.f);
    colors.emplace_back(255.0f / 255.f, 224.0f / 255.f, 102.0f / 255.f);
    colors.emplace_back(36.0f / 255.f, 123.0f / 255.f, 160.0f / 255.f);
    colors.emplace_back(112.0f / 255.f, 193.0f / 255.f, 179.0f / 255.f);
    colors.emplace_back(60.0f / 255.f, 60.0f / 255.f, 60.0f / 255.f);

    for (int i = 0; i < 256; ++i)
    {
        std::shared_ptr<gl_meshline> line = std::make_shared<gl_meshline>(asset_base);
        std::vector<float3> spline_points = create_curve(gen, 8.f, 48.f);
        line->set_vertices(spline_points);
        lines.push_back(line);
    }

    for (int i = 0; i < 256; ++i)
    {
        sizes.push_back(gen.random_float(1.0f, 16.0f));
    }

    gl_check_error(__FILE__, __LINE__);
}

void sample_gl_meshline::on_input(const app_input_event & event)
{
    flycam.handle_input(event);
}

void sample_gl_meshline::on_update(const app_update_event & e)
{
    flycam.update(e.timestep_ms);
    rotation_angle += 0.01f;
}

void sample_gl_meshline::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    float4x4 model = make_rotation_matrix({0, 1, 0}, 0.99f * rotation_angle);

    for (size_t l = 0; l < lines.size(); ++l)
    {
        std::shared_ptr<gl_meshline> & line = lines[l];
        float3 color = colors[l % colors.size()];
        float size = sizes[l];
        line->render(cam, model, float2(static_cast<float>(width), static_cast<float>(height)), color, size);
    }

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_meshline app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

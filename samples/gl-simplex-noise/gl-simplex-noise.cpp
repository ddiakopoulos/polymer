#include "index.hpp"
#include "gl-camera.hpp"
#include "gl-texture-view.hpp"

using namespace polymer;

struct sample_gl_simplex_noise final : public polymer_app
{
    universal_layout_container layout;

    std::vector<std::shared_ptr<GlTexture2D>> textures;
    std::vector<std::shared_ptr<GLTextureView>> views;

    const int texResolution = 512;
    std::vector<uint8_t> data;

    sample_gl_simplex_noise();
    ~sample_gl_simplex_noise();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_simplex_noise::sample_gl_simplex_noise() : polymer_app(1024, 1024, "sample-gl-simplex-noise")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    layout.bounds = { 0, 0, (float)width, (float)height };
    data.resize(texResolution * texResolution);

    float sz = 1.0f / 4.f;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            layout.add_child({ { j * sz, 0 },{ i * sz, 0 },{ (j + 1) * sz, 0 },{ (i + 1) * sz, 0 } }, std::make_shared<universal_layout_container>());
        }
    }

    layout.recompute();

    for (int i = 0; i < 16; ++i)
    {
        auto t = std::make_shared<GlTexture2D>();
        t->setup(texResolution, texResolution, GL_RED, GL_RED, GL_UNSIGNED_BYTE, nullptr);

        auto tv = std::make_shared<GLTextureView>(false);

        textures.push_back(t);
        views.push_back(tv);
    }
}

sample_gl_simplex_noise::~sample_gl_simplex_noise()
{

}

void sample_gl_simplex_noise::on_window_resize(int2 size)
{
    layout.bounds = { 0, 0, (float)size.x, (float)size.y };
    layout.recompute();
}

void sample_gl_simplex_noise::on_input(const app_input_event & event)
{

}

void sample_gl_simplex_noise::on_update(const app_update_event & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    const float time = e.elapsed_s;

    for (int i = 0; i < 16; ++i)
    {
        if (e.elapsedFrames > 1)
        {
            switch (i)
            {
            case 2: case 7: case 8: case 15: break;
            default: continue; break;
            }
        }

        for (int x = 0; x < texResolution; ++x)
        {
            for (int y = 0; y < texResolution; ++y)
            {
                float n = 0.0f;
                auto position = float2(x, y) * 0.01f;
                switch (i)
                {
                case 0:  n = noise::noise(position) * 0.5f + 0.5f; break;
                case 1:  n = noise::noise_ridged(position); break;
                case 2:  n = noise::noise_flow(position, time) * 0.5f + 0.5f; break;
                case 3:  n = noise::noise_fb(position) * 0.5f + 0.5f; break;
                case 4:  n = noise::noise_fb(position, 10, 5.0f, 0.75f) * 0.5f + 0.5f; break;
                case 5:  n = noise::noise_fb(noise::noise_fb(position * 3.0f)) * 0.5f + 0.5f; break;
                case 6:  n = noise::noise_fb(noise::noise_fb_deriv(position)) * 0.5f + 0.5f; break;
                case 7:  n = noise::noise_flow(position + noise::noise_fb(float3(position, time * 0.1f)), time) * 0.5f + 0.5f; break;
                case 8:  n = noise::noise_ridged_mf(float3(position, time * 0.1f), 1.0f, 5, 2.0f, 0.65f); break;
                case 9:  n = noise::noise_ridged_mf(position, 0.1f, 5, 1.5f, 1.5f); break;
                case 10: n = noise::noise_ridged_mf(noise::noise_ridged(position)); break;
                case 11: n = noise::noise_ridged_mf(position * 0.25f, -1.0f, 4, 3.0f, -0.65f); break;
                case 12: n = noise::noise_iq_fb(position, 5, float2x2({ 2.3f, -1.5f }, { 1.5f, 2.3f }), 0.5f) * 0.5f + 0.5f; break;
                case 13: n = noise::noise_iq_fb(position * 0.75f, 8, float2x2({ -12.5f, -0.5f }, { 0.5f, -12.5f }), 0.75f) * 0.5f + 0.5f; break;
                case 14: n = (noise::noise_deriv(position * 5.0f).y + noise::noise_deriv(position * 5.0f).z) * 0.5f; break;
                case 15: n = noise::noise(position + float2(noise::noise_curl(position, time).x)) * 0.5f + 0.5f; break;

                }
                data[y * texResolution + x] = clamp(n, 0.0f, 1.0f) * 255;
            }
        }

        textures[i]->setup(texResolution, texResolution, GL_RED, GL_RED, GL_UNSIGNED_BYTE, data.data());
    }

}

void sample_gl_simplex_noise::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    for (int i = 0; i < 16; i++)
    {
        views[i]->draw(layout.children[i]->bounds, float2(width, height), textures[i]->id());
    }

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_simplex_noise app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
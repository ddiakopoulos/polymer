/*
 * File: sample/gl-colormap.cpp
 */

#include "lib-polymer.hpp"
#include "gl-texture-view.hpp"
#include "stb/stb_easy_font.h"

using namespace polymer;


static const char s_fontVert[] = R"(#version 330
    layout(location = 0) in vec3 position;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
    }
)";

static const char s_fontFrag[] = R"(#version 330
    out vec4 f_color;
    void main()
    {
        f_color = vec4(1, 1, 1, 1.0); 
    }
)";

struct gl_font_view : public non_copyable
{
    gl_shader program;
    gl_mesh mesh;

    gl_font_view() 
    {
        program = gl_shader(s_fontVert, s_fontFrag);
    }

    void draw(const aabb_2d & rect, const float2 windowSize, const std::string & text)
    {
        GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean wasBlendingEnabled = glIsEnabled(GL_BLEND);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        struct Vertex { float3 position; };
        std::vector<Vertex> vertices;
        mesh.set_attribute(0, &Vertex::position);
        mesh.set_non_indexed(GL_TRIANGLES);

        char vertexBuffer[96000];
        int num_quads = stb_easy_font_print(0, 0, (char*) text.c_str(), nullptr, vertexBuffer, sizeof(vertexBuffer));

        float * data = reinterpret_cast<float*>(vertexBuffer);
        vertices.reserve(num_quads * 6);
        for (int quad = 0, stride = 0; quad < num_quads; ++quad, stride += 16) 
        {
            vertices.push_back( Vertex{float3(data[(0 * 4) + stride], data[(0 * 4) + 1 + stride], 1.f)});
            vertices.push_back( Vertex{float3(data[(1 * 4) + stride], data[(1 * 4) + 1 + stride], 1.f)});
            vertices.push_back( Vertex{float3(data[(2 * 4) + stride], data[(2 * 4) + 1 + stride], 1.f)});
            vertices.push_back( Vertex{float3(data[(2 * 4) + stride], data[(2 * 4) + 1 + stride], 1.f)});
            vertices.push_back( Vertex{float3(data[(3 * 4) + stride], data[(3 * 4) + 1 + stride], 1.f)});
            vertices.push_back( Vertex{float3(data[(0 * 4) + stride], data[(0 * 4) + 1 + stride], 1.f)});
        }

        mesh.set_vertices(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);

        const float4x4 projection = make_orthographic_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
        float4x4 model = make_scaling_matrix({ 1, 1, 0.f });
        model = (make_translation_matrix({ rect.min().x, rect.min().y, 0.f }) * model);

        program.bind();
        program.uniform("u_mvp", projection * model);
        mesh.draw_elements();
        program.unbind();

        if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
        if (wasCullingEnabled) glEnable(GL_CULL_FACE);
        if (wasBlendingEnabled) glEnable(GL_BLEND);
    }
};

struct colormap_view
{
    std::string name;
    std::shared_ptr<gl_texture_2d> texture;
};

struct sample_gl_colormap final : public polymer_app
{
    sample_gl_colormap();
    ~sample_gl_colormap();

    std::vector<colormap_view> generated_colormaps;
    gl_font_view font;
    gl_texture_view_2d view;

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_colormap::sample_gl_colormap() : polymer_app(1024, 1024, "sample-gl-colormap")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    const float num_steps = 2048.f;
    const float step_size = 1.f / num_steps;
    for (auto & map : colormap::colormap_table)
    {
        colormap_view view;
        std::vector<float3> colors_in_row;

        for (float c = 0; c < 1.f; c += step_size)
        {
            auto color = float3(colormap::get_color(c, map.first));
            colors_in_row.push_back(color);
        }

        std::shared_ptr<gl_texture_2d> t = std::make_shared< gl_texture_2d>();
        t->setup(num_steps, 1, GL_RGB, GL_RGB, GL_FLOAT, colors_in_row.data(), true);

        view.texture = t;
        view.name = map.second;

        generated_colormaps.push_back(view);
    }
}

sample_gl_colormap::~sample_gl_colormap() {}
void sample_gl_colormap::on_window_resize(int2 size) {}
void sample_gl_colormap::on_input(const app_input_event & event) {}
void sample_gl_colormap::on_update(const app_update_event & e) {}

void sample_gl_colormap::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    {
        const float height_per_map = height / (float)colormap::colormap_table.size();

        float current_y = 0.f;

        for (const auto & v : generated_colormaps)
        {
            view.draw(aabb_2d(128, current_y, width, current_y + height_per_map), float2(width,  height), v.texture->id());
            font.draw(aabb_2d(4, 2 + current_y, 8, current_y + 10), float2(width, height), v.name.c_str());
            current_y += height_per_map;
        }
    }

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_gl_colormap app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
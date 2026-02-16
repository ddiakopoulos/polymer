#pragma once

#ifndef polymer_gl_unreal_bloom_hpp
#define polymer_gl_unreal_bloom_hpp

#include "polymer-gfx-gl/gl-post-processing.hpp"
#include "polymer-core/util/file-io.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

inline std::vector<float> compute_gaussian_weights(int32_t kernel_radius)
{
    std::vector<float> weights(kernel_radius + 1);
    float sigma = static_cast<float>(kernel_radius) / 3.0f;
    for (int32_t i = 0; i <= kernel_radius; ++i)
        weights[i] = 0.39894228f * std::exp(-0.5f * i * i / (sigma * sigma)) / sigma;
    return weights;
}

struct gl_unreal_bloom_config
{
    bool bloom_enabled = true;
    float threshold = 0.8f;
    float knee = 0.5f;
    float strength = 1.0f;
    float radius = 0.5f;
    float exposure = 1.0f;
    float gamma = 2.2f;
    int32_t tonemap_mode = 3; // 0=none, 1=Filmic, 2=Hejl, 3=ACES 2.0, 4=ACES 1.0
};

struct gl_unreal_bloom : public gl_post_pass
{
    gl_unreal_bloom_config config;

    gl_shader brightness_shader;
    gl_shader blur_shader;
    gl_shader composite_shader;

    gl_vertex_array_object fullscreen_vao;

    gl_framebuffer bloom_fb_h[5];
    gl_framebuffer bloom_fb_v[5];
    gl_texture_2d bloom_tex_h[5];
    gl_texture_2d bloom_tex_v[5];

    gl_framebuffer output_fb;
    gl_texture_2d output_texture;

    int32_t internal_width = 0;
    int32_t internal_height = 0;

    gl_unreal_bloom(const std::string & asset_base_path)
    {
        std::string fullscreen_vert = polymer::read_file_text(asset_base_path + "/shaders/waterfall_fullscreen_vert.glsl");
        std::string bloom_base = asset_base_path + "/shaders/bloom/";

        brightness_shader = gl_shader(fullscreen_vert, polymer::read_file_text(bloom_base + "bloom_brightness_frag.glsl"));
        blur_shader = gl_shader(fullscreen_vert, polymer::read_file_text(bloom_base + "bloom_blur_frag.glsl"));
        composite_shader = gl_shader(fullscreen_vert, polymer::read_file_text(bloom_base + "bloom_composite_frag.glsl"));
    }

    ~gl_unreal_bloom() override = default;

    void setup_bloom_fbos(int32_t width, int32_t height)
    {
        int32_t mip_w = width / 2;
        int32_t mip_h = height / 2;

        for (int32_t i = 0; i < 5; ++i)
        {
            bloom_tex_h[i].setup(mip_w, mip_h, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
            glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glNamedFramebufferTexture(bloom_fb_h[i], GL_COLOR_ATTACHMENT0, bloom_tex_h[i], 0);
            bloom_fb_h[i].check_complete();

            bloom_tex_v[i].setup(mip_w, mip_h, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
            glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glNamedFramebufferTexture(bloom_fb_v[i], GL_COLOR_ATTACHMENT0, bloom_tex_v[i], 0);
            bloom_fb_v[i].check_complete();

            mip_w = std::max(1, mip_w / 2);
            mip_h = std::max(1, mip_h / 2);
        }

        output_texture.setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(output_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(output_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(output_fb, GL_COLOR_ATTACHMENT0, output_texture, 0);
        output_fb.check_complete();

        internal_width = width;
        internal_height = height;
    }

    void render(GLuint input_texture, int32_t width, int32_t height, bool render_to_screen) override
    {
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(fullscreen_vao);

        // Bloom blur passes (only when bloom is enabled)
        if (config.bloom_enabled)
        {
            int32_t mip_w = width / 2;
            int32_t mip_h = height / 2;

            // Brightness extraction -> bloom_v[0]
            glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_v[0]);
            glViewport(0, 0, mip_w, mip_h);
            glClear(GL_COLOR_BUFFER_BIT);

            brightness_shader.bind();
            brightness_shader.texture("s_hdr_color", 0, input_texture, GL_TEXTURE_2D);
            brightness_shader.uniform("u_threshold", config.threshold);
            brightness_shader.uniform("u_knee", config.knee);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            brightness_shader.unbind();

            // Multi-mip blur chain
            int32_t cur_w = mip_w;
            int32_t cur_h = mip_h;

            for (int32_t i = 0; i < 5; ++i)
            {
                if (i > 0)
                {
                    cur_w = std::max(1, cur_w / 2);
                    cur_h = std::max(1, cur_h / 2);
                }

                int32_t kernel_radius = 3 + i * 2;
                std::vector<float> weights = compute_gaussian_weights(kernel_radius);

                GLuint blur_input = (i == 0) ? static_cast<GLuint>(bloom_tex_v[0]) : static_cast<GLuint>(bloom_tex_v[i - 1]);

                // Horizontal blur
                glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_h[i]);
                glViewport(0, 0, cur_w, cur_h);
                glClear(GL_COLOR_BUFFER_BIT);

                blur_shader.bind();
                blur_shader.texture("s_source", 0, blur_input, GL_TEXTURE_2D);
                blur_shader.uniform("u_direction", linalg::aliases::float2(1.0f / cur_w, 0.0f));
                blur_shader.uniform("u_kernel_radius", kernel_radius);
                blur_shader.uniform("u_weights", kernel_radius + 1, weights);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                blur_shader.unbind();

                // Vertical blur
                glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_v[i]);
                glViewport(0, 0, cur_w, cur_h);
                glClear(GL_COLOR_BUFFER_BIT);

                blur_shader.bind();
                blur_shader.texture("s_source", 0, bloom_tex_h[i], GL_TEXTURE_2D);
                blur_shader.uniform("u_direction", linalg::aliases::float2(0.0f, 1.0f / cur_h));
                blur_shader.uniform("u_kernel_radius", kernel_radius);
                blur_shader.uniform("u_weights", kernel_radius + 1, weights);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                blur_shader.unbind();
            }
        }

        // Composite pass (always runs for tonemapping + gamma)
        if (render_to_screen)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, output_fb);
        }

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        composite_shader.bind();
        composite_shader.texture("s_hdr_color", 0, input_texture, GL_TEXTURE_2D);
        composite_shader.texture("s_bloom_0", 1, bloom_tex_v[0], GL_TEXTURE_2D);
        composite_shader.texture("s_bloom_1", 2, bloom_tex_v[1], GL_TEXTURE_2D);
        composite_shader.texture("s_bloom_2", 3, bloom_tex_v[2], GL_TEXTURE_2D);
        composite_shader.texture("s_bloom_3", 4, bloom_tex_v[3], GL_TEXTURE_2D);
        composite_shader.texture("s_bloom_4", 5, bloom_tex_v[4], GL_TEXTURE_2D);
        composite_shader.uniform("u_bloom_strength", config.bloom_enabled ? config.strength : 0.0f);
        composite_shader.uniform("u_bloom_radius", config.radius);
        composite_shader.uniform("u_exposure", config.exposure);
        composite_shader.uniform("u_gamma", config.gamma);
        composite_shader.uniform("u_tonemap_mode", config.tonemap_mode);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        composite_shader.unbind();
    }

    void resize(int32_t width, int32_t height) override
    {
        // Destroy existing resources (glTextureStorage2D allocates immutable storage)
        for (int32_t i = 0; i < 5; ++i)
        {
            bloom_tex_h[i] = gl_texture_2d();
            bloom_tex_v[i] = gl_texture_2d();
            bloom_fb_h[i] = gl_framebuffer();
            bloom_fb_v[i] = gl_framebuffer();
        }
        output_texture = gl_texture_2d();
        output_fb = gl_framebuffer();

        setup_bloom_fbos(width, height);
    }

    GLuint get_output_texture() const override
    {
        return output_texture;
    }
};

#endif // polymer_gl_unreal_bloom_hpp

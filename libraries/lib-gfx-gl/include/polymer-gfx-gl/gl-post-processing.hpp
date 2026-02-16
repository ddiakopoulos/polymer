#pragma once

#ifndef polymer_gl_post_processing_hpp
#define polymer_gl_post_processing_hpp

#include "polymer-gfx-gl/gl-api.hpp"

#include <vector>
#include <memory>

struct gl_post_pass
{
    bool enabled = true;
    virtual ~gl_post_pass() = default;
    virtual void render(GLuint input_texture, int32_t width, int32_t height, bool render_to_screen) = 0;
    virtual void resize(int32_t width, int32_t height) = 0;
    virtual GLuint get_output_texture() const = 0;
};

struct gl_effect_composer
{
    std::vector<std::shared_ptr<gl_post_pass>> passes;

    void add_pass(std::shared_ptr<gl_post_pass> pass)
    {
        passes.push_back(std::move(pass));
    }

    void render(GLuint input_texture, int32_t width, int32_t height)
    {
        int32_t last_enabled = -1;
        for (int32_t i = static_cast<int32_t>(passes.size()) - 1; i >= 0; --i)
        {
            if (passes[i]->enabled)
            {
                last_enabled = i;
                break;
            }
        }

        if (last_enabled < 0) return;

        GLuint current_input = input_texture;
        for (int32_t i = 0; i <= last_enabled; ++i)
        {
            if (!passes[i]->enabled) continue;
            bool is_last = (i == last_enabled);
            passes[i]->render(current_input, width, height, is_last);
            if (!is_last) current_input = passes[i]->get_output_texture();
        }
    }

    void resize(int32_t width, int32_t height)
    {
        for (std::shared_ptr<gl_post_pass> & pass : passes)
        {
            pass->resize(width, height);
        }
    }
};

#endif // polymer_gl_post_processing_hpp

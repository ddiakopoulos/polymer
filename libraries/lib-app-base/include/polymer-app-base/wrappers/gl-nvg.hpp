#pragma once

#ifndef nvg_util_h
#define nvg_util_h

#include <stdint.h>
#include <vector>
#include <string>
#include <stdlib.h>

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-app-base/glfw-app.hpp"

#include "nanovg/nanovg.h"

namespace polymer
{
    // NanoVG Factory Functions
    NVGcontext * make_nanovg_context(int flags);
    void release_nanovg_context(NVGcontext * context);

    class nvg_font
    {
        std::vector<uint8_t> buffer;
        NVGcontext * nvg;
    public:
        int id;
        nvg_font(NVGcontext * nvg, const std::string & name, const std::vector<uint8_t> & buffer);
        size_t get_cursor_location(const std::string & text, float fontSize, int xCoord) const;
    };

    class gl_nvg_surface
    {
        enum ContextFlags
        {
            CTX_ANTIALIAS = 1 << 0,             // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
            CTX_STENCIL_STROKES = 1 << 1,       // Flag indicating if strokes should be drawn using stencil buffer.
            CTX_DEBUG = 1 << 2,                 // Flag indicating that additional debug checks are done.
        };

        NVGcontext * nvg;
        std::shared_ptr<nvg_font> text_fontface, icon_fontface;
        const float2 size;
        const uint32_t num_surfaces;
        
        std::vector<gl_framebuffer> framebuffer;
        std::vector<gl_texture_2d> texture;

    public:

        struct font_data
        {
            // Required
            std::string text_font_name;
            std::vector<uint8_t> text_font_binary;

            // Optional
            std::string icon_font_name;
            std::vector<uint8_t> icon_font_binary;
        };

        gl_nvg_surface(const uint32_t num_surfaces, float2 surface_size, const font_data & font_data) 
            : num_surfaces(num_surfaces), size(surface_size)
        {
            nvg = make_nanovg_context(CTX_ANTIALIAS);
            if (!nvg) throw std::runtime_error("error initializing nanovg context");

            text_fontface = std::make_shared<nvg_font>(nvg, font_data.text_font_name, font_data.text_font_binary);

            // icon font is optional
            if (font_data.icon_font_binary.size())
            {
                icon_fontface = std::make_shared<nvg_font>(nvg, font_data.icon_font_name, font_data.icon_font_binary);
            }

            // Setup surfaces
            framebuffer.resize(num_surfaces);
            texture.resize(num_surfaces);

            for (uint32_t i = 0; i < num_surfaces; ++i)
            {
                glTextureImage2DEXT(texture[i], GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTextureParameteriEXT(texture[i], GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteriEXT(texture[i], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteriEXT(texture[i], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteriEXT(texture[i], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTextureParameteriEXT(texture[i], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                glNamedFramebufferTexture2DEXT(framebuffer[i], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[i], 0);
                framebuffer[i].check_complete();
            }
        }

        ~gl_nvg_surface() { release_nanovg_context(nvg); }

        NVGcontext * pre_draw(GLFWwindow * window, const uint32_t surface_idx)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer[surface_idx]);
            glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            nvgBeginFrame(nvg, size.x, size.y, 1.0);
            return nvg;
        }

        void post_draw()
        {
            nvgEndFrame(nvg);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glUseProgram(0);
        }

        gl_texture_2d & surface_texture(const uint32_t surface_idx)
        {
            return texture[surface_idx];
        }

        float2 surface_size() const { return size; }

        float draw_text_quick(const std::string & txt, const float text_size, const float2 position, const NVGcolor color)
        {
            nvgFontFaceId(nvg, text_fontface->id);
            nvgFontSize(nvg, text_size);
            float bounds[4];
            const float w = nvgTextBounds(nvg, 0, 0, txt.c_str(), NULL, bounds); // xmin, ymin, xmax, ymax
            const float width = (bounds[2] - bounds[0]) / 2.f;

            const float textX = position.x - width, textY = position.y + 8;
            nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgBeginPath(nvg);
            nvgFillColor(nvg, color);
            return nvgText(nvg, textX, textY, txt.c_str(), nullptr);
        }
    };

} // end namespace polymer

#endif
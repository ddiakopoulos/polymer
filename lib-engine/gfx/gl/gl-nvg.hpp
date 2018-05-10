#pragma once

#ifndef nvg_util_h
#define nvg_util_h

#include "gl-api.hpp"
#include "nanovg/nanovg.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include "file_io.hpp"

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
        nvg_font(NVGcontext * nvg, const std::string & name, std::vector<uint8_t> & buffer);
        size_t get_cursor_location(const std::string & text, float fontSize, int xCoord) const;
    };

   // A simple wrapper for an a NanoVG Context. Usage:
   // > surface.reset(new gl_nvg_surface(width, height, "source_code_pro_regular", "source_code_pro_regular"));
   // > auto nvg = surface->pre_draw(window);
   // > surface->post_draw();

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
        linalg::aliases::float2 lastCursor;

    public:

        gl_nvg_surface(float width, float height, const std::string & text_font, const std::string & icon_font)
        {
            nvg = make_nanovg_context(CTX_ANTIALIAS | CTX_STENCIL_STROKES);
            if (!nvg) throw std::runtime_error("error initializing nanovg context");
            text_fontface = std::make_shared<nvg_font>(nvg, text_font, polymer::read_file_binary("../assets/fonts/" + text_font + ".ttf"));
            icon_fontface = std::make_shared<nvg_font>(nvg, icon_font, polymer::read_file_binary("../assets/fonts/" + icon_font + ".ttf"));
        }

        ~gl_nvg_surface() { release_nanovg_context(nvg); }

        NVGcontext * pre_draw(GLFWwindow * window)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            nvgBeginFrame(nvg, width, height, 1.0);
            return nvg;
        }

        void post_draw()
        {
            nvgEndFrame(nvg);
        }
    };

} // end namespace polymer

#endif
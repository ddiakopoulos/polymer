#pragma once

#ifndef nvg_util_h
#define nvg_util_h

#include "gl-api.hpp"
#include "nanovg.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include "file_io.hpp"

class NvgFont
{
    std::vector<uint8_t> buffer;
    NVGcontext * nvg;
public:
    int id;
    NvgFont(NVGcontext * nvg, const std::string & name, std::vector<uint8_t> & buffer);
    size_t get_cursor_location(const std::string & text, float fontSize, int xCoord) const;
};

NVGcontext * make_nanovg_context(int flags);
void release_nanovg_context(NVGcontext * context);

/*  
 * A simple wrapper for an NVGContext. Usage: 
 * > surface.reset(new NvgSurface(width, height, "source_code_pro_regular", "source_code_pro_regular"));
 * > auto nvg = surface->pre_draw(window);
 * > surface->post_draw();
 */

class NvgSurface
{
    enum ContextFlags 
    {
        CTX_ANTIALIAS = 1 << 0,             // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
        CTX_STENCIL_STROKES = 1 << 1,       // Flag indicating if strokes should be drawn using stencil buffer.
        CTX_DEBUG = 1 << 2,                 // Flag indicating that additional debug checks are done.
    };

    NVGcontext * nvg;
    std::shared_ptr<NvgFont> text_fontface, icon_fontface;
    linalg::aliases::float2 lastCursor;
public:

    NvgSurface(float width, float height, const std::string & text_font, const std::string & icon_font)
    {
        nvg = make_nanovg_context(CTX_ANTIALIAS | CTX_STENCIL_STROKES);
        if (!nvg) throw std::runtime_error("error initializing nanovg context");
        text_fontface = std::make_shared<NvgFont>(nvg, text_font, polymer::read_file_binary("../assets/fonts/" + text_font + ".ttf"));
        icon_fontface = std::make_shared<NvgFont>(nvg, icon_font, polymer::read_file_binary("../assets/fonts/" + icon_font + ".ttf"));
    }

    ~NvgSurface() { release_nanovg_context(nvg); }

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


#endif
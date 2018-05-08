#include "gl-nvg.hpp"

#include <string>
#include <vector>

NvgFont::NvgFont(NVGcontext * nvg, const std::string & name, std::vector<uint8_t> & buffer)
{
    this->buffer = std::move(buffer);
    this->nvg = nvg;
    id = nvgCreateFontMem(nvg, name.c_str(), this->buffer.data(), (int) this->buffer.size(), 0);
    if (id < 0) throw std::runtime_error("Failed to load font: " + name);
}

size_t NvgFont::get_cursor_location(const std::string & text, float fontSize, int xCoord) const
{
    std::vector<NVGglyphPosition> positions(text.size());
    nvgFontSize(nvg, fontSize);
    nvgFontFaceId(nvg, id);
    nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    positions.resize(nvgTextGlyphPositions(nvg, 0, 0, text.data(), text.data() + (int) text.size(), positions.data(), (int) positions.size()));
    for (size_t i = 0; i<positions.size(); ++i)
    {
        if (xCoord < positions[i].maxx) return i;
    }
    return positions.size();
}

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"

NVGcontext * make_nanovg_context(int flags) { return nvgCreateGL3(flags); }
void release_nanovg_context(NVGcontext * ctx) { nvgDeleteGL3(ctx); }
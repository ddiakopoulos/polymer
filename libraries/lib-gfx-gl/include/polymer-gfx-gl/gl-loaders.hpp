#pragma once

#ifndef gl_loaders_hpp
#define gl_loaders_hpp

#include "polymer-gfx-gl/gl-api.hpp"

#include "polymer-core/util/file-io.hpp"
#include "polymer-core/tools/image-buffer.hpp"

#include "stb/stb_image.h" 
#include "gli/gli.hpp"

namespace polymer
{

    inline void flip_image_inplace(unsigned char * pixels, const uint32_t width, const uint32_t height, const uint32_t bytes_per_pixel)
    {
        const size_t stride = width * bytes_per_pixel;
        std::vector<unsigned char> row(stride);
        unsigned char * low = pixels;
        unsigned char * high = &pixels[(height - 1) * stride];

        for (; low < high; low += stride, high -= stride)
        {
            std::memcpy(row.data(), low, stride);
            std::memcpy(low, high, stride);
            std::memcpy(high, row.data(), stride);
        }
    }

    inline std::vector<uint8_t> load_image_data(const std::string & path, int32_t * out_width = nullptr, int32_t * out_height = nullptr, int32_t * out_bpp = nullptr, bool flip = false)
    {
        auto binaryFile = read_file_binary(path);

        if (flip) stbi_set_flip_vertically_on_load(1);
        else stbi_set_flip_vertically_on_load(0);

        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int)binaryFile.size(), &width, &height, &nBytes, 0);
        std::vector<uint8_t> d(width * height * nBytes);
        std::memcpy(d.data(), data, nBytes * width * height);
        stbi_image_free(data);

        if (out_bpp) *out_bpp = nBytes;
        if (out_width) *out_width = width;
        if (out_height) *out_height = height;

        return d;
    }

    inline image_buffer<uint8_t> load_image_buffer(const std::string & path, bool flip = false)
    {
        int32_t width {0};
        int32_t height {0};
        int32_t channels {0};
        std::vector<uint8_t> data = load_image_data(path, &width, &height, &channels, flip);

        image_buffer<uint8_t> result(int2(width, height), channels);
        std::memcpy(result.data(), data.data(), result.size_bytes());

        return std::move(result);
    }

    inline gl_texture_2d load_image(const std::string & path, bool flip = false)
    {
        auto binaryFile = polymer::read_file_binary(path);

        if (flip) stbi_set_flip_vertically_on_load(1);
        else stbi_set_flip_vertically_on_load(0);

        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int)binaryFile.size(), &width, &height, &nBytes, 0);

        gl_texture_2d tex;
        switch (nBytes)
        {
        case 1: tex.setup(width, height, GL_RED, GL_RED, GL_UNSIGNED_BYTE, data, true); break;
        case 2: tex.setup(width, height, GL_RED, GL_RED, GL_UNSIGNED_SHORT, data, true); break;
        case 3: tex.setup(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, data, true); break;
        case 4: tex.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, data, true); break;
        default: throw std::runtime_error("unsupported number of channels");
        }

        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        stbi_image_free(data);
        return tex;
    }

    inline gl_texture_2d load_cubemap(const gli::texture_cube & tex)
    {
        gl_texture_2d t;

        for (gli::texture_cube::size_type Face = 0; Face < 6; ++Face)
        {
            for (std::size_t Level = 0; Level < tex.levels(); ++Level)
            {
                gli::gl GL(gli::gl::PROFILE_GL33);
                gli::gl::format const fmt = GL.translate(tex.format(), tex.swizzles());
                auto w = GLsizei(tex[Face][Level].extent().x), h = GLsizei(tex[Face][Level].extent().y);
                glTextureImage2DEXT(t, GL_TEXTURE_CUBE_MAP_POSITIVE_X + GLenum(Face), GLint(Level), fmt.Internal, w, h, 0, fmt.External, fmt.Type, tex[Face][Level].data());
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, (GLint)tex.base_level());
                glTextureParameteriEXT(t, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, (GLint)tex.max_level());
                gl_check_error(__FILE__, __LINE__);
            }
        }

        return t;
    };

} // end namespace polymer

#endif // gl_loaders_hpp

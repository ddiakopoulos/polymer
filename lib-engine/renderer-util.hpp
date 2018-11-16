#pragma once

#ifndef polymer_renderer_util_hpp
#define polymer_renderer_util_hpp

#include "shader-library.hpp"
#include "logging.hpp"

#include "third_party/tinyexr/tinyexr.h"

namespace polymer
{

    void load_required_renderer_assets(const std::string & base_path, gl_shader_monitor & monitor)
    {
        try
        {
            monitor.watch("sky-hosek",
                base_path + "/shaders/sky_vert.glsl",
                base_path + "/shaders/sky_hosek_frag.glsl");

            monitor.watch("ibl",
                base_path + "/shaders/ibl_vert.glsl",
                base_path + "/shaders/ibl_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("default-shader",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/default_material_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("renderer-wireframe",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/wireframe_frag.glsl",
                base_path + "/shaders/renderer/wireframe_geom.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("depth-prepass",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/no_op_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("cascaded-shadows",
                base_path + "/shaders/renderer/shadowcascade_vert.glsl",
                base_path + "/shaders/renderer/shadowcascade_frag.glsl",
                base_path + "/shaders/renderer/shadowcascade_geom.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("phong-forward-lighting",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/phong_material_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("pbr-forward-lighting",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/pbr_material_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("post-tonemap",
                base_path + "/shaders/renderer/post_tonemap_vert.glsl",
                base_path + "/shaders/renderer/post_tonemap_frag.glsl");
        }
        catch (const std::exception & e)
        {
            log::get()->engine_log->info("load_required_renderer_assets() failed: {}", e.what());
        }
    }

    inline void export_exr_image(const std::string & path, const uint32_t w, const uint32_t h, const uint32_t c, std::vector<float> & buffer)
    {
        const char * err{ nullptr };
        int result = SaveEXR(buffer.data(), w, h, c, 0, path.c_str(), &err);
        if (result != TINYEXR_SUCCESS)
        {
            if (err) log::get()->engine_log->info("error with export_exr_image {} ({})", err, result);
            else log::get()->engine_log->info("error error with export_exr_image {}", result);
        }
    }

    inline std::vector<float> load_exr_image(const std::string & path, uint32_t & out_width, uint32_t & out_height, uint32_t & out_channels)
    {
        std::vector<uint8_t> in_buffer;

        try { in_buffer = read_file_binary(path); }
        catch (const std::exception & e) { log::get()->engine_log->info("error loading file {} ({}) ", path, e.what()); }

        int exr_result{ 0 };
        const char * exr_error{ nullptr };

        // Read EXR version
        EXRVersion exr_version;
        exr_result = ParseEXRVersionFromMemory(&exr_version, in_buffer.data(), in_buffer.size());
        if (exr_result != 0) throw std::runtime_error("ParseEXRVersionFromMemory returned with error: " + std::string(exr_error));
        if (exr_version.multipart) throw std::runtime_error("multipart OpenEXR files are not yet supported");

        // Read EXR header
        EXRHeader exr_header;
        exr_result = ParseEXRHeaderFromMemory(&exr_header, &exr_version, in_buffer.data(), in_buffer.size(), &exr_error);
        if (exr_result != 0) throw std::runtime_error("ParseEXRHeaderFromMemory returned with error: " + std::string(exr_error));

        // Initialize EXR image
        EXRImage exr_image;
        InitEXRImage(&exr_image);

        // Request type (float)
        for (int i = 0; i < exr_header.num_channels; ++i) exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;

        exr_result = LoadEXRImageFromMemory(&exr_image, &exr_header, in_buffer.data(), in_buffer.size(), &exr_error);
        if (exr_result != 0) throw std::runtime_error("LoadEXRImageFromMemory returned with error: " + std::string(exr_error));

        std::array<int, 4> rgba_idx{ -1, -1, -1, -1 };
        for (int c = 0; c < exr_header.num_channels; ++c)
        {
                 if (strcmp(exr_header.channels[c].name, "R") == 0) rgba_idx[0] = c;
            else if (strcmp(exr_header.channels[c].name, "G") == 0) rgba_idx[1] = c;
            else if (strcmp(exr_header.channels[c].name, "B") == 0) rgba_idx[2] = c;
            else if (strcmp(exr_header.channels[c].name, "A") == 0) rgba_idx[3] = c;
        }

        const size_t exr_image_size = exr_header.num_channels * static_cast<size_t>(exr_image.width) * static_cast<size_t>(exr_image.height);
        std::vector<float> out_buffer(exr_image_size);

        auto load_from_channel = [&](const int channel, const int component)
        {
            for (int y = 0; y < exr_image.height; ++y)
            {
                for (int x = 0; x < exr_image.width; ++x)
                {
                    out_buffer[exr_image.num_channels * (y * exr_image.width + x) + component] =
                        reinterpret_cast<float**>(exr_image.images)[channel][(y * exr_image.width + x)];
                }
            }
        };

        if (rgba_idx[0] != -1) load_from_channel(rgba_idx[0], 0);
        if (rgba_idx[1] != -1) load_from_channel(rgba_idx[1], 1);
        if (rgba_idx[2] != -1) load_from_channel(rgba_idx[2], 2);
        if (rgba_idx[3] != -1) load_from_channel(rgba_idx[3], 3);

        out_width = exr_image.width;
        out_height = exr_image.height;
        out_channels = exr_image.num_channels;

        FreeEXRErrorMessage(exr_error);
        FreeEXRHeader(&exr_header);
        FreeEXRImage(&exr_image);

        return out_buffer;
    }
}

#endif // end polymer_renderer_util_hpp
  
#pragma once

#ifndef polymer_renderer_util_hpp
#define polymer_renderer_util_hpp

#include "shader-library.hpp"
#include "logging.hpp"

#include "tinyexr/tinyexr.h"

namespace polymer
{

    void load_required_renderer_assets(const std::string & base_path, gl_shader_monitor & monitor)
    {            
        scoped_timer t("load_required_renderer_assets");

        // The Polymer editor has a number of "intrinsic" mesh assets that are loaded from disk at runtime. These primarily
        // add to the number of objects that can be quickly prototyped with, along with the usual set of procedural mesh functions
        // included with Polymer.
        {
            for (auto & entry : recursive_directory_iterator(base_path))
            {
                const size_t root_len = base_path.length(), ext_len = entry.path().extension().string().length();
                auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
                for (auto & chr : path) if (chr == '\\') chr = '/';

                if (entry.path().extension().string() == ".mesh")
                {
                    auto geo_import = import_polymer_binary_model(path);
                    create_handle_for_asset(std::string(get_filename_without_extension(path)).c_str(), make_mesh_from_geometry(geo_import));
                    create_handle_for_asset(std::string(get_filename_without_extension(path)).c_str(), std::move(geo_import));
                }
            }
        }

        try
        {
            monitor.watch("no-op",
                base_path + "/shaders/renderer/no_op_vert.glsl",
                base_path + "/shaders/renderer/no_op_frag.glsl",
                base_path + "/shaders/renderer");

            // [utility] used for rendering debug meshes 
            monitor.watch("debug-renderer",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/debug_renderer_frag.glsl",
                base_path + "/shaders/renderer");

            // [utility] default shader used when none are specified (shows world-space normals)
            monitor.watch("default-shader",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/default_material_frag.glsl",
                base_path + "/shaders/renderer");

            // [utility] wireframe rendering (currently for gizmo selection in scene editor)
            monitor.watch("renderer-wireframe",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/wireframe_frag.glsl",
                base_path + "/shaders/renderer/wireframe_geom.glsl",
                base_path + "/shaders/renderer");
            
            // [utility] render a single unlit diffuse texture (currently for imgui surfaces)
            monitor.watch("unlit-texture",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/unlit_texture_frag.glsl",
                base_path + "/shaders/renderer");

            // [utility] used for the xr laser pointer
            monitor.watch("xr-laser",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/xr_laser_frag.glsl",
                base_path + "/shaders/renderer");

            // [utility] used for shading the gizmo (both xr and desktop)
            monitor.watch("unlit-vertex-color",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/unlit_vertex_color_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] render a procedural sky
            monitor.watch("sky-hosek",
                base_path + "/shaders/sky_vert.glsl",
                base_path + "/shaders/sky_hosek_frag.glsl");

            // [renderer-pbr] render a cubemap
            monitor.watch("cubemap",
                base_path + "/shaders/cubemap_vert.glsl",
                base_path + "/shaders/cubemap_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] depth prepass
            monitor.watch("depth-prepass",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/no_op_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] cascaded shadow maps
            monitor.watch("cascaded-shadows",
                base_path + "/shaders/renderer/shadowcascade_vert.glsl",
                base_path + "/shaders/renderer/shadowcascade_frag.glsl",
                base_path + "/shaders/renderer/shadowcascade_geom.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] blinn-phong forward model
            monitor.watch("phong-forward-lighting",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/phong_material_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] standard gLTF-style PBR forward model 
            monitor.watch("pbr-forward-lighting",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/pbr_material_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] post-process tonemapping
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
  
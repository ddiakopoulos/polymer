#pragma once

#ifndef polymer_renderer_util_hpp
#define polymer_renderer_util_hpp

#include "polymer-engine/shader-library.hpp"
#include "polymer-engine/logging.hpp"

#include "tinyexr/tinyexr.h"

namespace polymer
{

    inline void load_required_renderer_assets(const std::string & base_path, gl_shader_monitor & monitor)
    {            
        simple_cpu_timer t;
        t.start();
        
        // Load default IBL cubemaps for PBR rendering
        auto default_radiance_bin = read_file_binary(base_path + "/textures/envmaps/default-radiance-cubemap.dds");
        auto default_irradiance_bin = read_file_binary(base_path + "/textures/envmaps/default-irradiance-cubemap.dds");

        gli::texture_cube r_cubemap_as_gli_cubemap(gli::load_dds((char *) default_radiance_bin.data(), default_radiance_bin.size()));
        if (r_cubemap_as_gli_cubemap.empty())
        {
            log::get()->import_log->error("Failed to load default-radiance-cubemap.dds");
        }
        else
        {
            log::get()->import_log->info("Loaded default-radiance-cubemap: {}x{}, {} levels",
                r_cubemap_as_gli_cubemap.extent().x, r_cubemap_as_gli_cubemap.extent().y, r_cubemap_as_gli_cubemap.levels());
            create_handle_for_asset("default-radiance-cubemap", load_cubemap(r_cubemap_as_gli_cubemap));
        }
        gl_check_error(__FILE__, __LINE__);

        gli::texture_cube irr_cubemap_as_gli_cubemap(gli::load_dds((char *) default_irradiance_bin.data(), default_irradiance_bin.size()));
        if (irr_cubemap_as_gli_cubemap.empty())
        {
            log::get()->import_log->error("Failed to load default-irradiance-cubemap.dds");
        }
        else
        {
            log::get()->import_log->info("Loaded default-irradiance-cubemap: {}x{}, {} levels",
                irr_cubemap_as_gli_cubemap.extent().x, irr_cubemap_as_gli_cubemap.extent().y, irr_cubemap_as_gli_cubemap.levels());
            create_handle_for_asset("default-irradiance-cubemap", load_cubemap(irr_cubemap_as_gli_cubemap));
        }
        gl_check_error(__FILE__, __LINE__);

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

            // [renderer-pbr] standard gLTF 2.0-style PBR forward model 
            monitor.watch("pbr-forward-lighting",
                base_path + "/shaders/renderer/renderer_vert.glsl",
                base_path + "/shaders/renderer/pbr_material_frag.glsl",
                base_path + "/shaders/renderer");

            // [renderer-pbr] post-process tonemapping
            monitor.watch("post-tonemap",
                base_path + "/shaders/renderer/post_tonemap_vert.glsl",
                base_path + "/shaders/renderer/post_tonemap_frag.glsl");

            // [renderer-pbr] particle rendering
            monitor.watch("particle-system",
                base_path + "/shaders/renderer/particle_system_vert.glsl",
                base_path + "/shaders/renderer/particle_system_frag.glsl");
        }
        catch (const std::exception & e)
        {
            log::get()->import_log->error("load_required_renderer_assets() failed: {}", e.what());
        }

        t.stop();

        log::get()->import_log->info("load_required_renderer_assets completed in {} ms", t.elapsed_ms());
    }

    // Generates a DFG (D*F*G / (4*NdotL*NdotV)) integration LUT for IBL specular
    // Based on Filament's implementation - stores (scale, bias) for split-sum approximation
    inline gl_texture_2d generate_dfg_lut(uint32_t resolution = 128)
    {
        std::vector<float> lut_data(resolution * resolution * 2);
        const float inv_resolution = 1.0f / static_cast<float>(resolution);

        auto radical_inverse_vdc = [](uint32_t bits) -> float
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        };

        auto hammersley = [&radical_inverse_vdc](uint32_t i, uint32_t n) -> float2
        {
            return float2(static_cast<float>(i) / static_cast<float>(n), radical_inverse_vdc(i));
        };

        auto importance_sample_ggx = [](float2 xi, float roughness) -> float3
        {
            float a = roughness * roughness;
            float phi = 2.0f * 3.14159265359f * xi.x;
            float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
            float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
            return float3(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);
        };

        auto visibility_smith_ggx_correlated = [](float ndotv, float ndotl, float a) -> float
        {
            float a2 = a * a;
            float ggxv = ndotl * std::sqrt(ndotv * ndotv * (1.0f - a2) + a2);
            float ggxl = ndotv * std::sqrt(ndotl * ndotl * (1.0f - a2) + a2);
            return 0.5f / (ggxv + ggxl);
        };

        const uint32_t sample_count = 1024;

        for (uint32_t y = 0; y < resolution; ++y)
        {
            float roughness = (static_cast<float>(y) + 0.5f) * inv_resolution;
            roughness = std::max(roughness, 0.089f);
            float a = roughness * roughness;

            for (uint32_t x = 0; x < resolution; ++x)
            {
                float ndotv = (static_cast<float>(x) + 0.5f) * inv_resolution;
                ndotv = std::max(ndotv, 0.001f);

                float3 V = float3(std::sqrt(1.0f - ndotv * ndotv), 0.0f, ndotv);
                float scale = 0.0f;
                float bias = 0.0f;

                for (uint32_t i = 0; i < sample_count; ++i)
                {
                    float2 xi = hammersley(i, sample_count);
                    float3 H = importance_sample_ggx(xi, roughness);
                    float3 L = 2.0f * dot(V, H) * H - V;

                    float ndotl = L.z > 0.0f ? L.z : 0.0f;
                    float ndoth = H.z > 0.0f ? H.z : 0.0f;
                    float vdoth_val = dot(V, H);
                    float vdoth = vdoth_val > 0.0f ? vdoth_val : 0.0f;

                    if (ndotl > 0.0f)
                    {
                        float vis = visibility_smith_ggx_correlated(ndotv, ndotl, a);
                        float vis_scaled = vis * vdoth * ndotl / (ndoth > 0.0001f ? ndoth : 0.0001f);
                        float fc = std::pow(1.0f - vdoth, 5.0f);
                        scale += vis_scaled * (1.0f - fc);
                        bias += vis_scaled * fc;
                    }
                }

                scale *= 4.0f / static_cast<float>(sample_count);
                bias *= 4.0f / static_cast<float>(sample_count);

                uint32_t idx = (y * resolution + x) * 2;
                lut_data[idx + 0] = scale;
                lut_data[idx + 1] = bias;
            }
        }

        gl_texture_2d dfg_lut;
        dfg_lut.setup(resolution, resolution, GL_RG16F, GL_RG, GL_FLOAT, lut_data.data());
        glTextureParameteri(dfg_lut, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(dfg_lut, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(dfg_lut, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(dfg_lut, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        log::get()->import_log->info("Generated DFG LUT: {}x{}", resolution, resolution);

        return dfg_lut;
    }

    inline void export_exr_image(const std::string & path, const uint32_t w, const uint32_t h, const uint32_t c, std::vector<float> & buffer)
    {
        const char * err{ nullptr };
        int result = SaveEXR(buffer.data(), w, h, c, 0, path.c_str(), &err);
        if (result != TINYEXR_SUCCESS)
        {
            if (err) log::get()->import_log->error("export_exr_image {} ({})", err, result);
            else log::get()->import_log->error("export_exr_image {}", result);
        }
    }

    inline std::vector<float> load_exr_image(const std::string & path, uint32_t & out_width, uint32_t & out_height, uint32_t & out_channels)
    {
        std::vector<uint8_t> in_buffer;

        try { in_buffer = read_file_binary(path); }
        catch (const std::exception & e) { log::get()->import_log->error("loading file {} ({}) ", path, e.what()); }

        try
        {
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
        catch (const std::exception & e) { log::get()->import_log->error("tinyexr failure {} ", e.what()); }

        return {};
    }
}

#endif // end polymer_renderer_util_hpp
  
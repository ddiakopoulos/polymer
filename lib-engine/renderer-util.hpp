#pragma once

#ifndef polymer_renderer_util_hpp
#define polymer_renderer_util_hpp

#include "shader-library.hpp"
#include "logging.hpp"

namespace polymer
{

    void load_required_renderer_assets(const std::string base_path, gl_shader_monitor & monitor)
    {
        try
        {
            monitor.watch("ibl",
                base_path + "/shaders/ibl_vert.glsl",
                base_path + "/shaders/ibl_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("renderer-wireframe",
                base_path + "/shaders/renderer/forward_lighting_vert.glsl",
                base_path + "/shaders/renderer/wireframe_frag.glsl",
                base_path + "/shaders/renderer/wireframe_geom.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("default-shader",
                base_path + "/shaders/renderer/forward_lighting_vert.glsl",
                base_path + "/shaders/renderer/default_material_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("sky-hosek",
                base_path + "/shaders/sky_vert.glsl",
                base_path + "/shaders/sky_hosek_frag.glsl");

            monitor.watch("depth-prepass",
                base_path + "/shaders/renderer/depth_prepass_vert.glsl",
                base_path + "/shaders/renderer/depth_prepass_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("cascaded-shadows",
                base_path + "/shaders/renderer/shadowcascade_vert.glsl",
                base_path + "/shaders/renderer/shadowcascade_frag.glsl",
                base_path + "/shaders/renderer/shadowcascade_geom.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("pbr-forward-lighting",
                base_path + "/shaders/renderer/forward_lighting_vert.glsl",
                base_path + "/shaders/renderer/forward_lighting_frag.glsl",
                base_path + "/shaders/renderer");

            monitor.watch("post-tonemap",
                base_path + "/shaders/renderer/post_tonemap_vert.glsl",
                base_path + "/shaders/renderer/post_tonemap_frag.glsl");
        }
        catch (const std::exception & e)
        {
            std::cout << "Could not load required renderer asset: " << e.what() << std::endl;
        }
    }
}

#endif // end polymer_renderer_util_hpp
  
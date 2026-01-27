#pragma once

#ifndef polymer_gaussian_splat_io_hpp
#define polymer_gaussian_splat_io_hpp

#include "polymer-core/math/math-core.hpp"
#include <vector>
#include <string>

namespace polymer
{

    struct gaussian_vertex
    {
        float4 position;       // xyz + w=1
        float4 scale_opacity;  // scale xyz (exp applied) + sigmoid(opacity)
        float4 rotation;       // quaternion xyzw (normalized)
        float shs[48];         // 16 SH coeffs x 3 RGB (interleaved)
    };

    struct gaussian_splat_scene
    {
        std::vector<gaussian_vertex> vertices;
        uint32_t sh_degree{ 3 };
    };

    // Check if a PLY file contains gaussian splat data by looking for
    // characteristic properties: opacity, scale_0, rot_0, f_dc_0
    bool is_gaussian_splat_ply(const std::string & path);

    // Import a gaussian splat PLY file
    // Applies exp() to scales, sigmoid to opacity, normalizes quaternions,
    // and reorganizes SH from PLY layout (all R, all G, all B) to interleaved RGB
    gaussian_splat_scene import_gaussian_splat_ply(const std::string & path);

} // end namespace polymer

#endif // end polymer_gaussian_splat_io_hpp

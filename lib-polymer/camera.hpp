#pragma once

#ifndef polymer_camera_hpp
#define polymer_camera_hpp

#include "math-core.hpp"
#include "util.hpp"
#include "geometry.hpp"

namespace polymer
{
    ////////////////////////////
    //   perspective_camera   //
    ////////////////////////////

    struct perspective_camera
    {
        transform pose;

        float vfov{ 1.3f }; // ~75 degrees
        float nearclip{ 0.01f };
        float farclip{ 24.f };

        float4x4 get_view_matrix() const { return pose.view_matrix(); }
        float4x4 get_projection_matrix(float aspectRatio) const { return make_projection_matrix(vfov, aspectRatio, nearclip, farclip); }

        float3 get_view_direction() const { return normalize(-pose.zdir()); }
        float3 get_eye_point() const { return pose.position; }

        void look_at(const float3 & target) { pose = lookat_rh(pose.position, target); }
        void look_at(const float3 & eyePoint, const float3 target) { pose = lookat_rh(eyePoint, target); }
        void look_at(const float3 & eyePoint, float3 const & target, float3 const & worldup) { pose = lookat_rh(eyePoint, target, worldup); }

        ray get_world_ray(const float2 cursor, const float2 viewport) const
        {
            const float aspect = viewport.x / viewport.y;
            const ray r = ray_from_viewport_pixel(cursor, viewport, get_projection_matrix(aspect));
            return pose * r;
        }
    };

} // end namespace polymer

#endif // end polymer_camera_hpp

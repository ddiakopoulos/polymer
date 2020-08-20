#pragma once

#ifndef polymer_camera_hpp
#define polymer_camera_hpp

#include "math-core.hpp"
#include "util.hpp"
#include "geometry.hpp"
#include <numeric>

namespace polymer
{

    /*
     * local space      [-∞, ∞] - (e.g. model vertices)
     * world space      [-∞, ∞] - model_matrix
     * camera space     [-∞, ∞] - view_matrix
     * ndc space        [-1, 1] - projection matrix (clip space)
     * projection space [ 0, 1] - do perspective divide
     * screen space     [ 0, 0] to [width, height]
     */

    struct perspective_camera
    {
        transform pose;

        float vfov{ 1.3f }; // ~75 degrees
        float nearclip{ 0.01f };
        float farclip{ 24.f };

        float4x4 get_view_matrix() const { return pose.view_matrix(); }
        float4x4 get_projection_matrix(const float aspect) const { return make_projection_matrix(vfov, aspect, nearclip, farclip); }

        float3 get_view_direction() const { return normalize(-pose.zdir()); }
        float3 get_eye_point() const { return pose.position; }

        void look_at(const float3 & target) { pose = lookat_rh(pose.position, target); }
        void look_at(const float3 & eyePoint, const float3 target) { pose = lookat_rh(eyePoint, target); }
        void look_at(const float3 & eyePoint, float3 const & target, float3 const & worldup) { pose = lookat_rh(eyePoint, target, worldup); }

        ray get_world_ray(const float2 screenspace_coord, const float2 screen_size) const
        {
            const float aspect = screen_size.x / screen_size.y;
            const ray r = ray_from_viewport_pixel(screenspace_coord, screen_size, get_projection_matrix(aspect));
            return pose * r;
        }

        // Project a point in (eye/view/camera) space to NDC coords. Returns a point in the NDC [-1, +1] range. 
        float3 project_point(const float3 & point, const float aspect_ratio) const
        {
            float4 pp4 = get_projection_matrix(aspect_ratio) * float4(point.x, point.y, point.z, 1.f);

            if (std::abs(pp4.w) > 1e-7f)
            {
                const float inv_w = 1.0f / pp4.w;
                pp4.x *= inv_w;
                pp4.y *= inv_w;
                pp4.z *= inv_w;
            }
            else
            {
                pp4.x = 0.0f;
                pp4.y = 0.0f;
                pp4.z = 0.0f;
            }

            return float3(pp4.x, pp4.y, pp4.z);
        }

        // Given a coordinate in view space, return a 2D point in NDC coordinates. 
        float2 view_to_ndc_coord(const float3 & view_coord, const float aspect_ratio) const
        {
            float3 projPoint = project_point(view_coord, aspect_ratio);
            return float2(projPoint.x, projPoint.y);
        }

        // Given a point in world-space, return a camera-relative view coordinate. 
        float3 world_to_view_coord(const float3 & world_coord) const
        {
            return transform_coord(get_view_matrix(), world_coord);
        }

        // Given a 2D point in normalized device coordinates [-1, +1], return a screen space coordinate. 
        int2 ndc_to_screen_coord(const float2 & ndc_coord, const float2 & viewport_size) const
        {
            // @todo - + viewport x + y
            int2 screen_coord;
            screen_coord.x = round_to_int(((ndc_coord.x + 1.0f) * 0.5f) * viewport_size.x);
            screen_coord.y = round_to_int((1.0f - (ndc_coord.y + 1.0f) * 0.5f) * viewport_size.y);
            return screen_coord;
        }

        // Given a 3D point in view coordinates, return a 2D point in screen space. 
        int2 view_to_screen_coord(const float3 & coord, const float2 & viewport_size) const
        {
            const float aspect_ratio = viewport_size.x / viewport_size.y;
            const float2 ndc_coord = view_to_ndc_coord(coord, aspect_ratio);
            return ndc_to_screen_coord(ndc_coord, viewport_size);
        }

        // Given a 3D point in world coordinates, return a 2D point in NDC space. 
        float2 world_to_ndc_point(const float3 & world_coord, const float aspect_ratio) const
        {
            float3 view_coord = world_to_view_coord(world_coord);
            return view_to_ndc_coord(view_coord, aspect_ratio);
        }

        // Given a 3D point in world coordinates, return a 2D point in screen space. 
        int2 world_to_screen(const float3 & world_coord, const float2 & viewport_size) const
        {
            const float aspect_ratio = viewport_size.x / viewport_size.y;
            const float2 ndc_coord = world_to_ndc_point(world_coord, aspect_ratio);
            return ndc_to_screen_coord(ndc_coord, viewport_size);
        }
    };

} // end namespace polymer

#endif // end polymer_camera_hpp

#pragma once

#ifndef polymer_camera_hpp
#define polymer_camera_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/util/util.hpp"
#include "polymer-core/tools/geometry.hpp"
#include <numeric>

namespace polymer
{

    /*
     * Nomenclature decoder ring:  
     * -> local space      [-∞, ∞]    - sometimes called model space
     * -> world space      [-∞, ∞]    - via model_matrix
     * -> camera space     [-∞, ∞]    - via view_matrix (sometimes called eye or view space) 
     * -> clip space       [-w, w]    - (generally only on GPU)
     * -> ndc space        [-1, 1]    - via projection matrix
     * -> projection space [0, 1]     - via perspective divide (generally only on GPU)
     * -> screen space     [0, size]  - sometimes called device space
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
            float4 projected_point = (get_projection_matrix(aspect_ratio) * get_view_matrix()) * float4(point.x, point.y, point.z, 1.f);

            if (std::abs(projected_point.w) > float(1e-7))
            {
                // normalize if w is different than 1
                const float inv_w = 1.0f / projected_point.w;
                projected_point.x *= inv_w;
                projected_point.y *= inv_w;
                projected_point.z *= inv_w;
            }
            else
            {
                projected_point.x = 0.0f;
                projected_point.y = 0.0f;
                projected_point.z = 0.0f;
            }

            return float3(projected_point.x, projected_point.y, projected_point.z);
        }

        // Deprojects a point in NDC to (eye/view/camera) space.
        // point.z should be to be in view space. If it is ndc, then an the inverse projection
        // is sufficient and this function is not required. 
        float3 deproject_point(const float3 & point, const float aspect_ratio) const
        {
            // Get world position for a point near the far plane
            float4 far_point(point.x(), point.y(), 0.98f, 1.0f);
            far_point = inverse(get_projection_matrix(aspect_ratio)) * far_point;

            if (std::abs(far_point.w) > float(1e-7))
            {
                // Perspective divide
                const float inv_w = 1.0f / far_point.w;

                float3 far_point_3d;
                far_point_3d.x   = far_point.x * inv_w;
                far_point_3d.y   = far_point.y * inv_w;
                far_point_3d.z   = far_point.z * inv_w;
                
                // Find the distance to the far point along the camera's viewing axis
                const float distance_along_z = dot(far_point_3d, get_view_direction());

                // check not behind the camera 
                if (distance_along_z >= 0.0f)
                {
                    // Direction from origin to our point
                    float3 dir = far_point_3d;  // Camera is at (0, 0, 0) so it's the same vector

                    // view space depth (point.z) is distance along the camera's view direction. Since our direction
                    // vector is not parallel to the viewing axis, instead of normalizing it with its own length, we
                    // instead normalize it with the length projected along the view dir. 
                    dir /= distance_along_z;

                    // find the final position along the direction
                    return dir * point.z;
                }
            }

            return float3(0.0f, 0.0f, 0.0f);
        }

        // Given a coordinate in view space, return a 2D point in NDC coordinates.  
        float2 view_to_ndc_coord(const float3 & view_coord, const float aspect_ratio) const
        {
            float3 projPoint = project_point(view_coord, aspect_ratio);
            return float2(projPoint.x, projPoint.y);
        }

        // Given a point in world space, return a view-relative coordinate. 
        float3 world_to_view_coord(const float3 & world_coord) const
        {
            return transform_coord(get_view_matrix(), world_coord);
        }

        // Given a 2D point in normalized device coordinates [-1, +1], return a 2D screen space coordinate. 
        int2 ndc_to_screen_coord(const float2 & ndc_point, const float2 & viewport_position, const float2 & viewport_size) const
        {
            int2 screen_coord;
            screen_coord.x = round_to_int(viewport_position.x + ((ndc_point.x + 1.0f) * 0.5f) * viewport_size.x);
            screen_coord.y = round_to_int(viewport_position.y + (1.0f - (ndc_point.y + 1.0f) * 0.5f) * viewport_size.y);
            return screen_coord;
        }

        // Given a 3D point in view space, return a 2D point in screen space. 
        int2 view_to_screen_coord(const float3 & view_point, const float2 & viewport_position, const float2 & viewport_size) const
        {
            const float aspect_ratio = viewport_size.x / viewport_size.y;
            const float2 ndc_coord   = view_to_ndc_coord(view_point, aspect_ratio);
            return ndc_to_screen_coord(ndc_coord, viewport_position, viewport_size);
        }

        // Given a 3D point in world space, return a 2D point in NDC space. 
        float2 world_to_ndc_point(const float3 & world_point, const float aspect_ratio) const
        {
            float3 view_coord = world_to_view_coord(world_point);
            return view_to_ndc_coord(view_coord, aspect_ratio);
        }

        // Given a 3D point in world space, return a 2D point in screen space. 
        int2 world_to_screen(const float3 & world_point, const float2 & viewport_position, const float2 & viewport_size) const
        {
            const float aspect_ratio = viewport_size.x / viewport_size.y;
            const float2 ndc_coord   = world_to_ndc_point(world_point, aspect_ratio);
            return ndc_to_screen_coord(ndc_coord, viewport_size, viewport_position);
        }

        // Given a 2D point in ndc space, return a 3D point in view space according to the given depth
        float3 ndc_to_view_point(const float2 & ndc_point, float depth, const float aspect_ratio) const
        {
            return deproject_point(float3(ndc_point.x, ndc_point.y, depth), aspect_ratio);
        }
    };

    struct screen_raycaster
    {
        perspective_camera & cam;
        float2 viewport;
        screen_raycaster(perspective_camera & camera, const float2 viewport) : cam(camera), viewport(viewport) {}
        ray from(const float2 & cursor) const { return cam.get_world_ray(cursor, viewport); };
    };

} // end namespace polymer

#endif // end polymer_camera_hpp

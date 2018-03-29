/*
 * File: math-projection.hpp
 * This header aggregates functions for constructing common projection matrices,
 * along with the extraction and computation of attributes on existing
 * projection matrices (like field of view, focal length, near/far clip, etc). 
 */

#pragma once 

#ifndef math_projection_hpp
#define math_projection_hpp

#include "math-common.hpp"
#include <ratio>
#include <assert.h>

namespace polymer
{
    struct FieldOfView
    {
        float left, right, bottom, top, near, far;
    };

    inline float4x4 make_projection_matrix(float l, float r, float b, float t, float n, float f)
    {
        return{ { 2 * n / (r - l),0,0,0 },{ 0,2 * n / (t - b),0,0 },{ (r + l) / (r - l),(t + b) / (t - b),-(f + n) / (f - n),-1 },{ 0,0,-2 * f*n / (f - n),0 } };
    }

    inline float4x4 make_projection_matrix(float vFovInRadians, float aspectRatio, float nearZ, float farZ)
    {
        const float top = nearZ * std::tan(vFovInRadians / 2.f), right = top * aspectRatio;
        return make_projection_matrix(-right, right, -top, top, nearZ, farZ);
    }

    inline float4x4 make_orthographic_matrix(float l, float r, float b, float t, float n, float f)
    {
        return{ { 2 / (r - l),0,0,0 },{ 0,2 / (t - b),0,0 },{ 0,0,-2 / (f - n),0 },{ -(r + l) / (r - l),-(t + b) / (t - b),-(f + n) / (f - n),1 } };
    }

    // Based on http://aras-p.info/texts/obliqueortho.html (http://www.terathon.com/lengyel/Lengyel-Oblique.pdf)
    // This is valid for both perspective and orthographic projections. `clip_plane` is defined in camera space.
    inline void calculate_oblique_matrix(float4x4 & projection, const float4 & clip_plane)
    {
        const float4 q = mul(inverse(projection), float4(sign(clip_plane.x), sign(clip_plane.y), 1.f, 1.f));
        const float4 c = clip_plane * (2.f / (dot(clip_plane, q)));
        projection[0][2] = c.x - projection[0][3];
        projection[1][2] = c.y - projection[1][3];
        projection[2][2] = c.z - projection[2][3];
        projection[3][2] = c.w - projection[3][3];
    }

    inline void get_tanspace_fov(const float4x4 & projection, FieldOfView & fov)
    {
        fov.near = projection[3][2] / (projection[2][2] - 1.0f);
        fov.far = projection[3][2] / (1.0f + projection[2][2]);

        fov.left = fov.near * (projection[2][0] - 1.0f) / projection[0][0];
        fov.right = fov.near * (1.0f + projection[2][0]) / projection[0][0];

        fov.bottom = fov.near * (projection[2][1] - 1.0f) / projection[1][1];
        fov.top = fov.near * (1.0f + projection[2][1]) / projection[1][1];
    }

    inline float vfov_from_projection(const float4x4 & projection)
    {
        return std::atan((1.0f / projection[1][1])) * 2.0f;
    }

    inline float aspect_from_projection(const float4x4 & projection)
    {
        return 1.0f / (projection[0][0] * (1.0f / projection[1][1]));
    }

    inline void near_far_clip_from_projection(const float4x4 & projection, float & near, float & far)
    {
        near = projection[3][2] / (projection[2][2] - 1.0f);
        far = projection[3][2] / (1.0f + projection[2][2]);
    }

    inline float get_focal_length(float vFoV)
    {
        return (1.f / (tan(vFoV * 0.5f) * 2.0f));
    }

    inline float get_focal_length_pixels(const int widthPixels, float vFoV)
    {
        auto f = widthPixels / 2 / std::tan(vFoV * 0.5f);
    }

    inline float dfov_to_vfov(float dFoV, float aspectRatio)
    {
        return 2.f * atan(tan(dFoV / 2.f) / sqrt(1.f + aspectRatio * aspectRatio));
    }

    inline float dfov_to_hfov(float dFoV, float aspectRatio)
    {
        return 2.f * atan(tan(dFoV / 2.f) / sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    inline float vfov_to_dfov(float vFoV, float aspectRatio)
    {
        return 2.f * atan(tan(vFoV / 2.f) * sqrt(1.f + aspectRatio * aspectRatio));
    }

    inline float hfov_to_dfov(float hFoV, float aspectRatio)
    {
        return 2.f * atan(tan(hFoV / 2.f) * sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    inline float hfov_to_vfov(float hFoV, float aspectRatio)
    {
        return 2.f * atan(tan(hFoV / 2.f) / aspectRatio);
    }

    // https://computergraphics.stackexchange.com/questions/1736/vr-and-frustum-culling
    inline void compute_center_view(const float4x4 & leftProjection, const float4x4 & rightProjection, const float interCameraDistance, float4x4 & outProjection, float3 & outTranslation)
    {
        FieldOfView leftFov = {};
        FieldOfView rightFov = {};
        get_tanspace_fov(leftProjection, leftFov);
        get_tanspace_fov(rightProjection, rightFov);

        // In the case of VR SDKs which provide asymmetric frusta, get their extents
        const float tanHalfFovWidth = max(leftFov.left, leftFov.right, rightFov.left, rightFov.right);
        const float tanHalfFovHeight = max(leftFov.top, leftFov.bottom, rightFov.top, rightFov.bottom);

        // Double check that the near and far clip planes on both projections match
        float leftNearClip, leftFarClip;
        float rightNearClip, rightFarClip;
        near_far_clip_from_projection(leftProjection, leftNearClip, leftFarClip);
        near_far_clip_from_projection(rightProjection, rightNearClip, rightFarClip);
        assert(leftNearClip == rightNearClip && leftFarClip == rightFarClip);

        const float4x4 superfrustumProjection = make_projection_matrix(-tanHalfFovWidth, tanHalfFovWidth, -tanHalfFovHeight, tanHalfFovHeight, leftNearClip, leftFarClip);
        const float superfrustumAspect = tanHalfFovWidth / tanHalfFovHeight;
        const float superfrustumvFoV = vfov_from_projection(superfrustumProjection);

        // Follows the technique outlined by Cass Everitt here: https://www.facebook.com/photo.php?fbid=10154006919426632&set=a.46932936631.70217.703211631&type=1&theater
        const float Nc = (interCameraDistance * 0.5f) * superfrustumProjection[0][0];
        const float4x4 superfrustumProjectionFixed = make_projection_matrix(superfrustumvFoV, superfrustumAspect, leftNearClip + Nc, leftFarClip + Nc);

        outProjection = superfrustumProjectionFixed;
        outTranslation = float3(0, 0, Nc);
    }
}

#endif // end math_projection_hpp

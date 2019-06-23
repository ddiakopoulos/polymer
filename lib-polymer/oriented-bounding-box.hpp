#pragma once

#ifndef polymer_obb_hpp
#define polymer_obb_hpp

#include "math-core.hpp"

namespace polymer
{    
    struct oriented_bounding_box
    {
        float3 half_ext;
        float3 center;
        quatf orientation;

        oriented_bounding_box(float3 center, float3 halfExtents, float4 orientation) 
            : center(center), half_ext(halfExtents), orientation(orientation) { }

        float calc_radius() const { return length(half_ext); };

        bool is_inside(const float3 & point) const { /* todo */ return false; }

        bool intersects(const oriented_bounding_box & other)
        {
            // Early out using a sphere check
            float minCollisionDistance = other.calc_radius() + calc_radius();
            if (length2(other.center - center) > (minCollisionDistance * minCollisionDistance))
            {
                return false;
            }

            auto thisCorners = calculate_obb_corners(*this);
            auto otherCorners = calculate_obb_corners(other);

            auto thisAxes = calculate_orthogonal_axes(orientation);
            auto otherAxes = calculate_orthogonal_axes(other.orientation);

            std::vector<plane> thisPlanes = {
                plane(-thisAxes[0], thisCorners[0]),
                plane(-thisAxes[1], thisCorners[0]),
                plane(-thisAxes[2], thisCorners[0]),
                plane( thisAxes[0], thisCorners[7]),
                plane( thisAxes[1], thisCorners[7]),
                plane( thisAxes[2], thisCorners[7]) 
            };

            std::vector<plane> otherPlanes = {
                 plane(-otherAxes[0], otherCorners[0]),
                 plane(-otherAxes[1], otherCorners[0]),
                 plane(-otherAxes[2], otherCorners[0]),
                 plane( otherAxes[0], otherCorners[7]),
                 plane( otherAxes[1], otherCorners[7]),
                 plane( otherAxes[2], otherCorners[7]) 
            };

            // Corners of box1 vs faces of box2
            for (int i = 0; i < 6; i++)
            {
                bool allPositive = true;
                for (int j = 0; j < 8; j++)
                {
                    if (otherPlanes[i].is_negative_half_space(thisCorners[j]))
                    {
                        allPositive = false;
                        break;
                    }
                }

                if (allPositive)
                {
                    return false; // We found a separating axis so there's no collision
                }
            }

            // Corners of box2 vs faces of box1
            for (int i = 0; i < 6; i++)
            {
                bool allPositive = true;
                for (int j = 0; j < 8; j++)
                {
                    // A negative projection has been found, no need to keep checking.
                    // It does not mean that there is a collision though.
                    if (thisPlanes[i].is_negative_half_space(otherCorners[j]))
                    {
                        allPositive = false;
                        break;
                    }
                }

                // We found a separating axis so there's no collision
                if (allPositive)
                {
                    return false;
                }
            }

            // No separating axis has been found = collision
            return true;
        }

        inline const std::vector<float3> calculate_obb_corners(const oriented_bounding_box & obb)
        {
            std::vector<float3> corners;
            const std::vector<float3> orthogonalAxes = calculate_orthogonal_axes(obb.orientation);
            corners[0] = center - orthogonalAxes[0] * half_ext.x - orthogonalAxes[1] * half_ext.y - orthogonalAxes[2] * half_ext.z;
            corners[1] = center + orthogonalAxes[0] * half_ext.x - orthogonalAxes[1] * half_ext.y - orthogonalAxes[2] * half_ext.z;
            corners[2] = center + orthogonalAxes[0] * half_ext.x + orthogonalAxes[1] * half_ext.y - orthogonalAxes[2] * half_ext.z;
            corners[3] = center - orthogonalAxes[0] * half_ext.x + orthogonalAxes[1] * half_ext.y - orthogonalAxes[2] * half_ext.z;
            corners[4] = center - orthogonalAxes[0] * half_ext.x + orthogonalAxes[1] * half_ext.y + orthogonalAxes[2] * half_ext.z;
            corners[5] = center - orthogonalAxes[0] * half_ext.x - orthogonalAxes[1] * half_ext.y + orthogonalAxes[2] * half_ext.z;
            corners[6] = center + orthogonalAxes[0] * half_ext.x - orthogonalAxes[1] * half_ext.y + orthogonalAxes[2] * half_ext.z;
            corners[7] = center + orthogonalAxes[0] * half_ext.x + orthogonalAxes[1] * half_ext.y + orthogonalAxes[2] * half_ext.z;
            return corners;
        }

        inline const std::vector<float3> calculate_orthogonal_axes(const quatf & orientation)
        {
            std::vector<float3> axes;
            axes[0] = qxdir(orientation);
            axes[1] = qydir(orientation);
            axes[2] = qzdir(orientation);
            return axes;
        }

    };
}

#endif // polymer_obb_hpp

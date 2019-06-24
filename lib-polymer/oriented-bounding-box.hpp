#pragma once

#ifndef polymer_obb_hpp
#define polymer_obb_hpp

#include "math-core.hpp"

namespace polymer
{    
    // PCA implementation kindly borrowed from Stan Melax' geometric.h utilities
    // MIT License https://github.com/melax/sandbox/blob/master/include/geometric.h

    /*
    namespace pca_impl
    {
        inline float3 diagonal_3x3(const float3x3 & m)
        {
            return{ m.x.x, m.y.y, m.z.z };
        }

        // returns angle that rotates m into diagonal matrix d where d01==d10==0 and d00>d11 (the eigenvalues)
        inline float make_diagonalized_matrix(const float2x2 & m)
        {
            float d = m.y.y - m.x.x;
            return atan2f(d + sqrtf(d*d + 4.f*m.x.y*m.y.x), 2.f * m.x.y);
        }

        // Input must be a symmetric matrix.
        // returns orientation of the principle axes.
        // returns quaternion q such that its corresponding column major matrix Q 
        // Diagonal matrix D = transpose(Q) * A * (Q); thus  A == Q*D*QT
        // The directions of q (cols of Q) are the eigenvectors D's diagonal is the eigenvalues
        // As per 'col' convention if float3x3 Q = qgetmatrix(q); then Q*v = q*v*conj(q)
        inline float4 make_diagonalized_matrix(const float3x3 & A)
        {
            int maxsteps = 24;  // certainly wont need that many.
            int i;
            quatf q(0, 0, 0, 1);
            for (i = 0; i < maxsteps; ++i)
            {
                float3x3 Q = qmat(q); // Q*v == q*v*conj(q)
                float3x3 D = mul(transpose(Q), A, Q);  // A = Q*D*Q^T
                float3 offdiag(D[1][2], D[0][2], D[0][1]); // elements not on the diagonal
                float3 om(fabsf(offdiag.x), fabsf(offdiag.y), fabsf(offdiag.z)); // mag of each offdiag elem
                int k = (om.x > om.y&&om.x > om.z) ? 0 : (om.y > om.z) ? 1 : 2; // index of largest element of offdiag
                int k1 = (k + 1) % 3;
                int k2 = (k + 2) % 3;
                if (offdiag[k] == 0.0f) break;  // diagonal already
                float thet = (D[k2][k2] - D[k1][k1]) / (2.0f*offdiag[k]);
                float sgn = (thet > 0.0f) ? 1.0f : -1.0f;
                thet *= sgn; // make it positive
                float t = sgn / (thet + ((thet < 1.E6f) ? sqrtf(thet*thet + 1.0f) : thet)); // sign(T)/(|T|+sqrt(T^2+1))
                float c = 1.0f / sqrtf(t*t + 1.0f); //  c= 1/(t^2+1) , t=s/c 
                if (c == 1.0f) break;  // no room for improvement - reached machine precision.
                quatf jr(0, 0, 0, 0); // jacobi rotation for this iteration.
                jr[k] = sgn * sqrtf((1.0f - c) / 2.0f);  // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)  
                jr[k] *= -1.0f; // note we want a final result semantic that takes D to A, not A to D
                jr.w = sqrtf(1.0f - (jr[k] * jr[k]));
                if (jr.w == 1.0f) break; // reached limits of floating point precision
                q = qmul(q, jr);
                q = normalize(q);
            }
            float h = 1.f / sqrtf(2.f);  // M_SQRT2
            auto e = [&q, &A]() {return diagonal_3x3(mul(transpose(qmat(q)), A, qmat(q))); };  // current ordering of eigenvals of q
            q =  (e().x < e().z) ? qmul(q, float4(0, h, 0, h)) : q;
            q =  (e().y < e().z) ? qmul(q, float4(h, 0, 0, h)) : q;
            q =  (e().x < e().y) ? qmul(q, float4(0, 0, h, h)) : q; // size order z,y,x so xy spans a planeish spread
            q = (qzdir(q).z < 0) ? qmul(q, float4(1, 0, 0, 0)) : q;
            q = (qydir(q).y < 0) ? qmul(q, float4(0, 0, 1, 0)) : q;
            q = (q.w < 0) ? -q : q;
            auto M = transpose(qmat(q)) * A * qmat(q);   // to test result
            return q;
        }
    }

    // Returns principal axes as a pose and population's variance along pose's local x,y,z
    inline std::pair<transform, float3> make_principal_axes(const std::vector<float3> & points)
    {
        if (points.size() == 0) throw std::invalid_argument("not enough points for PCA");
        float3 com;
        float3x3 cov;
        for (const auto & p : points) com += p;
        com /= (float)points.size();
        for (const auto & p : points)cov += outerprod(p - com, p - com);
        cov /= (float)points.size();
        auto q = pca_impl::Diagonalizer(cov);
        return std::make_pair<transform, float3>({ com, q }, pca_impl::diagonal_3x3(transpose(qmat(q) * cov * qmat(q)));
    }
    */

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

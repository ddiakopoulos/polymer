#ifndef parallel_transport_frames_hpp
#define parallel_transport_frames_hpp

#include "util.hpp"
#include "math-common.hpp"
#include "splines.hpp"

// good ref: http://sunandblackcat.com/tipFullView.php?l=eng&topicid=4

// Compute a set of reference frames defined by their transformation matrix along a 
// curve. It is designed so that the array of points and the array of matrices used 
// to fetch these routines don't need to be ordered as the curve. e.g.
//
//     m[0] = first_frame(p[0], p[1], p[2]);
//     for(int i = 1; i < n - 1; i++)
//     {
//         m[i] = next_frame(m[i-1], p[i-1], p[i], t[i-1], t[i]);
//     }
//     m[n-1] = last_frame(m[n-2], p[n-2], p[n-1]);
//
//   See Game Programming Gems 2, Section 2.5

namespace polymer
{
    inline std::vector<float4x4> make_parallel_transport_frame_bezier(const std::array<transform, 4> controlPoints, const int segments)
    {
        bezier_spline curve(controlPoints[0].position, controlPoints[1].position, controlPoints[2].position, controlPoints[3].position);

        std::vector<float3> points;   // Points in spline
        std::vector<float3> tangents; // Tangents in spline (fwd dir)

        // Build the spline.
        float dt = 1.0f / float(segments);
        for (int i = 0; i < segments; ++i)
        {
            const float t = float(i) * dt;
            points.push_back(curve.point(t));
            tangents.push_back(normalize(curve.derivative(t)));
        }

        auto n = points.size();
        std::vector<float4x4> frames(n); // Coordinate frame at each spline sample

        // Require at least 3 points to start
        if (n >= 3)
        {
            // First frame, expressed in a Y-up, right-handed coordinate system
            const float3 B = normalize(points[1] - points[0]);      // bitangent
            const float3 T = normalize(cross({ 0, 1, 0 }, B));      // tangent
            const float3 N = cross(B, N);                           // normal
            frames[0] = { float4(-T, 0), float4(N, 0), float4(B, 0), float4(points[0], 1) };

            // This sets the transformation matrix to the last reference frame defined by the previously computed 
            // transformation matrix and the last point along the curve.
            for (int i = 1; i < n - 1; ++i)
            {
                float3 axis;
                float r = 0.f;

                if ((length2(tangents[i - 1]) != 0.0f) && (length2(tangents[i]) != 0.0f))
                {
                    const float3 prevTangent = (tangents[i - 1]);
                    const float3 curTangent = (tangents[i]);

                    float dp = dot(prevTangent, curTangent);

                    if (dp > 1) dp = 1;
                    else if (dp < -1) dp = -1;

                    r = std::acos(dp);
                    axis = cross(prevTangent, curTangent);
                }

                if ((length(axis) != 0.0f) && (r != 0.0f))
                {
                    const float4x4 R = make_rotation_matrix(normalize(axis), r);      // note the normalized axis
                    const float4x4 Tj = make_translation_matrix(points[i]);           // current point
                    const float4x4 Ti = make_translation_matrix(-points[i - 1]);      // previous point
                    frames[i] = mul(Tj, mul(R, mul(Ti, frames[i - 1])));
                }
                else
                {
                    const float4x4 t = make_translation_matrix(points[i] - points[i - 1]);
                    frames[i] = mul(t, frames[i - 1]);
                }
            }

            // Last frame
            frames[n - 1] = mul(make_translation_matrix(points[n - 1] - points[n - 2]), frames[n - 2]);
        }

        return frames;
    }

} // end namespace polymer

#endif // end parallel_transport_frames_hpp
#pragma once

#ifndef polymer_splines_hpp
#define polymer_splines_hpp

#include "polymer-core/math/math-core.hpp"

namespace polymer
{
 
    // A cubic spline as a piecewise curve with a continuous second derivative.
    // https://www.math.ucla.edu/~baker/149.1.02w/handouts/dd_splines.pdf
    class cubic_bezier
    {
        float3 p0, p1, p2, p3;
        std::vector<float> arcLengths;
        
    public:
        
        size_t num_steps {64};
    
        cubic_bezier() {};

        cubic_bezier(const float3 & p0, const float3 & p1, const float3 & p2, const float3 & p3, const size_t num_steps = 64) 
            : num_steps(num_steps)
        {
            set_control_points(p0, p1, p2, p3);
        }
        
        // anchor, handle, handle, anchor
        void set_control_points(const float3 & p0, const float3 & p1, const float3 & p2, const float3 & p3)
        {
            this->p0 = p0;
            this->p1 = p1;
            this->p2 = p2;
            this->p3 = p3;

            arcLengths.resize(num_steps);
            arcLengths[0] = 0.f;
            float3 start = p0;
            for (size_t i = 1; i < num_steps; i++)
            {
                float t = float(i) / float(num_steps - 1);
                float3 end = evaluate(t);
                float length = distance(start, end);
                arcLengths[i] = arcLengths[i - 1] + length;
                start = end;
            }
        }

        std::array<float3, 4> get_control_points() const 
        {
            return {p0, p1, p2, p3};
        }
        
        float3 evaluate(const float t) const
        {
            float t2 = t * t;
            float t3 = t2 * t;
            float tt1 = 1.0f - t;
            float tt2 = tt1 * tt1;
            float tt3 = tt2 * tt1;
            return (tt3 * p0) + (3.0f * t * tt2 * p1) + (3.0f * tt1 * t2 * p2) + (t3 * p3);
        }
        
        float3 derivative(const float t) const
        {
            float t2 = t * t;
            float tt1 = 1.0f - t;
            float tt2 = tt1 * tt1;
            return (-3.0f * tt2 * p0) + ((3.0f * tt2 - 6.0f * t * tt1) * p1) + ((6.0f * t * tt1 - 3.0f * t2) * p2) + (3.0f * t2 * p3);
        }
        
        float3 derivative2(const float t) const
        {
            return 6.0f * (1.0f - t) * (p2 - 2.0f * p1 + p0) + 6.0f * t * (p3 - 2.0f * p2 + p1);
        }
        
        float curvature(const float t) const
        {
            float3 deriv = derivative(t);
            float3 deriv2 = derivative2(t);
            return linalg::length(cross(deriv, deriv2)) / powf(linalg::length(deriv), 3.0f);
        }
        
        float max_curvature() const
        {
            float max = std::numeric_limits<float>::min();
            for (size_t i = 0; i < num_steps; i++)
            {
                float t = float(i) / float(num_steps - 1);
                float c = curvature(t);
                if (c > max) max = c;
            }
            
            return max;
        }
        
        float length() const
        {
            return arcLengths[arcLengths.size() - 1];
        }
        
        float get_length_parameter(const float t)
        {
            float targetLength = t * arcLengths[arcLengths.size() - 1];
            
            // Find largest arc length that is less than the target length
            size_t first = 0;
            size_t last = arcLengths.size() - 1;
            while (first <= last)
            {
                auto mid = (first + last) / 2;
                if (arcLengths[mid] > targetLength) last = mid - 1;
                else first = mid + 1;
            }
            
            size_t index = first - 1;
            if (arcLengths[index] == targetLength)
            {
                return index / float(arcLengths.size() - 1); // No need to interpolate
            }
            
            // Begin interpolation
            float start = arcLengths[index];
            float end = arcLengths[index + 1];
            float length = end - start;
            float fraction = (targetLength - start) / length;
            return (index + fraction) / float(arcLengths.size() - 1);
        }

        // Convert the parameteric control points into polynomial coefficients
        float4 get_cubic_coefficients(const int dimension)
        {
            float a = -p0[dimension] + 3.0f * p1[dimension] - 3.0f * p2[dimension] + p3[dimension];
            float b =  3.0f * p0[dimension] - 6.0f * p1[dimension] + 3.0f * p2[dimension];
            float c = -3.0f * p0[dimension] + 3.0f * p1[dimension];
            float d = p0[dimension];
            return { a, b, c, d };
        }
        
    };
    
}

#endif // polymer_splines_hpp

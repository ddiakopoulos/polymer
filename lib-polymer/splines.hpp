#pragma once

#ifndef polymer_splines_hpp
#define polymer_splines_hpp

#include "math-core.hpp"

namespace polymer
{
      
    class bezier_spline
    {
        
        void calculate_length()
        {
            arcLengths.resize(num_steps);
            arcLengths[0] = 0.0f;
            float3 start = p0;
            for (size_t i = 1; i < num_steps; i++)
            {
                float t = float(i) / float(num_steps - 1);
                float3 end = point(t);
                float length = distance(start, end);
                arcLengths[i] = arcLengths[i - 1] + length;
                start = end;
            }
        }
        
        float3 p0, p1, p2, p3;
        std::vector<float> arcLengths;
        
    public:
        
        size_t num_steps;

        bezier_spline(float3 p0, float3 p1, float3 p2, float3 p3, const size_t num_steps = 32) : num_steps(num_steps)
        {
            set_control_points(p0, p1, p2, p3);
        }
        
        void set_control_points(float3 p0, float3 p1, float3 p2, float3 p3)
        {
            this->p0 = p0;
            this->p1 = p1;
            this->p2 = p2;
            this->p3 = p3;
            calculate_length();
        }
        
        float3 point(const float t) const
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
        
        float get_length_parameter(float t)
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
        
    };
    
}

#endif // polymer_splines_hpp

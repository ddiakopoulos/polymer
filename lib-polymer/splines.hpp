// See COPYING file for attribution information

#pragma once

#ifndef constant_spline_h
#define constant_spline_h

#include "math-core.hpp"

namespace polymer
{
    
    struct SplinePoint
    {
        float3 point;
        float distance = 0.0f;
        float ac = 0.0f;
        SplinePoint(){};
        SplinePoint(float3 p, float d, float ac) : point(p), distance(d), ac(ac) {}
    };
    
    // This object creates a B-Spline using 4 points, and a number steps
    // or a fixed step distance can be specified to create a set of points that cover the curve at constant rate.
    class ConstantSpline
    {
        std::vector<SplinePoint> points;
        std::vector<SplinePoint> lPoints;
        
    public:
        
        float3 p0, p1, p2, p3;
        
        float d = 0.0f;
        
        ConstantSpline() { };
        
        void calculate(float increment = 0.01f)
        {
            d = 0.0f;
            points.clear();
            
            float3 tmp, result;
            for (float j = 0; j <= 1.0f; j += increment)
            {
                float i = (1 - j);
                float ii = i * i;
                float iii = ii * i;
                
                float jj = j * j;
                float jjj = jj * j;
                
                result = float3(0, 0, 0);
                
                tmp = p0;
                tmp *= iii;
                result += tmp;
                
                tmp = p1;
                tmp *= 3 * j * ii;
                result += tmp;
                
                tmp = p2;
                tmp *= 3 * jj * i;
                result += tmp;
                
                tmp = p3;
                tmp *= jjj;
                result += tmp;
                
                points.emplace_back(result, 0.0f, 0.0f);
            }
            
            points.emplace_back(p3, 0.0f, 0.0f);
        }
        
        void calculate_distances()
        {
            d = 0.0f;
            
            SplinePoint to;
            SplinePoint from;
            float td = 0.0f;
            
            for (size_t j = 0; j < points.size() - 1; j++)
            {
                points[j].distance = td;
                points[j].ac = d;
                
                from = points[j];
                to = points[j + 1];
                td = distance(to.point, from.point);
                
                d += td;
            }
            
            points[points.size() - 1].distance = 0;
            points[points.size() - 1].ac = d;
        }
        
        float split_segment(const float distancePerStep, SplinePoint & a, const SplinePoint & b, std::vector<SplinePoint> & l)
        {
            SplinePoint t = b;
            float distance = 0.0f;
            
            t.point -= a.point;
            
            auto rd = length(t.point);
            t.point = safe_normalize(t.point);
            t.point *= distancePerStep;
            
            auto s = std::floor(rd / distancePerStep);
            
            for (size_t i = 0; i < s; ++i)
            {
                a.point += t.point;
                l.push_back(a);
                distance += distancePerStep;
            }
            return distance;
        }
        
        // In Will Wright's own words:
        //  "Construct network based functions that are defined by divisible intervals
        //   while approximating said network and composing it of pieces of simple functions defined on
        //   subintervals and joined at their endpoints with a suitable degree of smoothness."
        void reticulate(uint32_t steps)
        {
            float distancePerStep = d / float(steps);
            
            lPoints.clear();
            
            float localD = 0.f;
            
            // First point
            SplinePoint current = points[0];
            lPoints.push_back(current);
            
            // Reticulate
            for (size_t i = 0; i < points.size(); ++i)
            {
                if (points[i].ac - localD > distancePerStep)
                {
                    localD += split_segment(distancePerStep, current, points[i], lPoints);
                }
            }
            
            // Last point
            lPoints.push_back(points[points.size() - 1]);
        }
        
        std::vector<float3> get_spline()
        {
            std::vector<float3> spline;
            for (auto & p : lPoints) { spline.push_back(p.point); };
            return spline;
        }
        
    };
    
    
    class BezierCurve
    {
        
        void calculate_length()
        {
            arcLengths.resize(num_steps());
            arcLengths[0] = 0.0f;
            float3 start = p0;
            for (int i = 1; i < num_steps(); i++)
            {
                float t = float(i) / float(num_steps() - 1);
                float3 end = point(t);
                float length = distance(start, end);
                arcLengths[i] = arcLengths[i - 1] + length;
                start = end;
            }
        }
        
        float3 p0, p1, p2, p3;
        std::vector<float> arcLengths;
        
    public:
        
        BezierCurve(float3 p0, float3 p1, float3 p2, float3 p3)
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
        
        float num_steps() const
        {
            return 32;
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
            for (int i = 0; i < num_steps(); i++)
            {
                float t = float(i) / float(num_steps() - 1);
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
                return index / float(arcLengths.size() - 1); // No need to interpolate
            
            // Begin interpolation
            float start = arcLengths[index];
            float end = arcLengths[index + 1];
            float length = end - start;
            float fraction = (targetLength - start) / length;
            return (index + fraction) / float(arcLengths.size() - 1);
        }
        
    };
    
}

#endif // constant_spline_h

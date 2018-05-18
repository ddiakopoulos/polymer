/*
 * File: math-ray.hpp
 * This header file defines the `Ray` value type and a number of intersection
 * routines against geometric primitives (axis-aligned boxes, spheres, and triangles). 
 */

#pragma once

#ifndef math_ray_hpp
#define math_ray_hpp

#include "math-common.hpp"
#include "math-spatial.hpp"
#include "math-primitives.hpp"

namespace polymer
{

    /////////////
    //   Ray   //
    /////////////

    struct ray
    {
        float3 origin, direction;
        ray() = default;
        ray(const float3 & ori, const float3 & dir) : origin(ori), direction(dir) {}
        float3 inverse_direction() const { return{ 1.f / direction.x, 1.f / direction.y, 1.f / direction.z }; }
        float3 calculate_position(float t) const { return origin + direction * t; }
    };

    inline std::ostream & operator << (std::ostream & o, const ray & r)
    {
        return o << "{" << r.origin << " => " << r.direction << "}";
    }

    inline ray between(const float3 & start, const float3 & end)
    {
        return{ start, safe_normalize(end - start) };
    }

    inline ray ray_from_viewport_pixel(const float2 & pixelCoord, const float2 & viewportSize, const float4x4 & projectionMatrix)
    {
        float vx = pixelCoord.x * 2 / viewportSize.x - 1, vy = 1 - pixelCoord.y * 2 / viewportSize.y;
        auto invProj = inverse(projectionMatrix);
        return{ { 0,0,0 }, safe_normalize(transform_coord(invProj,{ vx, vy, +1 }) - transform_coord(invProj,{ vx, vy, -1 })) };
    }

    inline ray operator * (const transform & pose, const ray & ray)
    {
        return{ pose.transform_coord(ray.origin), pose.transform_vector(ray.direction) };
    }

    //////////////////////////////
    // Ray-object intersections //
    //////////////////////////////

    inline bool intersect_ray_plane(const ray & ray, const plane & p, float3 * intersection = nullptr, float * outT = nullptr)
    {
        const float d = dot(ray.direction, p.get_normal());

        // Make sure we're not parallel to the plane
        if (std::abs(d) > PLANE_EPSILON)
        {
            const float t = -p.distance_to(ray.origin) / d;

            if (t >= PLANE_EPSILON)
            {
                if (outT) *outT = t;
                if (intersection) *intersection = ray.origin + (t * ray.direction);
                return true;
            }
        }
        if (outT) *outT = std::numeric_limits<float>::max();
        return false;
    }

    // Real-Time Collision Detection pg. 180
    inline bool intersect_ray_box(const ray & ray, const float3 & min, const float3 & max, float * outTmin = nullptr, float * outTmax = nullptr, float3 * outNormal = nullptr)
    {
        float tmin = 0.f; // set to -FLT_MAX to get first hit on line
        float tmax = std::numeric_limits<float>::max(); // set to max distance ray can travel (for segment)
        float3 n;
        float3 normal(0, 0, 0);

        const float3 invDist = ray.inverse_direction();

        // For all three slabs
        for (int i = 0; i < 3; ++i)
        {
            if (std::abs(ray.direction[i]) < PLANE_EPSILON)
            {
                // Ray is parallel to slab. No hit if r.origin not within slab
                if ((ray.origin[i] < min[i]) || (ray.origin[i] > max[i])) return false;
            }
            else
            {
                // Compute intersection t value of ray with near and far plane of slab
                float t1 = (min[i] - ray.origin[i]) * invDist[i]; // near
                float t2 = (max[i] - ray.origin[i]) * invDist[i]; // far

                n.x = (i == 0) ? min[i] : 0.0f;
                n.y = (i == 1) ? min[i] : 0.0f;
                n.z = (i == 2) ? min[i] : 0.0f;

                // Swap t1 and t2 if need so that t1 is intersection with near plane and t2 with far plane
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                    n = -n;
                }

                // Compute the intersection of the of slab intersection interval with previous slabs
                if (t1 > tmin)
                {
                    tmin = t1;
                    normal = n;
                }
                tmax = std::min(tmax, t2);

                // If the slabs intersection is empty, there is no hit
                if (tmin > tmax || tmax <= PLANE_EPSILON) return false;
            }
        }
        if (outTmin) *outTmin = tmin;
        if (outTmax) *outTmax = tmax;
        if (outNormal) *outNormal = (tmin) ? normalize(normal) : float3(0, 0, 0);

        return true;
    }

    // Returns the closest point on the ray to the Sphere. If ray intersects then returns the point of nearest intersection.
    inline float3 intersect_ray_sphere(const ray & ray, const sphere & s)
    {
        float t;
        float3 diff = ray.origin - s.center;
        float a = dot(ray.direction, ray.direction);
        float b = 2.f * dot(diff, ray.direction);
        float c = dot(diff, diff) - s.radius * s.radius;
        float disc = b * b - 4.f * a * c;

        if (disc > 0)
        {
            float e = std::sqrt(disc);
            float denom = 2.f * a;
            t = (-b - e) / denom; // smaller root

            if (t > SPHERE_EPSILON) return ray.calculate_position(t);

            t = (-b + e) / denom; // larger root
            if (t > SPHERE_EPSILON) return ray.calculate_position(t);
        }

        // doesn't intersect; closest point on line
        t = dot(-diff, safe_normalize(ray.direction));
        float3 onRay = ray.calculate_position(t);
        return s.center + safe_normalize(onRay - s.center) * s.radius;
    }

    inline bool intersect_ray_sphere(const ray & ray, const sphere & sphere, float * outT = nullptr, float3 * outNormal = nullptr)
    {
        float t;
        const float3 diff = ray.origin - sphere.center;
        const float a = dot(ray.direction, ray.direction);
        const float b = 2.0f * dot(diff, ray.direction);
        const float c = dot(diff, diff) - sphere.radius * sphere.radius;
        const float disc = b * b - 4.0f * a * c;

        if (disc < 0.0f)
        {
            return false;
        }
        else
        {
            float e = std::sqrt(disc);
            float denom = 1.f / (2.0f * a);

            t = (-b - e) * denom;
            if (t > SPHERE_EPSILON)
            {
                if (outT) *outT = t;
                if (outNormal) *outNormal = t ? ((ray.direction * t - diff) / sphere.radius) : normalize(ray.direction * t - diff);
                return true;
            }

            t = (-b + e) * denom;
            if (t > SPHERE_EPSILON)
            {
                if (outT) *outT = t;
                if (outNormal) *outNormal = t ? ((ray.direction * t - diff) / sphere.radius) : normalize(ray.direction * t - diff);
                return true;
            }
        }
        if (outT) *outT = 0;
        if (outNormal) *outNormal = float3(0, 0, 0);
        return false;
    }

    // Implementation adapted from: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
    inline bool intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * outT = nullptr, float2 * outUV = nullptr)
    {
        float3 e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);

        float a = dot(e1, h);
        if (fabsf(a) == 0.0f) return false; // Ray is collinear with triangle plane

        float3 s = ray.origin - v0;
        float f = 1 / a;
        float u = f * dot(s, h);
        if (u < 0 || u > 1) return false; // Line intersection is outside bounds of triangle

        float3 q = cross(s, e1);
        float v = f * dot(ray.direction, q);
        if (v < 0 || u + v > 1) return false; // Line intersection is outside bounds of triangle

        float t = f * dot(e2, q);
        if (t < 0) return false; // Line intersection, but not a ray intersection

        if (outT) *outT = t;
        if (outUV) *outUV = { u,v };

        return true;
    }

}

#endif // end math_ray_hpp
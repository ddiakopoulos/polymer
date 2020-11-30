﻿/* 
 * File: math-spatial.hpp
 * This header file defines and implements data structures and algorithms 
 * related to the affine transformation of 3D objects in space. Most of this
 * codebase supports a right-handed, Y-up coordinate system, however some
 * general utilities for converting between arbitrary coordinate systems
 * are also provided. 
 */

#pragma once

#ifndef math_spatial_hpp
#define math_spatial_hpp

#include "math-common.hpp"

namespace polymer
{
    ///////////////////
    //   transform   //
    ///////////////////

    // Rigid transformation value-type
    struct transform
    {
        quatf     orientation;    // Orientation of an object, expressed as a rotation quaternion from the base orientation
        float3    position;       // Position of an object, expressed as a translation vector from the base position

        transform() : transform({ 0,0,0,1 }, { 0,0,0 }) {}
        transform(const quatf & orientation, const float3 & position) : orientation(orientation), position(position) {}
        explicit  transform(const quatf & orientation) : transform(orientation, { 0,0,0 }) {}
        explicit  transform(const float3 & position) : transform({ 0,0,0,1 }, position) {}
                  
        transform inverse() const { auto invOri = linalg::inverse(orientation); return { invOri, qrot(invOri, -position) }; }
        float4x4  matrix() const { return { { qxdir(orientation),0 },{ qydir(orientation),0 },{ qzdir(orientation),0 },{ position,1 } }; }
        float4x4  view_matrix() const { return inverse().matrix(); }
        float3    xdir() const { return qxdir(orientation); } // Equivalent to transform_vector({1,0,0})
        float3    ydir() const { return qydir(orientation); } // Equivalent to transform_vector({0,1,0})
        float3    zdir() const { return qzdir(orientation); } // Equivalent to transform_vector({0,0,1})
                  
        float3    transform_vector(const float3 & vec) const { return qrot(orientation, vec); }
        float3    transform_coord(const float3 & coord) const { return position + transform_vector(coord); }
        float3    detransform_coord(const float3 & coord) const { return detransform_vector(coord - position); }   // Equivalent to inverse().transform_coord(coord), but faster
        float3    detransform_vector(const float3 & vec) const { return qrot(linalg::inverse(orientation), vec); } // Equivalent to inverse().transform_vector(vec), but faster

        transform operator * (const transform & pose) const { return{ orientation * pose.orientation, transform_coord(pose.position) }; }
    };

    inline bool operator == (const transform & a, const transform & b) { return (a.position == b.position) && (a.orientation == b.orientation); }
    inline bool operator != (const transform & a, const transform & b) { return (a.position != b.position) || (a.orientation != b.orientation); }
    inline std::ostream & operator << (std::ostream & o, const transform & r) { return o << "{" << r.position << ", " << r.orientation << "}"; }

    //////////////////////////////////////////
    //   rotation quaternion construction   //
    //////////////////////////////////////////
    
    inline quatf make_rotation_quat_axis_angle(const float3 & axis, float angle)
    {
        return {axis * std::sin(angle/2), std::cos(angle/2)};
    }
    
    inline quatf make_rotation_quat_around_x(float angle)
    {
        return make_rotation_quat_axis_angle({1,0,0}, angle);
    }
    
    inline quatf make_rotation_quat_around_y(float angle)
    {
        return make_rotation_quat_axis_angle({0,1,0}, angle);
    }
    
    inline quatf make_rotation_quat_around_z(float angle)
    {
        return make_rotation_quat_axis_angle({0,0,1}, angle);
    }
    
    // http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors
    inline quatf make_rotation_quat_between_vectors(const float3 & from, const float3 & to)
    {
        auto a = safe_normalize(from), b = safe_normalize(to);
        return make_rotation_quat_axis_angle(safe_normalize(cross(a,b)), std::acos(dot(a,b)));
    }
    
    inline quatf make_rotation_quat_between_vectors_snapped(const float3 & from, const float3 & to, const float angle)
    {
        auto a = safe_normalize(from);
        auto b = safe_normalize(to);
        auto snappedAcos = std::floor(std::acos(dot(a,b)) / angle) * angle;
        return make_rotation_quat_axis_angle(safe_normalize(cross(a,b)), snappedAcos);
    }
    
    inline quatf make_rotation_quat_from_rotation_matrix(const float3x3 & m)
    {
        const float magw   = m[0][0] + m[1][1] + m[2][2];
        const bool wvsz    = magw  > m[2][2];
        const float magzw  = wvsz ? magw : m[2][2];
        const float3 prezw = wvsz ? float3(1,1,1)  : float3(-1,-1,1);
        const quatf postzw = wvsz ? quatf(0,0,0,1) : quatf(0,0,1,0);
        
        const bool xvsy    = m[0][0] > m[1][1];
        const float magxy  = xvsy ? m[0][0] : m[1][1];
        const float3 prexy = xvsy ? float3(1,-1,-1) : float3(-1,1,-1);
        const quatf postxy = xvsy ? quatf(1,0,0,0)  : quatf(0,1,0,0);
        
        const bool zwvsxy = magzw > magxy;
        const float3 pre  = zwvsxy ? prezw  : prexy;
        const quatf post  = zwvsxy ? postzw : postxy;
        
        const float t  = pre.x * m[0][0] + pre.y * m[1][1] + pre.z * m[2][2] + 1;
        const float s  = 1.f / sqrt(t) / 2.f;
        const quatf qp = quatf(pre.y * m[1][2] - pre.z * m[2][1], pre.z * m[2][0] - pre.x * m[0][2], pre.x * m[0][1] - pre.y * m[1][0], t) * s;
        return qp * post;
    }
    
    inline quatf make_rotation_quat_from_pose_matrix(const float4x4 & m)
    {
        return make_rotation_quat_from_rotation_matrix({m[0].xyz(),m[1].xyz(),m[2].xyz()});
    }
    
    inline float4 make_axis_angle_rotation_quat(const quatf & q)
    {
        float w = 2.0f * (float) acosf(std::max(-1.0f, std::min(q.w, 1.0f))); // angle
        float den = (float) sqrt(std::abs(1.0 - q.w * q.w));
        if (den > 1E-5f) return {q.x / den, q.y / den, q.z / den, w};
        return {1.0f, 0.f, 0.f, w};
    }
    
    /////////////////////////////
    //   quaternion utilities  //
    /////////////////////////////
    
    // Quaternion <=> Euler ref: http://www.swarthmore.edu/NatSci/mzucker1/e27/diebel2006attitude.pdf
    // ZYX is probably the most common standard: yaw, pitch, roll (YPR)
    // XYZ is somewhat less common: roll, pitch, yaw (RPY)

    inline float4 make_quat_from_euler_zyx(float y, float p, float r)
    {
        float4 q;
        q.x = cos(y/2.f)*cos(p/2.f)*cos(r/2.f) - sin(y/2.f)*sin(p/2.f)*sin(r/2.f);
        q.y = cos(y/2.f)*cos(p/2.f)*sin(r/2.f) + sin(y/2.f)*cos(r/2.f)*sin(p/2.f);
        q.z = cos(y/2.f)*cos(r/2.f)*sin(p/2.f) - sin(y/2.f)*cos(p/2.f)*sin(r/2.f);
        q.w = cos(y/2.f)*sin(p/2.f)*sin(r/2.f) + cos(p/2.f)*cos(r/2.f)*sin(y/2.f);
        return q;
    }

    inline float4 make_quat_from_euler_xyz(float r, float p, float y)
    {
        float4 q;
        q.x = cos(r/2.f)*cos(p/2.f)*cos(y/2.f) + sin(r/2.f)*sin(p/2.f)*sin(y/2.f);
        q.y = sin(r/2.f)*cos(p/2.f)*cos(y/2.f) - cos(r/2.f)*sin(y/2.f)*sin(p/2.f);
        q.z = cos(r/2.f)*cos(y/2.f)*sin(p/2.f) + sin(r/2.f)*cos(p/2.f)*sin(y/2.f);
        q.w = cos(r/2.f)*cos(p/2.f)*sin(y/2.f) - sin(p/2.f)*cos(y/2.f)*sin(r/2.f);
        return q;
    }

    inline float3 make_euler_from_quat_zyx(float4 q)
    {
        float3 ypr;
        const double q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
        ypr.x = float(atan2(-2.f*q1*q2 + 2 * q0*q3, q1*q1 + q0*q0 - q3*q3 - q2*q2));
        ypr.y = float(asin(+2.f*q1*q3 + 2.f*q0*q2));
        ypr.z = float(atan2(-2.f*q2*q3 + 2.f*q0*q1, q3*q3 - q2*q2 - q1*q1 + q0*q0));
        return ypr;
    }

    inline float3 make_euler_from_quat_xyz(float4 q)
    {
        float3 rpy;
        const double q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
        rpy.x = float(atan2(2.f*q2*q3 + 2.f*q0*q1, q3*q3 - q2*q2 - q1*q1 + q0*q0));
        rpy.y = float(-asin(2.f*q1*q3 - 2.f*q0*q2));
        rpy.z = float(atan2(2.f*q1*q2 + 2.f*q0*q3, q1*q1 + q0*q0 - q3*q3 - q2*q2));
        return rpy;
    }

    // Decompose rotation of q around the axis vt where q = swing * twist
    // Twist is a rotation about vt, and swing is a rotation about a vector perpindicular to vt
    // http://www.alinenormoyle.com/weblog/?p=726.
    // A singularity exists when swing is close to 180 degrees.
    inline void decompose_swing_twist(const quatf q, const float3 vt, quatf & swing, quatf & twist)
    {
        const float3 p = vt * dot(vt, twist.xyz());
        twist = safe_normalize(quatf(p.x, p.y, p.z, q.w));
        if (!twist.x && !twist.y && !twist.z && !twist.w) twist = quatf(0, 0, 0, 1); // singularity
        swing = q * conjugate(twist);
    }

    inline quatf interpolate_short(const quatf & a, const quatf & b, const float & t)
    {
        if (t <= 0) return a;
        if (t >= 0) return b;

        float fCos = dot(a, b);
        auto b2(b);

        if (fCos < 0)
        {
            b2 = -b;
            fCos = -fCos;
        }

        float k0, k1;
        if (fCos >(1.f - std::numeric_limits<float>::epsilon()))
        {
            k0 = 1.f - t;
            k1 = t;
        }
        else
        {
            const float s = std::sqrt(1.f - fCos * fCos), ang = std::atan2(s, fCos), oneOverS = 1.f / s;
            k0 = (std::sin(1.f - t) * ang) * oneOverS;
            k1 = std::sin(t * ang) * oneOverS;
        }

        return{ k0 * a.x + k1 * b2.x, k0 * a.y + k1 * b2.y, k0 * a.z + k1 * b2.z, k0 * a.w + k1 * b2.w };
    }

    // https://fgiesen.wordpress.com/2013/01/07/small-note-on-quaternion-distance-metrics/
    inline float compute_quat_closeness(const quatf & a, const quatf & b)
    {
        return std::acos((2.f * pow(dot(a, b), 2.f))  - 1.f);
    }

    // Returns an arbitrary unit-length vector orthogonal to v, ensuring non-colinearity.
    inline float3 orth(const float3 & v)
    {
        auto argmaxIdx = [](const float * a, int count)
        {
            if (count == 0) return static_cast<int>(-1);
            return static_cast<int>(std::max_element(a, a + count) - a);
        };

        float3 absv = abs(v);
        float3 u(1, 1, 1);
        u[argmaxIdx(&absv[0], 3)] = 0.0f;
        return normalize(cross(u, v));
    }

    // Shortest arc quat from Game Programming Gems 1 (Section 2.10)
    // Given two vectors, v0 and v1, this function returns a quat
    // where q * v0 = v1. v0 and v1 must be normalized, unit-length vectors.
    inline quatf make_quat_from_to(const float3 & v0, const float3 & v1)
    {
        const float3 c = cross(v0, v1);
        const float d = dot(v0, v1);
        if (d <= -1.0f) 
        { 
            const float3 a = orth(v0);
            return quatf(a.x, a.y, a.z, 0.0f); // 180 degrees around any orthogonal axis
        }
        const float s = std::sqrt((1.f + d) * 2.0f);
        return{ c.x / s, c.y / s, c.z / s, s / 2.0f };
    }

    // Spherical Spline Quaternion Interpolation
    // Reference: http://run.usc.edu/cs520-s13/assign2/p245-shoemake.pdf
    inline quatf squad(const quatf & a, const quatf & b, const quatf & c, const quatf & d, const float mu)
    {
        return slerp(slerp(a, d, mu), slerp(b, c, mu), 2 * (1 - mu) * mu);
    }

    inline float3 transform_vector(const quatf & quat, const float3 & v)
    {
        return (quat * quatf(v, 1)).xyz();
    }
    
    ////////////////////////////////////
    //   affine matrix construction   //
    ////////////////////////////////////

    inline float4x4 make_scaling_matrix(float scaling)
    {
        return {{scaling,0,0,0}, {0,scaling,0,0}, {0,0,scaling,0}, {0,0,0,1}};
    }
    
    inline float4x4 make_scaling_matrix(const float3 & scaling)
    {
        return {{scaling.x,0,0,0}, {0,scaling.y,0,0}, {0,0,scaling.z,0}, {0,0,0,1}};
    }
    
    inline float4x4 make_rotation_matrix(const quatf & rotation)
    {
        return {{qxdir(rotation),0},{qydir(rotation),0},{qzdir(rotation),0},{0,0,0,1}};
    }
    
    inline float4x4 make_rotation_matrix(const float3 & axis, float angle)
    {
        return make_rotation_matrix(make_rotation_quat_axis_angle(axis, angle));
    }
    
    inline float4x4 make_translation_matrix(const float3 & translation)
    {
        return {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {translation,1}};
    }
    
    inline float4x4 make_rigid_transformation_matrix(const quatf & rotation, const float3 & translation)
    {
        return {{qxdir(rotation),0},{qydir(rotation),0},{qzdir(rotation),0},{translation,1}};
    }

    //     | 1-2Nx^2   -2NxNy  -2NxNz  -2NxD |
    // m = |  -2NxNy  1-2Ny^2  -2NyNz  -2NyD |
    //     |  -2NxNz   -2NyNz 1-2Nz^2  -2NzD |
    //     |    0        0       0       1   |
    // Where (Nx,Ny,Nz,D) are the coefficients of plane equation (xNx + yNy + zNz + D = 0) and
    // (Nx, Ny, Nz) is the normal vector of given plane.
    inline float4x4 make_reflection_matrix(const float4 & plane)
    {
        static const float4x4 Zero4x4 = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 } };
        float4x4 reflectionMat = Zero4x4;

        reflectionMat[0][0] = (1.f - 2.0f * plane[0] * plane[0]);
        reflectionMat[1][0] = (-2.0f * plane[0] * plane[1]);
        reflectionMat[2][0] = (-2.0f * plane[0] * plane[2]);
        reflectionMat[3][0] = (-2.0f * plane[3] * plane[0]);

        reflectionMat[0][1] = (-2.0f * plane[1] * plane[0]);
        reflectionMat[1][1] = (1.f - 2.0f * plane[1] * plane[1]);
        reflectionMat[2][1] = (-2.0f * plane[1] * plane[2]);
        reflectionMat[3][1] = (-2.0f * plane[3] * plane[1]);

        reflectionMat[0][2] = (-2.0f * plane[2] * plane[0]);
        reflectionMat[1][2] = (-2.0f * plane[2] * plane[1]);
        reflectionMat[2][2] = (1.f - 2.0f * plane[2] * plane[2]);
        reflectionMat[3][2] = (-2.0f * plane[3] * plane[2]);

        reflectionMat[0][3] = 0.0f;
        reflectionMat[1][3] = 0.0f;
        reflectionMat[2][3] = 0.0f;
        reflectionMat[3][3] = 1.0f;

        return reflectionMat;
    }

    //////////////////////////////////////////
    //   general transformation utilities   //
    //////////////////////////////////////////

    inline float4x4 remove_scale(float4x4 transform)
    {
        transform.row(0) = normalize(transform.row(0));
        transform.row(1) = normalize(transform.row(1));
        transform.row(2) = normalize(transform.row(2));
        return transform;
    }

    inline float3x3 get_rotation_submatrix(const float4x4 & transform)
    {
        return { transform[0].xyz, transform[1].xyz, transform[2].xyz };
    }

    inline float3 get_translation_vector(const float4x4 & transform)
    {
        return { transform.row(0).w, transform.row(1).w, transform.row(2).w };
    }

    inline float3 transform_coord(const float4x4 & transform, const float3 & coord)
    {
        auto r = transform * float4(coord, 1); return (r.xyz / r.w);
    }

    inline float3 transform_vector(const float4x4 & transform, const float3 & vector)
    {
        return (transform * float4(vector, 0)).xyz;
    }

    /////////////////////////////////////
    //   transformation construction   //
    /////////////////////////////////////

    // The long form of (transform) source.inverse() * target
    inline transform make_transform_from_to(const transform & source, const transform & target)
    {
        transform ret;
        const auto inv = inverse(source.orientation);
        ret.orientation = inv * target.orientation;
        ret.position = qrot(inv, target.position - source.position);
        return ret;
    }

    inline transform lookat_rh(float3 eyePoint, float3 target, float3 worldUp = { 0,1,0 })
    {
        transform p;
        float3 zDir = normalize(eyePoint - target);
        float3 xDir = normalize(cross(worldUp, zDir));
        float3 yDir = cross(zDir, xDir);
        p.position = eyePoint;
        p.orientation = normalize(make_rotation_quat_from_rotation_matrix({ xDir, yDir, zDir }));
        return p;
    }

    inline transform lookat_lh(float3 eyePoint, float3 target, float3 worldUp = { 0,1,0 })
    {
        transform p;
        float3 zDir = normalize(target - eyePoint);
        float3 xDir = normalize(cross(worldUp, zDir));
        float3 yDir = cross(zDir, xDir);
        p.position = eyePoint;
        p.orientation = normalize(make_rotation_quat_from_rotation_matrix({ xDir, yDir, zDir }));
        return p;
    }

    // @tofix - this is not correct for parallel transport frames
    inline transform make_transform_from_matrix(const float4x4 & xform)
    {
        transform p;
        p.position = xform[3].xyz;
        p.orientation = make_rotation_quat_from_rotation_matrix(get_rotation_submatrix(xform));
        return p;
    }

    /////////////////////////////////////
    //   coordinate system utilities   //
    /////////////////////////////////////

    // A value type representing an abstract direction vector in 3D space, independent of any coordinate system
    enum class coord_axis { forward, back, left, right, up, down };

    constexpr float dot(coord_axis a, coord_axis b) { return a == b ? 1 : (static_cast<int>(a) ^ static_cast<int>(b)) == 1 ? -1 : 0.0f; }

    // A concrete 3D coordinate system with defined x, y, and z axes
    struct coord_system
    {
        coord_axis x_axis, y_axis, z_axis;
        constexpr float3 operator ()(coord_axis axis) const { return { dot(x_axis, axis), dot(y_axis, axis), dot(z_axis, axis) }; }
        constexpr float3 cross(coord_axis a, coord_axis b) const { return linalg::cross((*this)(a), (*this)(b)); }
        constexpr bool is_orthogonal() const { return dot(x_axis, y_axis) == 0 && dot(y_axis, z_axis) == 0 && dot(z_axis, x_axis) == 0; }
        constexpr bool is_left_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::left)) == 1; }
        constexpr bool is_right_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::right)) == 1; }
        float3 get_axis(coord_axis a) const { return{ dot(x_axis, a), dot(y_axis, a), dot(z_axis, a) }; }
        float3 get_left() const { return get_axis(coord_axis::left); }
        float3 get_right() const { return get_axis(coord_axis::right); }
        float3 get_up() const { return get_axis(coord_axis::up); }
        float3 get_down() const { return get_axis(coord_axis::down); }
        float3 get_forward() const { return get_axis(coord_axis::forward); }
        float3 get_back() const { return get_axis(coord_axis::back); }
    };

    inline float4x4 coordinate_system_from_to(const coord_system & from, const coord_system & to)
    {
        return{
            { to.get_axis(from.x_axis), 0 },
            { to.get_axis(from.y_axis), 0 },
            { to.get_axis(from.z_axis), 0 },
            { 0,0,0,1 }
        };
    }

    ///////////////////////////////////////////
    //   spherical & cartesian coordinates   //
    ///////////////////////////////////////////

    // These functions adopt the physics convention (ISO):
    // * (rho) r defined as the radial distance, 
    // * (theta) θ defined as the the polar angle (inclination)
    // * (phi) φ defined as the azimuthal angle (zenith)

    // These conversion routines assume the following: 
    // * the systems have the same origin
    // * the spherical reference plane is the cartesian xy-plane
    // * θ is inclination from the z direction
    // * φ is measured from the cartesian x-axis (so that the y-axis has φ = +90°)

    // theta ∈ [0, π], phi ∈ [0, 2π), rho ∈ [0, ∞)
    inline float3 cartsesian_coord(float thetaRad, float phiRad, float rhoRad = 1.f)
    {
        return float3(rhoRad * sin(thetaRad) * cos(phiRad), rhoRad * sin(phiRad) * sin(thetaRad), rhoRad * cos(thetaRad));
    }

    inline float3 spherical_coord(const float3 & coord)
    {
        const float radius = length(coord);
        return float3(radius, std::acos(coord.z / radius), std::atan(coord.y / coord.x));
    }

}

#endif // end math_spatial_hpp

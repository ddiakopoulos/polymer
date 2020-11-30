#pragma once

#ifndef polymer_bullet_utils_hpp
#define polymer_bullet_utils_hpp

#include "math-core.hpp"
#include "btBulletCollisionCommon.h"

namespace polymer
{
    struct bt_contact_point
    {
        float depth{ 1.f };
        float3 location;
        float3 normal;
        float3 velocity;
        float velocityNorm;
        btCollisionObject * object;
    };

    inline btVector3 to_bt(const float3 & v)
    {
        return btVector3(static_cast<btScalar>(v.x), static_cast<btScalar>(v.y), static_cast<btScalar>(v.z));
    }

    inline btQuaternion to_bt(const quatf & q)
    {
        return btQuaternion(static_cast<btScalar>(q.x), static_cast<btScalar>(q.y), static_cast<btScalar>(q.z), static_cast<btScalar>(q.w));
    }

    inline btMatrix3x3 to_bt(const float3x3 & m)
    {
        auto rQ = make_rotation_quat_from_rotation_matrix(m);
        return btMatrix3x3(to_bt(rQ));
    }

    inline btTransform to_bt(const float4x4 & xform)
    {
        auto r = get_rotation_submatrix(xform);
        auto t = xform[3].xyz;
        return btTransform(to_bt(r), to_bt(t));
    }

    inline float3 from_bt(const btVector3 & v)
    {
        return float3(v.x(), v.y(), v.z());
    }

    inline quatf from_bt(const btQuaternion & q)
    {
        return quatf(q.x(), q.y(), q.z(), q.w());
    }

    inline float3x3 from_bt(const btMatrix3x3 & m)
    {
        btQuaternion q;
        m.getRotation(q);
        return get_rotation_submatrix(make_rotation_matrix(from_bt(q)));
    }

    inline float4x4 from_bt(const btTransform & xform)
    {
        auto tM = make_translation_matrix(from_bt(xform.getOrigin()));
        auto rM = make_rotation_matrix(from_bt(xform.getRotation()));
        return tM * rM;
    }

    inline transform make_pose(const btTransform & xform)
    {
        return { from_bt(xform.getRotation()), from_bt(xform.getOrigin()) };
    }

    inline btTransform to_bt(const transform & xform)
    {
        btTransform x;
        x.getOrigin() = to_bt(xform.position);
        x.getRotation() = to_bt(xform.orientation);
        return x;
    }

} // end namespace polymer

#endif // end bullet_utils_hpp

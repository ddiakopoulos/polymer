#pragma once

#ifndef polymer_bullet_object_hpp
#define polymer_bullet_object_hpp

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "math-core.hpp"
#include "geometry.hpp"
#include "bullet_utils.hpp"

#include <functional>

namespace polymer
{
    struct contact_result_callback : public btCollisionWorld::ContactResultCallback
    {
        btRigidBody & body;
        std::vector<bt_contact_point> contacts;

        contact_result_callback(btRigidBody & target) : btCollisionWorld::ContactResultCallback(), body(target) { }

        // Called for each contact point
        virtual btScalar addSingleResult(btManifoldPoint & cp, const btCollisionObjectWrapper * colObj0, int, int, const btCollisionObjectWrapper * colObj1, int, int) override
        {
            btVector3 point, n;
            bt_contact_point p;

            if (colObj0->m_collisionObject == &body)
            {
                point = cp.m_localPointA;
                n = cp.m_normalWorldOnB;
                p.object = const_cast<btCollisionObject*>(colObj1->m_collisionObject);
            }
            else
            {
                point = cp.m_localPointB;
                n = -cp.m_normalWorldOnB;
                p.object = const_cast<btCollisionObject*>(colObj0->m_collisionObject);
            }

            p.location = { point.x(), point.y(), point.z() };
            p.normal = { n.x(), n.y(), n.z() };
            p.depth = fabs(cp.getDistance());
            p.velocity = { body.getLinearVelocity().x(), body.getLinearVelocity().y(), body.getLinearVelocity().z() };
            p.velocityNorm = dot(p.normal, p.velocity);

            contacts.push_back(p);

            return 0;
        }
    };

    class physics_object
    {
        std::shared_ptr<btDiscreteDynamicsWorld> world;

    public:

        btMotionState * state = { nullptr };
        std::unique_ptr<btRigidBody> body = { nullptr };

        physics_object(btMotionState * state, btCollisionShape * collisionShape, std::shared_ptr<btDiscreteDynamicsWorld> world, float mass = 1.0f)
            : state(state), world(world)
        {
            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) collisionShape->calculateLocalInertia(mass, inertia);
            btRigidBody::btRigidBodyConstructionInfo constructionInfo(mass, state, collisionShape, inertia);
            body.reset(new btRigidBody(constructionInfo));
        }

        physics_object(float4x4 xform, btCollisionShape * collisionShape, std::shared_ptr<btDiscreteDynamicsWorld> world, float mass = 1.0f)
            : world(world)
        {
            btTransform worldXform = to_bt(xform);
            state = new btDefaultMotionState(worldXform);

            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) collisionShape->calculateLocalInertia(mass, inertia);
            btRigidBody::btRigidBodyConstructionInfo constructionInfo(mass, state, collisionShape, inertia);
            body.reset(new btRigidBody(constructionInfo));
        }

        ~physics_object()
        {
            if (state) delete state;
            world->removeCollisionObject(body.get());
        }

        btDiscreteDynamicsWorld * get_world() const
        {
            return world.get();
        }

        std::vector<bt_contact_point> collide_world() const
        {
            struct CollideCallbackWorld : public contact_result_callback
            {
                CollideCallbackWorld(btRigidBody & target) : contact_result_callback(target) {}
                virtual bool needsCollision(btBroadphaseProxy * proxy) const override
                {
                    if (!btCollisionWorld::ContactResultCallback::needsCollision(proxy)) return false; // superclass pre-filters
                    return body.checkCollideWithOverride(static_cast<btCollisionObject*>(proxy->m_clientObject)); // avoid contraint contacts
                }
            };

            CollideCallbackWorld callback(*body.get());
            world->contactTest(body.get(), callback);
            return callback.contacts;
        }

        std::vector<bt_contact_point> collide_with(btCollisionObject * other) const
        {
            contact_result_callback callback(*body.get());
            world->contactPairTest(body.get(), other, callback);
            return callback.contacts;
        }

        bool first_contact(const float4x4 src, bt_contact_point & contact) const
        {
            struct ContactCallback : public btCollisionWorld::ClosestConvexResultCallback
            {
                btRigidBody & me;
                bt_contact_point point;
                bool hit{ false };

                ContactCallback(btRigidBody & me, const float3 & to) : btCollisionWorld::ClosestConvexResultCallback(me.getWorldTransform().getOrigin(), btVector3(to[0], to[1], to[2])), me(me) {}

                bool needsCollision(btBroadphaseProxy* proxy) const
                {
                    if (proxy->m_clientObject == &me) return false;
                    if (!btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy)) return false;
                    return me.checkCollideWithOverride(static_cast<btCollisionObject*>(proxy->m_clientObject));
                }

                btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool)
                {
                    point.depth = convexResult.m_hitFraction;
                    point.normal = float3(convexResult.m_hitNormalLocal.x(), convexResult.m_hitNormalLocal.y(), convexResult.m_hitNormalLocal.z());
                    point.object = const_cast<btCollisionObject*>(convexResult.m_hitCollisionObject);
                    hit = true;
                    return 0;
                }
            };

            ContactCallback callback(*body.get(), src.w.xyz());
            world->convexSweepTest(reinterpret_cast<btConvexShape*>(body->getCollisionShape()), body->getWorldTransform(), to_bt(src), callback, 0);
            contact = callback.point;
            return callback.hit;
        }

    };

} // end namespace polymer

#endif // end polymer_bullet_object_hpp

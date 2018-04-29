#pragma once

#ifndef bullet_engine_vr_hpp
#define bullet_engine_vr_hpp

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "math-core.hpp"
#include "geometry.hpp"
#include "bullet_utils.hpp"
#include <functional>

#include "bullet_object.hpp"

using namespace polymer;

class BulletEngineVR
{
    using OnTickCallback = std::function<void(float, BulletEngineVR *)>;

    std::shared_ptr<btBroadphaseInterface> broadphase = { nullptr };
    std::shared_ptr<btDefaultCollisionConfiguration> collisionConfiguration = { nullptr };
    std::shared_ptr<btCollisionDispatcher> dispatcher = { nullptr };
    std::shared_ptr<btSequentialImpulseConstraintSolver> solver = { nullptr };
    std::shared_ptr<btDiscreteDynamicsWorld> dynamicsWorld = { nullptr };

    std::vector<OnTickCallback> bulletTicks;

    static void tick_callback(btDynamicsWorld * world, btScalar time)
    {
        BulletEngineVR * engineContext = static_cast<BulletEngineVR *>(world->getWorldUserInfo());

        for (auto & t : engineContext->bulletTicks)
        {
            t(static_cast<float>(time), engineContext);
        }
    }

public:

    BulletEngineVR()
    {
        broadphase.reset(new btDbvtBroadphase());
        collisionConfiguration.reset(new btDefaultCollisionConfiguration());
        dispatcher.reset(new btCollisionDispatcher(collisionConfiguration.get()));
        solver.reset(new btSequentialImpulseConstraintSolver());
        dynamicsWorld.reset(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get()));
        dynamicsWorld->setGravity(btVector3(0.f, -9.87f, 0.0f));
        dynamicsWorld->setInternalTickCallback(tick_callback, static_cast<void *>(this), true);
    }

    ~BulletEngineVR() { }

    std::shared_ptr<btDiscreteDynamicsWorld> get_world()
    {
        return dynamicsWorld;
    }

    // Add a new rigid body based on BulletObjectVR wrapper
    void add_object(BulletObjectVR * object)
    {
        object->body->setDamping(0.3f, 0.5f);
        dynamicsWorld->addRigidBody(object->body.get());
    }

    // Remove an existing rigid body based on BulletObjectVR wrapper
    void remove_object(BulletObjectVR * object)
    {
        object->get_world()->removeRigidBody(object->body.get());
    }

    void add_task(const OnTickCallback & f)
    {
        bulletTicks.push_back({ f });
    }

    void update(const float dt = 0.0f)
    {
        const int result = dynamicsWorld->stepSimulation(dt);
    }

};

#endif // end bullet_engine_vr_hpp

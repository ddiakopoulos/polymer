#pragma once

#ifndef polymer_bullet_engine_hpp
#define polymer_bullet_engine_hpp

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "math-core.hpp"
#include "geometry.hpp"
#include "bullet_utils.hpp"
#include "bullet_object.hpp"

namespace polymer
{

    class bullet_engine
    {
        using tick_callback_t = std::function<void(float, bullet_engine *)>;

        std::shared_ptr<btBroadphaseInterface> broadphase = { nullptr };
        std::shared_ptr<btDefaultCollisionConfiguration> collisionConfiguration = { nullptr };
        std::shared_ptr<btCollisionDispatcher> dispatcher = { nullptr };
        std::shared_ptr<btSequentialImpulseConstraintSolver> solver = { nullptr };
        std::shared_ptr<btDiscreteDynamicsWorld> dynamicsWorld = { nullptr };

        std::vector<tick_callback_t> bulletTicks;

        static void tick_callback(btDynamicsWorld * world, btScalar time)
        {
            bullet_engine * ctx = static_cast<bullet_engine *>(world->getWorldUserInfo());
            for (auto & t : ctx->bulletTicks)
            {
                t(static_cast<float>(time), ctx);
            }
        }

    public:

        bullet_engine()
        {
            broadphase.reset(new btDbvtBroadphase());
            collisionConfiguration.reset(new btDefaultCollisionConfiguration());
            dispatcher.reset(new btCollisionDispatcher(collisionConfiguration.get()));
            solver.reset(new btSequentialImpulseConstraintSolver());
            dynamicsWorld.reset(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get()));
            dynamicsWorld->setGravity(btVector3(0.f, -9.87f, 0.0f));
            dynamicsWorld->setInternalTickCallback(tick_callback, static_cast<void *>(this), true);
        }

        std::shared_ptr<btDiscreteDynamicsWorld> get_world()
        {
            return dynamicsWorld;
        }

        // Add a new rigid body based on physics_object wrapper
        void add_object(physics_object * object)
        {
            object->body->setDamping(0.3f, 0.5f);
            dynamicsWorld->addRigidBody(object->body.get());
        }

        // Remove an existing rigid body based on physics_object wrapper
        void remove_object(physics_object * object)
        {
            object->get_world()->removeRigidBody(object->body.get());
        }

        void add_task(const tick_callback_t & f)
        {
            bulletTicks.push_back({ f });
        }

        void update(const float dt = 0.0f)
        {
            const int result = dynamicsWorld->stepSimulation(dt);
        }

    };

} // end namespace polymer

#endif // end polymer_bullet_engine_hpp

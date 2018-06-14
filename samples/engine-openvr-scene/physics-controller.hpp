#pragma once

#include "lib-polymer.hpp"
#include "lib-engine.hpp"
#include "bullet_engine.hpp"
#include "bullet_object.hpp"
#include "bullet_utils.hpp"
#include "bullet_visualizer.hpp"

namespace polymer
{

    class physics_object_openvr_controller
    {
        transform latestPose;

        void update_physics(const float dt, bullet_engine * engine)
        {
            physicsObject->body->clearForces();
            physicsObject->body->setWorldTransform(to_bt(latestPose.matrix()));
        }

    public:

        std::shared_ptr<bullet_engine> engine;
        const openvr_controller * ctrl;

        btCollisionShape * controllerShape{ nullptr };
        physics_object * physicsObject{ nullptr };

        physics_object_openvr_controller(std::shared_ptr<bullet_engine> engine, const openvr_controller * ctrl)
            : engine(engine), ctrl(ctrl)
        {

            // Physics tick
            engine->add_task([=](float time, bullet_engine * engine)
            {
                this->update_physics(time, engine);
            });

            controllerShape = new btBoxShape(btVector3(0.096f, 0.096f, 0.0123f)); // fixme to use renderData

            physicsObject = new physics_object(new btDefaultMotionState(), controllerShape, engine->get_world(), 0.5f); // Controllers require non-zero mass

            physicsObject->body->setFriction(2.f);
            physicsObject->body->setRestitution(0.1f);
            physicsObject->body->setGravity(btVector3(0, 0, 0));
            physicsObject->body->setActivationState(DISABLE_DEACTIVATION);

            engine->add_object(physicsObject);
        }

        ~physics_object_openvr_controller()
        {
            engine->remove_object(physicsObject);
            delete physicsObject;
        }

        void update(const transform & latestControllerPose)
        {
            latestPose = latestControllerPose;

            auto collisionList = physicsObject->collide_world();
            for (auto & c : collisionList)
            {
                // Contact points
            }
        }
    };

} // end namespace polymer
#include "index.hpp"

#include "bullet_engine.hpp"
#include "bullet_visualizer.hpp"
#include "openvr-hmd.hpp"
#include "shader-library.hpp"

#include "gfx/gl/gl-renderable-grid.hpp"
#include "gfx/gl/gl-camera.hpp"
#include "gfx/gl/gl-async-gpu-timer.hpp"

#include "radix_sort.hpp"
#include "procedural_mesh.hpp"
#include "parabolic_pointer.hpp"

#include <future>
#include "quick_hull.hpp"
#include "algo_misc.hpp"
#include "gl-imgui.hpp"

using namespace polymer;

struct viewport_t
{
    float2 bmin, bmax;
    GLuint texture;
};

// physics_object_openvr_controller wraps physics_object and is responsible for creating a controlled physically-activating 
// object, and keeping the physics engine aware of the latest user-controlled pose.
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
    std::shared_ptr<cached_controller_render_data> renderData;

    btCollisionShape * controllerShape{ nullptr };
    physics_object * physicsObject{ nullptr };

    physics_object_openvr_controller(std::shared_ptr<bullet_engine> engine, const openvr_controller * ctrl, std::shared_ptr<cached_controller_render_data> renderData)
        : engine(engine), ctrl(ctrl), renderData(renderData)
    {

        // Physics tick
        engine->add_task([=](float time, bullet_engine * engine)
        {
            this->update_physics(time, engine);
        });

        controllerShape = new btBoxShape(btVector3(0.096, 0.096, 0.0123)); // fixme to use renderData

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

struct vr_app_state
{
    gl_renderable_grid grid {0.25f, 24, 24 };
    geometry navMesh;

    pointer_data params;
    bool regeneratePointer = false;

    std::unique_ptr<physics_object_openvr_controller> leftController;
    std::unique_ptr<physics_object_openvr_controller> rightController;
    std::vector<std::shared_ptr<physics_object>> physicsObjects;

    bool needsTeleport{ false };
    float3 teleportLocation;
    /* StaticMesh teleportationArc; */
};

struct sample_vr_app : public polymer_app
{
    uint64_t frameCount = 0;

    std::unique_ptr<openvr_hmd> hmd;

    perspective_camera debugCam;
    fps_camera_controller cameraController;

    gl_shader_monitor shaderMonitor = { "../assets/" };
    
    std::vector<viewport_t> viewports;
    vr_app_state scene;

    simple_cpu_timer t;
    gl_gpu_timer gpuTimer;

    std::shared_ptr<bullet_engine> physicsEngine;
    std::unique_ptr<physics_visualizer> physicsDebugRenderer;

    std::unique_ptr<gui::imgui_instance> igm;

    sample_vr_app();
    ~sample_vr_app();

    void setup_physics();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
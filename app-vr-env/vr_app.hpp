#include "index.hpp"

#include "../lib-engine/bullet_engine.hpp"
#include "../lib-engine/bullet_debug.hpp"
#include "../lib-engine/openvr-hmd.hpp"
#include "../lib-engine/shader-library.hpp"

#include "../lib-engine/gfx/gl/gl-renderable-grid.hpp"
#include "../lib-engine/gfx/gl/gl-camera.hpp"
#include "../lib-engine/gfx/gl/gl-async-gpu-timer.hpp"

#include "radix_sort.hpp"
#include "procedural_mesh.hpp"
#include "parabolic_pointer.hpp"

#include <future>
#include "quick_hull.hpp"
#include "algo_misc.hpp"
#include "gl-imgui.hpp"

using namespace polymer;

struct ScreenViewport
{
    float2 bmin, bmax;
    GLuint texture;
};

// MotionControllerVR wraps BulletObjectVR and is responsible for creating a controlled physically-activating 
// object, and keeping the physics engine aware of the latest user-controlled pose.
class MotionControllerVR
{
    transform latestPose;

    void update_physics(const float dt, BulletEngineVR * engine)
    {
        physicsObject->body->clearForces();
        physicsObject->body->setWorldTransform(to_bt(latestPose.matrix()));
    }

public:

    std::shared_ptr<BulletEngineVR> engine;
    const openvr_controller * ctrl;
    std::shared_ptr<cached_controller_render_data> renderData;

    btCollisionShape * controllerShape{ nullptr };
    BulletObjectVR * physicsObject{ nullptr };

    MotionControllerVR(std::shared_ptr<BulletEngineVR> engine, const openvr_controller * ctrl, std::shared_ptr<cached_controller_render_data> renderData)
        : engine(engine), ctrl(ctrl), renderData(renderData)
    {

        // Physics tick
        engine->add_task([=](float time, BulletEngineVR * engine)
        {
            this->update_physics(time, engine);
        });

        controllerShape = new btBoxShape(btVector3(0.096, 0.096, 0.0123)); // fixme to use renderData

        physicsObject = new BulletObjectVR(new btDefaultMotionState(), controllerShape, engine->get_world(), 0.5f); // Controllers require non-zero mass

        physicsObject->body->setFriction(2.f);
        physicsObject->body->setRestitution(0.1f);
        physicsObject->body->setGravity(btVector3(0, 0, 0));
        physicsObject->body->setActivationState(DISABLE_DEACTIVATION);

        engine->add_object(physicsObject);
    }

    ~MotionControllerVR()
    {
        engine->remove_object(physicsObject);
        delete physicsObject;
    }

    void update(const transform & latestControllerPose)
    {
        latestPose = latestControllerPose;

        auto collisionList = physicsObject->CollideWorld();
        for (auto & c : collisionList)
        {
            // Contact points
        }
    }
};

struct poly_scene
{
    gl_renderable_grid grid {0.25f, 24, 24 };
    Geometry navMesh;

    ParabolicPointerParams params;
    bool regeneratePointer = false;

    std::unique_ptr<MotionControllerVR> leftController;
    std::unique_ptr<MotionControllerVR> rightController;

    bool needsTeleport{ false };
    float3 teleportLocation;
    /* StaticMesh teleportationArc; */

    std::vector<std::shared_ptr<BulletObjectVR>> physicsObjects;

};

struct VirtualRealityApp : public polymer_app
{
    uint64_t frameCount = 0;

    std::unique_ptr<openvr_hmd> hmd;

    perspective_camera debugCam;
    fps_camera_controller cameraController;

    gl_shader_monitor shaderMonitor = { "../assets/" };
    
    std::vector<ScreenViewport> viewports;
    poly_scene scene;

    simple_cpu_timer t;
    gl_gpu_timer gpuTimer;

    std::shared_ptr<BulletEngineVR> physicsEngine;
    std::unique_ptr<PhysicsDebugRenderer> physicsDebugRenderer;

    std::unique_ptr<gui::imgui_instance> igm;

    VirtualRealityApp();
    ~VirtualRealityApp();

    void setup_physics();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};
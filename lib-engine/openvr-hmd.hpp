#pragma once

#ifndef polymer_openvr_hmd_hpp
#define polymer_openvr_hmd_hpp

#include "openvr/include/openvr.h"
#include "math-core.hpp"
#include "geometry.hpp"
#include "gl-api.hpp"

using namespace polymer;

inline Pose make_pose(const vr::HmdMatrix34_t & m)
{
    return {
        make_rotation_quat_from_rotation_matrix({ { m.m[0][0], m.m[1][0], m.m[2][0] },{ m.m[0][1], m.m[1][1], m.m[2][1] },{ m.m[0][2], m.m[1][2], m.m[2][2] } }),
        { m.m[0][3], m.m[1][3], m.m[2][3] }
    };
}

struct cached_controller_render_data
{
    Geometry mesh;
    GlTexture2D tex;
    bool loaded = false;
};

class openvr_controller
{
    Pose p;

public:

    struct button_state
    {
        bool down = false;
        bool lastDown = false;
        bool pressed = false;
        bool released = false;

        void update(bool state)
        {
            lastDown = down;
            down = state;
            pressed = (!lastDown) && state;
            released = lastDown && (!state);
        }
    };

    button_state pad;
    button_state trigger;

    float2 touchpad = float2(0.0f, 0.0f);

    void set_pose(const Pose & newPose) { p = newPose; }
    const Pose get_pose(const Pose & worldPose) const { return worldPose * p; }
    Ray forward_ray() const { return Ray(p.position, p.transform_vector(float3(0.0f, 0.0f, -1.0f))); }
};

class openvr_hmd 
{
    vr::IVRSystem * hmd { nullptr };
    vr::IVRRenderModels * renderModels { nullptr };

    uint2 renderTargetSize;
    Pose hmdPose;
    Pose worldPose;

    std::shared_ptr<cached_controller_render_data> controllerRenderData;
    openvr_controller controllers[2];

public:

    openvr_hmd();
    ~openvr_hmd();

    void set_world_pose(const Pose & p);
    Pose get_world_pose();

    Pose get_hmd_pose() const;
    void set_hmd_pose(const Pose & p);

    Pose get_eye_pose(vr::Hmd_Eye eye);

    uint2 get_recommended_render_target_size();
    float4x4 get_proj_matrix(vr::Hmd_Eye eye, float near_clip, float far_clip);
    void get_optical_properties(vr::Hmd_Eye eye, float & aspectRatio, float & vfov);

    const openvr_controller * get_controller(const vr::ETrackedControllerRole controller);
    std::shared_ptr<cached_controller_render_data> get_controller_render_data();

    void update();
    void submit(const GLuint leftEye, const GLuint rightEye);
};

#endif // end polymer_openvr_hmd_hpp

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

struct ControllerRenderData
{
    Geometry mesh;
    GlTexture2D tex;
    bool loaded = false;
};

class OpenVR_Controller
{
    Pose p;

public:

    struct ButtonState
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

    ButtonState pad;
    ButtonState trigger;

    float2 touchpad = float2(0.0f, 0.0f);

    void set_pose(const Pose & newPose) { p = newPose; }
    const Pose get_pose(const Pose & worldPose) const { return worldPose * p; }
    Ray forward_ray() const { return Ray(p.position, p.transform_vector(float3(0.0f, 0.0f, -1.0f))); }
};

struct camera_intrinsics
{
    int width;  // width of the image in pixels
    int height; // height of the image in pixels
    float ppx;  // horizontal coordinate of the principal point of the image, as a pixel offset from the left edge
    float ppy;  // vertical coordinate of the principal point of the image, as a pixel offset from the top edge
    float fx;   // focal length of the image plane, as a multiple of pixel width
    float fy;   // focal length of the image plane, as a multiple of pixel height
};

struct tracked_camera_frame
{
    Pose render_pose;
    GlTexture2D texture;
    image_buffer<uint8_t, 3> rawBytes;
};

class OpenVR_TrackedCamera
{
    vr::IVRSystem * hmd{ nullptr };
    vr::IVRTrackedCamera * trackedCamera{ nullptr };

    vr::TrackedCameraHandle_t trackedCameraHandle{ INVALID_TRACKED_CAMERA_HANDLE };

    uint32_t lastFrameSequence{ 0 };
    uint32_t cameraFrameBufferSize{ 0 };

    camera_intrinsics intrin;
    float4x4 projectionMatrix;
    tracked_camera_frame frame;

public:

    bool initialize(vr::IVRSystem * vr_system);
    bool start();
    void stop();
    void capture();

    camera_intrinsics get_intrinsics() const { return intrin; }
    float4x4 const get_projection_matrix() { return projectionMatrix; }
    tracked_camera_frame & get_frame() { return frame; }
};

class OpenVR_HMD 
{
    vr::IVRSystem * hmd { nullptr };
    vr::IVRRenderModels * renderModels { nullptr };

    uint2 renderTargetSize;
    Pose hmdPose;
    Pose worldPose;

    std::shared_ptr<ControllerRenderData> controllerRenderData;
    OpenVR_Controller controllers[2];

public:

    OpenVR_HMD();
    ~OpenVR_HMD();

    const OpenVR_Controller * get_controller(const vr::ETrackedControllerRole controller)     
    {
        if (controller == vr::TrackedControllerRole_LeftHand) return &controllers[0];
        if (controller == vr::TrackedControllerRole_RightHand) return &controllers[1];
        if (controller == vr::TrackedControllerRole_Invalid) throw std::runtime_error("invalid controller enum");
        return nullptr;
    }

    std::shared_ptr<ControllerRenderData> get_controller_render_data() { return controllerRenderData; }

    void set_world_pose(const Pose & p) { worldPose = p; }
    Pose get_world_pose() { return worldPose; }

    Pose get_hmd_pose() const { return worldPose * hmdPose; }

    void set_hmd_pose(Pose p) { hmdPose = p; }

    uint2 get_recommended_render_target_size() { return renderTargetSize; }

    float4x4 get_proj_matrix(vr::Hmd_Eye eye, float near_clip, float far_clip) { return transpose(reinterpret_cast<const float4x4 &>(hmd->GetProjectionMatrix(eye, near_clip, far_clip))); }

    Pose get_eye_pose(vr::Hmd_Eye eye) { return get_hmd_pose() * make_pose(hmd->GetEyeToHeadTransform(eye)); }

    void get_optical_properties(vr::Hmd_Eye eye, float & aspectRatio, float & vfov)
    {
        float l_left = 0.0f, l_right = 0.0f, l_top = 0.0f, l_bottom = 0.0f;
        hmd->GetProjectionRaw(vr::Hmd_Eye::Eye_Left, &l_left, &l_right, &l_top, &l_bottom);

        float r_left = 0.0f, r_right = 0.0f, r_top = 0.0f, r_bottom = 0.0f;
        hmd->GetProjectionRaw(vr::Hmd_Eye::Eye_Right, &r_left, &r_right, &r_top, &r_bottom);

        float2 tanHalfFov = float2(max(-l_left, l_right, -r_left, r_right), max(-l_top, l_bottom, -r_top, r_bottom));
        aspectRatio = tanHalfFov.x / tanHalfFov.y;
        vfov = 2.0f * std::atan(tanHalfFov.y);
    }

    void update();

    void submit(const GLuint leftEye, const GLuint rightEye);
};

#endif // end polymer_openvr_hmd_hpp

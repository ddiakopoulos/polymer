#pragma once

#ifndef polymer_openvr_hmd_hpp
#define polymer_openvr_hmd_hpp

#include "openvr/include/openvr.h"
#include "math-core.hpp"
#include "geometry.hpp"
#include "gl-api.hpp"
#include <functional>

using namespace polymer;

inline transform make_pose(const vr::HmdMatrix34_t & m)
{
    return {
        make_rotation_quat_from_rotation_matrix({ { m.m[0][0], m.m[1][0], m.m[2][0] },{ m.m[0][1], m.m[1][1], m.m[2][1] },{ m.m[0][2], m.m[1][2], m.m[2][2] } }),
        { m.m[0][3], m.m[1][3], m.m[2][3] }
    };
}

struct cached_controller_render_data
{
    geometry mesh;
    gl_texture_2d tex;
    bool loaded = false;
};

struct input_button_state
{
    bool prev_down{ false };    // do not use directly, for state tracking only
    bool down{ false };         // query if the button is currently down
    bool pressed{ false };      // query if the button was pressed for a single frame
    bool released{ false };     // query if the button was released for a single frame
};

inline void update_button_state(input_button_state & state, const bool value)
{
    state.prev_down = state.down;
    state.down = value;
    state.pressed = !state.prev_down && value;
    state.released = state.prev_down && !value;
}

struct openvr_controller
{
    transform t;
    float2 axis_values{ 0.f, 0.f };
    std::unordered_map<vr::EVRButtonId, input_button_state> buttons;
};

class openvr_hmd 
{
    vr::IVRSystem * hmd { nullptr };
    vr::IVRRenderModels * renderModels { nullptr };

    uint2 renderTargetSize;
    transform hmdPose, worldPose;

    cached_controller_render_data controllerRenderData;
    openvr_controller controllers[2];
    std::function<void(cached_controller_render_data & data)> async_data_cb;
    void load_render_data_impl(vr::VREvent_t event);

public:

    openvr_hmd();
    ~openvr_hmd();

    // The world pose represents an offset that is applied to `get_hmd_pose()`
    // It is most useful for teleportation functionality. 
    void set_world_pose(const transform & p);
    transform get_world_pose();

    // The pose of the headset, relative to the current world pose.
    // If no world pose is set, then it is relative to the center of the OpenVR
    // tracking volume. The view matrix is derived from this data. 
    transform get_hmd_pose() const;
    void set_hmd_pose(const transform & p);

    // Returns the per-eye flavor of the view matrix. The eye pose and
    // hmd pose must be multiplied together to derive the per-eye
    // view matrix with correct stereo disparity that must be submitted to the
    // renderer. 
    transform get_eye_pose(vr::Hmd_Eye eye);

    // OpenVR provides a recommended render target size, in pixels.  
    uint2 get_recommended_render_target_size();

    // Returns the per-eye projection matrix given a near and far clip value. 
    float4x4 get_proj_matrix(vr::Hmd_Eye eye, float near_clip, float far_clip);
    
    // Returns the aspect ratio and vertical field of view in radians for a given eye
    void get_optical_properties(vr::Hmd_Eye eye, float & aspectRatio, float & vfov);

    // Returns current controller state.
    openvr_controller get_controller(const vr::ETrackedControllerRole controller);

    void controller_render_data_callback(std::function<void(cached_controller_render_data & data)> callback);

    // Must be called per-frame in the update loop
    void update();

    // Submit rendered per-eye OpenGL textures to the OpenVR compositor
    void submit(const GLuint leftEye, const GLuint rightEye);
};

#endif // end polymer_openvr_hmd_hpp

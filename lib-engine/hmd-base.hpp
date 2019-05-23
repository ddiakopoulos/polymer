#pragma once

#ifndef polymer_hmd_base_hpp
#define polymer_hmd_base_hpp

#include "math-core.hpp"
#include "geometry.hpp"
#include "gl-api.hpp"
#include <functional>

using namespace polymer;

///////////////////
//   Utilities   //
///////////////////

enum class vr_input_vendor
{
    unknown,
    vive_wand,
    valve_knuckles,
    oculus_rift_touch,
    oculus_quest_touch,
    oculus_go,
    leap_motion
};

enum class vr_controller_role
{
    invalid = 0,
    left_hand = 1,
    right_hand = 2
};

struct cached_controller_render_data
{
    geometry mesh;
    gl_texture_2d tex;
    bool loaded = false;
    vr_controller_role role;
};

struct vr_button_state
{
    bool prev_down{ false }; // do not use directly, for state tracking only
    bool down{ false };      // query if the button is currently down
    bool pressed{ false };   // query if the button was pressed for a single frame
    bool released{ false };  // query if the button was released for a single frame
};

void update_button_state(vr_button_state & state, const bool value);

enum class vr_eye
{
    left_eye = 0,
    right_eye = 1
};

enum class vr_button
{
   system,
   menu, 
   grip, 
   xy, 
   trigger
};

struct vr_controller
{
    transform t;
    float2 xy_values{ 0.f, 0.f };
    std::unordered_map<vr_button, vr_button_state> buttons;
};

vr_button get_button_id_for_vendor(const uint32_t which_button, const vr_input_vendor vendor);

////////////////////////////
//   hmd base interface   //
////////////////////////////

struct hmd_base
{
    // The world pose represents an offset that is applied to `get_hmd_pose()`
    // It is most useful for teleportation functionality. 
    virtual void set_world_pose(const transform & p) = 0;
    virtual transform get_world_pose() = 0;

    // The pose of the headset, relative to the current world pose.
    // If no world pose is set, then it is relative to the center of the
    // tracking volume. The view matrix is derived from this data. 
    virtual transform get_hmd_pose() const = 0;
    virtual void set_hmd_pose(const transform & p) = 0;

    // Returns the per-eye flavor of the view matrix. The eye pose and
    // hmd pose must be multiplied together to derive the per-eye view matrix
    // with correct stereo disparity that must be submitted to the renderer. 
    virtual transform get_eye_pose(vr_eye eye) = 0;

    // Return the recommended render target size, in pixels.  
    virtual uint2 get_recommended_render_target_size() = 0;

    // Returns the per-eye projection matrix given a near and far clip value. 
    virtual float4x4 get_proj_matrix(vr_eye eye, float near_clip, float far_clip) = 0;

    // Returns the aspect ratio and vertical field of view in radians for a given eye
    virtual void get_optical_properties(vr_eye eye, float & aspectRatio, float & vfov) = 0;

    // Returns current controller state.
    virtual vr_controller get_controller(vr_controller_role controller) = 0;
    virtual void controller_render_data_callback(std::function<void(cached_controller_render_data & data)> callback) = 0;

    // Returns the vendor of the the input controllers
    virtual vr_input_vendor get_input_vendor() = 0;

    // Returns the stencil mask for a given eye
    virtual gl_mesh get_stencil_mask(vr_eye eye) = 0;

    // Must be called per-frame in the update loop
    virtual void update() = 0;

    // Submit rendered per-eye OpenGL textures to the compositor
    virtual void submit(const GLuint leftEye, const GLuint rightEye) = 0;
};

#endif // end polymer_hmd_base_hpp

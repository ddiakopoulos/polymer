#pragma once

#ifndef polymer_openvr_hmd_hpp
#define polymer_openvr_hmd_hpp

#include "openvr/include/openvr.h"
#include "hmd-base.hpp"

using namespace polymer;

inline transform make_pose(const vr::HmdMatrix34_t & m)
{
    return {
        make_rotation_quat_from_rotation_matrix({ { m.m[0][0], m.m[1][0], m.m[2][0] },{ m.m[0][1], m.m[1][1], m.m[2][1] },{ m.m[0][2], m.m[1][2], m.m[2][2] } }),
        { m.m[0][3], m.m[1][3], m.m[2][3] }
    };
}

class openvr_hmd : public hmd_base
{
    vr::IVRSystem * hmd { nullptr };
    vr::IVRRenderModels * renderModels { nullptr };

    uint2 renderTargetSize;
    transform hmdPose, worldPose;

    cached_controller_render_data controllerRenderData[2];
    vr_controller controllers[2];
    std::function<void(cached_controller_render_data & data)> async_data_cb;
    void load_render_data_impl(vr::VREvent_t event);

public:

    openvr_hmd();
    ~openvr_hmd();
 
    virtual void set_world_pose(const transform & p) override final;
    virtual transform get_world_pose() override final;

    virtual transform get_hmd_pose() const;
    virtual void set_hmd_pose(const transform & p) override final;

    virtual transform get_eye_pose(vr_eye eye) override final;
    virtual vr_controller get_controller(vr_controller_role controller) override final;
    virtual uint2 get_recommended_render_target_size() override final;
    virtual float4x4 get_proj_matrix(vr_eye eye, float near_clip, float far_clip) override final;
    virtual void get_optical_properties(vr_eye eye, float & aspectRatio, float & vfov) override final;
    virtual gl_mesh get_stencil_mask(vr_eye eye) override final;
    virtual vr_input_vendor get_input_vendor() override final;

    virtual void controller_render_data_callback(std::function<void(cached_controller_render_data & data)> callback) override final;

    virtual void update() override final;
    virtual void submit(const GLuint leftEye, const GLuint rightEye) override final;
};

#endif // end polymer_openvr_hmd_hpp

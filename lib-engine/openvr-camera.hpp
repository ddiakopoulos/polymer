#pragma once

#ifndef polymer_openvr_camera_hpp
#define polymer_openvr_camera_hpp

#include "openvr/include/openvr.h"

#include "math-core.hpp"
#include "geometry.hpp"
#include "gl-api.hpp"
#include "image-buffer.hpp"

namespace polymer
{
    struct tracked_camera_frame
    {
        transform render_pose;
        gl_texture_2d texture;
        image_buffer<uint8_t> rawBytes;
    };

    class openvr_tracked_camera
    {
        vr::IVRSystem * hmd{ nullptr };
        vr::IVRTrackedCamera * trackedCamera{ nullptr };
        vr::TrackedCameraHandle_t trackedCameraHandle{ INVALID_TRACKED_CAMERA_HANDLE };

        uint32_t lastFrameSequence{ 0 };
        uint32_t cameraFrameBufferSize{ 0 };

        float4x4 projectionMatrix;
        camera_intrinsics intrin;
        tracked_camera_frame frame;
        uint32_t index;

    public:

        bool initialize(vr::IVRSystem * vr_system, const uint32_t camera_index);
        bool start();
        void stop();
        void capture();

        camera_intrinsics get_intrinsics() const { return intrin; }
        float4x4 const get_projection_matrix() { return projectionMatrix; }
        tracked_camera_frame & get_frame() { return frame; }
    };

} // end namespace polymer

#endif // end camera
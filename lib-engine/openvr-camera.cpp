#include "openvr-camera.hpp"
#include "openvr-hmd.hpp" // for make_pose(...)

using namespace polymer;

///////////////////////////////
//   OpenVR Tracked Camera   //
///////////////////////////////

#undef near
#undef far

bool openvr_tracked_camera::initialize(vr::IVRSystem * vr_system, const uint32_t camera_index) 
{
    if (!vr_system) return false;

    index = camera_index;
    assert(index < 2);

    hmd = vr_system;

    trackedCamera = vr::VRTrackedCamera();
    if (!trackedCamera)
    {
        std::cout << "could not acquire VRTrackedCamera" << std::endl;
        return false;
    }

    bool systemHasCamera = false;
    vr::EVRTrackedCameraError error = trackedCamera->HasCamera(vr::k_unTrackedDeviceIndex_Hmd, &systemHasCamera);
    if (error != vr::VRTrackedCameraError_None || !systemHasCamera)
    {
        std::cout << "system has no tracked camera available. did you enable it in steam settings? " << trackedCamera->GetCameraErrorNameFromEnum(error) << std::endl;
        return false;
    }

    vr::ETrackedPropertyError propertyError;
    char buffer[128];
    hmd->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_CameraFirmwareDescription_String, buffer, sizeof(buffer), &propertyError);
    if (propertyError != vr::TrackedProp_Success)
    {
        std::cout << "failed to get camera firmware desc" << std::endl;
        return false;
    }

    std::cout << "OpenVR_TrackedCamera::Prop_CameraFirmwareDescription_Stringbuffer - " << buffer << std::endl;

    vr::HmdVector2_t focalLength;
    vr::HmdVector2_t principalPoint;
    error = trackedCamera->GetCameraIntrinsics(vr::k_unTrackedDeviceIndex_Hmd, index, vr::VRTrackedCameraFrameType_MaximumUndistorted, &focalLength, &principalPoint);

    vr::HmdMatrix44_t trackedCameraProjection;
    float near = .01f;
    float far = 100.f;
    error = trackedCamera->GetCameraProjection(vr::k_unTrackedDeviceIndex_Hmd, index, vr::VRTrackedCameraFrameType_MaximumUndistorted, near, far, &trackedCameraProjection);

    projectionMatrix = transpose(reinterpret_cast<const float4x4 &>(trackedCameraProjection));

    intrin.fx = focalLength.v[0];
    intrin.fy = focalLength.v[1];
    intrin.ppx = principalPoint.v[0];
    intrin.ppy = principalPoint.v[1];

    return true;
}

bool openvr_tracked_camera::start()
{
    uint32_t frameWidth = 0;
    uint32_t frameHeight = 0;
    uint32_t framebufferSize = 0;

    if (trackedCamera->GetCameraFrameSize(vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_MaximumUndistorted, &frameWidth, &frameHeight, &framebufferSize) != vr::VRTrackedCameraError_None)
    {
        intrin.width = frameWidth;
        intrin.height = frameHeight;

        std::cout << "GetCameraFrameSize() failed" << std::endl;
        return false;
    }

    // Generate texture handle
    frame.texture.setup(intrin.width, intrin.height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Create a persistent buffer for holding the incoming camera data
    frame.rawBytes = image_buffer<uint8_t>(int2(frameWidth, frameHeight), 3);

    lastFrameSequence = 0;

    // Open and cache OpenVR camera handle
    trackedCamera->AcquireVideoStreamingService(vr::k_unTrackedDeviceIndex_Hmd, &trackedCameraHandle);
    if (trackedCameraHandle == INVALID_TRACKED_CAMERA_HANDLE)
    {
        std::cout << "AcquireVideoStreamingService() failed" << std::endl;
        return false;
    }

    return true;
}

void openvr_tracked_camera::stop()
{
    trackedCamera->ReleaseVideoStreamingService(trackedCameraHandle);
    trackedCameraHandle = INVALID_TRACKED_CAMERA_HANDLE;
}

void openvr_tracked_camera::capture()
{
    if (!trackedCamera || !trackedCameraHandle) return;

    vr::CameraVideoStreamFrameHeader_t frameHeader;
    vr::EVRTrackedCameraError error = trackedCamera->GetVideoStreamFrameBuffer(trackedCameraHandle, vr::VRTrackedCameraFrameType_MaximumUndistorted, nullptr, 0, &frameHeader, sizeof(frameHeader));
    if (error != vr::VRTrackedCameraError_None) return;

    // Ideally called once every ~16ms but who knows
    if (frameHeader.nFrameSequence == lastFrameSequence) return;

    // Copy
    error = trackedCamera->GetVideoStreamFrameBuffer(trackedCameraHandle, vr::VRTrackedCameraFrameType_MaximumUndistorted, frame.rawBytes.data(), cameraFrameBufferSize, &frameHeader, sizeof(frameHeader));
    if (error != vr::VRTrackedCameraError_None) return;

    frame.render_pose = make_pose(frameHeader.standingTrackedDevicePose.mDeviceToAbsoluteTracking);

    lastFrameSequence = frameHeader.nFrameSequence;

    glTextureImage2DEXT(frame.texture, GL_TEXTURE_2D, 0, GL_RGB, intrin.width, intrin.height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.rawBytes.data());
}
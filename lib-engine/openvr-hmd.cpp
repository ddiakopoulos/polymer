#include "openvr-hmd.hpp"

std::string get_tracked_device_string(vr::IVRSystem * pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL)
{
    uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
    if (unRequiredBufferLen == 0) return "";
    std::vector<char> pchBuffer(unRequiredBufferLen);
    unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer.data(), unRequiredBufferLen, peError);
    std::string result = { pchBuffer.begin(), pchBuffer.end() };
    return result;
}

///////////////////////////////
//   OpenVR Tracked Camera   //
///////////////////////////////

bool OpenVR_TrackedCamera::initialize(vr::IVRSystem * vr_system)
{
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
    error = trackedCamera->GetCameraIntrinsics(vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_MaximumUndistorted, &focalLength, &principalPoint);

    vr::HmdMatrix44_t trackedCameraProjection;
    float near = .01f;
    float far = 100.f;
    error = trackedCamera->GetCameraProjection(vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_MaximumUndistorted, near, far, &trackedCameraProjection);

    projectionMatrix = transpose(reinterpret_cast<const float4x4 &>(trackedCameraProjection));

    intrin.fx = focalLength.v[0];
    intrin.fy = focalLength.v[1];
    intrin.ppx = principalPoint.v[0];
    intrin.ppy = principalPoint.v[1];

    return true;
}

bool OpenVR_TrackedCamera::start()
{
    uint32_t frameWidth = 0;
    uint32_t frameHeight = 0;
    uint32_t framebufferSize = 0;

    intrin.width = frameWidth;
    intrin.height = frameHeight;

    if (trackedCamera->GetCameraFrameSize(vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_MaximumUndistorted, &frameWidth, &frameHeight, &framebufferSize) != vr::VRTrackedCameraError_None)
    {
        std::cout << "GetCameraFrameSize() failed" << std::endl;
        return false;
    }

    // Create a persistent buffer for holding the incoming camera data
    //frame.rawBytes = image_buffer<uint8_t, 3>(int2(frameWidth, frameHeight));

    lastFrameSequence = 0;

    // Setup openvr handle
    trackedCamera->AcquireVideoStreamingService(vr::k_unTrackedDeviceIndex_Hmd, &trackedCameraHandle);
    if (trackedCameraHandle == INVALID_TRACKED_CAMERA_HANDLE)
    {
        std::cout << "AcquireVideoStreamingService() failed" << std::endl;
        return false;
    }

    return true;
}

void OpenVR_TrackedCamera::stop()
{
    trackedCamera->ReleaseVideoStreamingService(trackedCameraHandle);
    trackedCameraHandle = INVALID_TRACKED_CAMERA_HANDLE;
}

void OpenVR_TrackedCamera::capture()
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

///////////////////////////////////
//   OpenVR HMD Implementation   //
///////////////////////////////////

OpenVR_HMD::OpenVR_HMD()
{
    vr::EVRInitError eError = vr::VRInitError_None;
    hmd = vr::VR_Init(&eError, vr::VRApplication_Scene);
    if (eError != vr::VRInitError_None) throw std::runtime_error("Unable to init VR runtime: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));

    std::cout << "VR Driver:  " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String) << std::endl;
    std::cout << "VR Display: " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String) << std::endl;

    controllerRenderData = std::make_shared<ControllerRenderData>();
    //controllers[0].renderData = controllerRenderData;
    //controllers[1].renderData = controllerRenderData;

    renderModels = (vr::IVRRenderModels *)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
    if (!renderModels)
    {
        vr::VR_Shutdown();
        throw std::runtime_error("Unable to get render model interface: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));
    }

    {
        vr::RenderModel_t * model = nullptr;
        vr::RenderModel_TextureMap_t * texture = nullptr;

        while (true)
        {
            // see VREvent_TrackedDeviceActivated below for the proper way of doing this
            renderModels->LoadRenderModel_Async("vr_controller_vive_1_5", &model);
            if (model) renderModels->LoadTexture_Async(model->diffuseTextureId, &texture);
            if (model && texture) break;
        }

        for (uint32_t v = 0; v < model->unVertexCount; v++ )
        {
            const vr::RenderModel_Vertex_t vertex = model->rVertexData[v];
            controllerRenderData->mesh.vertices.push_back({ vertex.vPosition.v[0], vertex.vPosition.v[1], vertex.vPosition.v[2] });
            controllerRenderData->mesh.normals.push_back({ vertex.vNormal.v[0], vertex.vNormal.v[1], vertex.vNormal.v[2] });
            controllerRenderData->mesh.texcoord0.push_back({ vertex.rfTextureCoord[0], vertex.rfTextureCoord[1] });
        }

        for (uint32_t f = 0; f < model->unTriangleCount * 3; f +=3)
        {
            controllerRenderData->mesh.faces.push_back({ model->rIndexData[f], model->rIndexData[f + 1] , model->rIndexData[f + 2] });
        }

        glTextureImage2DEXT(controllerRenderData->tex, GL_TEXTURE_2D, 0, GL_RGBA, texture->unWidth, texture->unHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->rubTextureMapData);
        glGenerateTextureMipmapEXT(controllerRenderData->tex, GL_TEXTURE_2D);
        glTextureParameteriEXT(controllerRenderData->tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteriEXT(controllerRenderData->tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        renderModels->FreeTexture(texture);
        renderModels->FreeRenderModel(model);

        controllerRenderData->loaded = true;
    }

    hmd->GetRecommendedRenderTargetSize(&renderTargetSize.x, &renderTargetSize.y);

    // Setup the compositor
    if (!vr::VRCompositor())
    {
        throw std::runtime_error("could not initialize VRCompositor");
    }
}

OpenVR_HMD::~OpenVR_HMD()
{
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    glDebugMessageCallback(nullptr, nullptr);
    if (hmd) vr::VR_Shutdown();
}

void OpenVR_HMD::update()
{
    // Handle events
    vr::VREvent_t event;
    while (hmd->PollNextEvent(&event, sizeof(event)))
    {
        switch (event.eventType)
        {
        
        case vr::VREvent_TrackedDeviceActivated: 
        {
            std::cout << "Device " << event.trackedDeviceIndex << " attached." << std::endl;

            if (hmd->GetTrackedDeviceClass(event.trackedDeviceIndex) == vr::TrackedDeviceClass_Controller && controllerRenderData->loaded == false)
            { 
                vr::EVRInitError eError = vr::VRInitError_None;
                std::string sRenderModelName = get_tracked_device_string(hmd, event.trackedDeviceIndex, vr::Prop_RenderModelName_String);
                std::cout << "Render Model Is: " << sRenderModelName << std::endl;
            }

            break;
        }
        case vr::VREvent_TrackedDeviceDeactivated: std::cout << "Device " << event.trackedDeviceIndex << " detached." << std::endl; break;
        case vr::VREvent_TrackedDeviceUpdated: std::cout << "Device " << event.trackedDeviceIndex << " updated." << std::endl; break;
        }
    }

    // Get HMD pose
    std::array<vr::TrackedDevicePose_t, 16> poses;
    vr::VRCompositor()->WaitGetPoses(poses.data(), static_cast<uint32_t>(poses.size()), nullptr, 0);
    for (vr::TrackedDeviceIndex_t i = 0; i < poses.size(); ++i)
    {
        if (!poses[i].bPoseIsValid) continue;
        switch (hmd->GetTrackedDeviceClass(i))
        {
        case vr::TrackedDeviceClass_HMD:
        {
            hmdPose = make_pose(poses[i].mDeviceToAbsoluteTracking); 
            break;
        }
        case vr::TrackedDeviceClass_Controller:
        {
            vr::VRControllerState_t controllerState = vr::VRControllerState_t();
            switch (hmd->GetControllerRoleForTrackedDeviceIndex(i))
            {
            case vr::TrackedControllerRole_LeftHand:
            {
                if (hmd->GetControllerState(i, &controllerState, sizeof(controllerState)))
                {
                    controllers[0].trigger.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));
                    controllers[0].pad.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));
                    controllers[0].touchpad.x = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x;
                    controllers[0].touchpad.y = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y;
                    controllers[0].set_pose(make_pose(poses[i].mDeviceToAbsoluteTracking));
                }
                break;
            }
            case vr::TrackedControllerRole_RightHand:
            {
                if (hmd->GetControllerState(i, &controllerState, sizeof(controllerState)))
                {
                    controllers[1].trigger.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));
                    controllers[1].pad.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));
                    controllers[1].touchpad.x = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x;
                    controllers[1].touchpad.y = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y;
                    controllers[1].set_pose(make_pose(poses[i].mDeviceToAbsoluteTracking));
                }
                break;
            }
            }
            break;
        }
        }
    }
}

void OpenVR_HMD::submit(const GLuint leftEye, const GLuint rightEye)
{
    const vr::Texture_t leftTex = { (void*)(intptr_t) leftEye, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::VRCompositor()->Submit(vr::Eye_Left, &leftTex);

    const vr::Texture_t rightTex = { (void*)(intptr_t) rightEye, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::VRCompositor()->Submit(vr::Eye_Right, &rightTex);

    glFlush();
}
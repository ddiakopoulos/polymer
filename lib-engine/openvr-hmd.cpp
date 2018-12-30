#include "openvr-hmd.hpp"

using namespace polymer;

std::string get_tracked_device_string(vr::IVRSystem * pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL)
{
    uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
    if (unRequiredBufferLen == 0) return "";
    std::vector<char> pchBuffer(unRequiredBufferLen);
    unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer.data(), unRequiredBufferLen, peError);
    std::string result = { pchBuffer.begin(), pchBuffer.end() };
    return result;
}

///////////////////////////////////
//   OpenVR HMD Implementation   //
///////////////////////////////////

openvr_hmd::openvr_hmd()
{
    vr::EVRInitError eError = vr::VRInitError_None;
    hmd = vr::VR_Init(&eError, vr::VRApplication_Scene);
    if (eError != vr::VRInitError_None) throw std::runtime_error("Unable to init VR runtime: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));

    std::cout << "VR Driver:  " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String) << std::endl;
    std::cout << "VR Display: " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String) << std::endl;

    hmd->GetRecommendedRenderTargetSize(&renderTargetSize.x, &renderTargetSize.y);

    // Setup the compositor
    if (!vr::VRCompositor())
    {
        throw std::runtime_error("could not initialize VRCompositor");
    }
}

openvr_hmd::~openvr_hmd()
{
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    glDebugMessageCallback(nullptr, nullptr);
    if (hmd) vr::VR_Shutdown();
}

vr_controller openvr_hmd::get_controller(vr_controller_role controller)
{
    auto openvr_controller = static_cast<vr::ETrackedControllerRole>(controller);

    if (openvr_controller == vr::TrackedControllerRole_LeftHand) return controllers[0];
    else if (openvr_controller == vr::TrackedControllerRole_RightHand) return controllers[1];
    else throw std::invalid_argument("invalid controller enum");
    return {};
}

void openvr_hmd::controller_render_data_callback(std::function<void(cached_controller_render_data & data)> callback)
{
    async_data_cb = callback;
}

gl_mesh openvr_hmd::get_stencil_mask(vr_eye eye)
{
    auto openvr_eye = static_cast<vr::Hmd_Eye>(eye);

    gl_mesh mesh;

    auto ham =  hmd->GetHiddenAreaMesh(openvr_eye, vr::k_eHiddenAreaMesh_Standard); // k_eHiddenAreaMesh_Standard k_eHiddenAreaMesh_Inverse
    assert(ham.unTriangleCount > 0);
    std::vector<float2> hidden_vertices;

    for (uint32_t i = 0; i < ham.unTriangleCount * 3; ++i)
    {
        auto v = ham.pVertexData[i].v;
        hidden_vertices.push_back(float2(v[0] * 2.f - 1.f, 1.f - 2.f * v[1]));
    }

    mesh.set_vertices(hidden_vertices.size(), hidden_vertices.data(), GL_STATIC_DRAW);
    mesh.set_attribute(0, 2, GL_FLOAT, GL_FALSE, sizeof(float2), ((float*)0));

    return std::move(mesh);
}

void openvr_hmd::set_world_pose(const transform & p) 
{
    worldPose = p; 
}

transform openvr_hmd::get_world_pose() 
{ 
    return worldPose; 
}

transform openvr_hmd::get_hmd_pose() const 
{ 
    return worldPose * hmdPose; 
}

void openvr_hmd::set_hmd_pose(const transform & p) 
{ 
    hmdPose = p; 
}

transform openvr_hmd::get_eye_pose(vr_eye eye)
{ 
    return get_hmd_pose() * make_pose(hmd->GetEyeToHeadTransform(static_cast<vr::Hmd_Eye>(eye)));
}

uint2 openvr_hmd::get_recommended_render_target_size()
{
    return renderTargetSize;
}

float4x4 openvr_hmd::get_proj_matrix(vr_eye eye, float near_clip, float far_clip)
{
    return transpose(reinterpret_cast<const float4x4 &>(hmd->GetProjectionMatrix(static_cast<vr::Hmd_Eye>(eye), near_clip, far_clip)));
}

vr_input_vendor openvr_hmd::get_input_vendor() 
{ 
    return vr_input_vendor::vive_wand; 
}

void openvr_hmd::get_optical_properties(vr_eye eye, float & aspectRatio, float & vfov)
{
    float l_left = 0.0f, l_right = 0.0f, l_top = 0.0f, l_bottom = 0.0f;
    hmd->GetProjectionRaw(vr::Hmd_Eye::Eye_Left, &l_left, &l_right, &l_top, &l_bottom);

    float r_left = 0.0f, r_right = 0.0f, r_top = 0.0f, r_bottom = 0.0f;
    hmd->GetProjectionRaw(vr::Hmd_Eye::Eye_Right, &r_left, &r_right, &r_top, &r_bottom);

    float2 tanHalfFov = float2(max(-l_left, l_right, -r_left, r_right), max(-l_top, l_bottom, -r_top, r_bottom));
    aspectRatio = tanHalfFov.x / tanHalfFov.y;
    vfov = 2.0f * std::atan(tanHalfFov.y);
}

void openvr_hmd::load_render_data_impl(vr::VREvent_t event)
{
    if (controllerRenderData.loaded == true)
    {
        async_data_cb(controllerRenderData);
        return;
    }

    if (hmd->GetTrackedDeviceClass(event.trackedDeviceIndex) == vr::TrackedDeviceClass_Controller)
    {
        vr::EVRInitError eError = vr::VRInitError_None;
        std::string sRenderModelName = get_tracked_device_string(hmd, event.trackedDeviceIndex, vr::Prop_RenderModelName_String);

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
                renderModels->LoadRenderModel_Async(sRenderModelName.c_str(), &model);
                if (model) renderModels->LoadTexture_Async(model->diffuseTextureId, &texture);
                if (model && texture) break;
            }

            for (uint32_t v = 0; v < model->unVertexCount; v++)
            {
                const vr::RenderModel_Vertex_t vertex = model->rVertexData[v];
                controllerRenderData.mesh.vertices.push_back({ vertex.vPosition.v[0], vertex.vPosition.v[1], vertex.vPosition.v[2] });
                controllerRenderData.mesh.normals.push_back({ vertex.vNormal.v[0], vertex.vNormal.v[1], vertex.vNormal.v[2] });
                controllerRenderData.mesh.texcoord0.push_back({ vertex.rfTextureCoord[0], vertex.rfTextureCoord[1] });
            }

            for (uint32_t f = 0; f < model->unTriangleCount * 3; f += 3)
            {
                controllerRenderData.mesh.faces.push_back({ model->rIndexData[f], model->rIndexData[f + 1] , model->rIndexData[f + 2] });
            }

            glTextureImage2DEXT(controllerRenderData.tex, GL_TEXTURE_2D, 0, GL_RGBA, texture->unWidth, texture->unHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->rubTextureMapData);
            glGenerateTextureMipmapEXT(controllerRenderData.tex, GL_TEXTURE_2D);
            glTextureParameteriEXT(controllerRenderData.tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteriEXT(controllerRenderData.tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

            renderModels->FreeTexture(texture);
            renderModels->FreeRenderModel(model);

            controllerRenderData.loaded = true;

            async_data_cb(controllerRenderData);
        }
    }
}

void openvr_hmd::update()
{
    vr::VREvent_t event;
    while (hmd->PollNextEvent(&event, sizeof(event)))
    {
        switch (event.eventType)
        {
        case vr::VREvent_TrackedDeviceActivated: std::cout << "OpenVR device " << event.trackedDeviceIndex << " attached." << std::endl; break;
        case vr::VREvent_TrackedDeviceDeactivated: std::cout << "OpenVR device " << event.trackedDeviceIndex << " detached." << std::endl; break;
        case vr::VREvent_TrackedDeviceUpdated: std::cout << "OpenVR device " << event.trackedDeviceIndex << " updated." << std::endl; break;
        }

        // Setup render model data if applicable
        load_render_data_impl(event);
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
                    update_button_state(controllers[0].buttons[get_button_id_for_vendor(static_cast<uint32_t>(vr::k_EButton_SteamVR_Trigger), get_input_vendor())],
                        !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));

                    update_button_state(controllers[0].buttons[get_button_id_for_vendor(static_cast<uint32_t>(vr::k_EButton_SteamVR_Touchpad), get_input_vendor())],
                        !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));

                    controllers[0].xy_values = { controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x, controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y };
                    controllers[0].t = (worldPose * make_pose(poses[i].mDeviceToAbsoluteTracking));
                }
                break;
            }
            case vr::TrackedControllerRole_RightHand:
            {
                if (hmd->GetControllerState(i, &controllerState, sizeof(controllerState)))
                {
                    update_button_state(controllers[1].buttons[get_button_id_for_vendor(static_cast<uint32_t>(vr::k_EButton_SteamVR_Trigger), get_input_vendor())],
                        !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));

                    update_button_state(controllers[1].buttons[get_button_id_for_vendor(static_cast<uint32_t>(vr::k_EButton_SteamVR_Touchpad), get_input_vendor())],
                        !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));

                    controllers[1].xy_values = { controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x, controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y };
                    controllers[1].t = (worldPose * make_pose(poses[i].mDeviceToAbsoluteTracking));
                }
                break;
            }
            }
            break;
        }
        }
    }
}

void openvr_hmd::submit(const GLuint leftEye, const GLuint rightEye)
{
    const vr::Texture_t leftTex = { (void*)(intptr_t) leftEye, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::VRCompositor()->Submit(vr::Eye_Left, &leftTex);

    const vr::Texture_t rightTex = { (void*)(intptr_t) rightEye, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::VRCompositor()->Submit(vr::Eye_Right, &rightTex);

    glFlush();
}
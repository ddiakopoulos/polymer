#include "renderer-standard.hpp"
#include "material.hpp"
#include "math-spatial.hpp"
#include "geometry.hpp"

////////////////////////////////////////////////
//   stable_cascaded_shadows implementation   //
////////////////////////////////////////////////

stable_cascaded_shadows::stable_cascaded_shadows()
{
    shadowArrayDepth.setup(GL_TEXTURE_2D_ARRAY, resolution, resolution, uniforms::NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTextureEXT(shadowArrayFramebuffer, GL_DEPTH_ATTACHMENT, shadowArrayDepth, 0);
    shadowArrayFramebuffer.check_complete();
    gl_check_error(__FILE__, __LINE__);
}

void stable_cascaded_shadows::update_cascades(const float4x4 & view, const float near, const float far, const float aspectRatio, const float vfov, const float3 & lightDir)
{
    nearPlanes.clear();
    farPlanes.clear();
    splitPlanes.clear();
    viewMatrices.clear();
    projMatrices.clear();
    shadowMatrices.clear();

    for (size_t C = 0; C < uniforms::NUM_CASCADES; ++C)
    {
        const float splitIdx = uniforms::NUM_CASCADES;

        // Find the split planes using GPU Gem 3. Chap 10 "Practical Split Scheme".
        // http://http.developer.nvidia.com/GPUGems3/gpugems3_ch10.html
        const float splitNear = C > 0 ? mix(near + (static_cast<float>(C) / splitIdx) * (far - near),
            near * pow(far / near, static_cast<float>(C) / splitIdx), splitLambda) : near;

        const float splitFar = C < splitIdx - 1 ? mix(near + (static_cast<float>(C + 1) / splitIdx) * (far - near),
            near * pow(far / near, static_cast<float>(C + 1) / splitIdx), splitLambda) : far;

        const float4x4 splitProjectionMatrix = make_projection_matrix(vfov, aspectRatio, splitNear, splitFar);

        // Extract the frustum points
        float4 splitFrustumVerts[8] = {
            { -1.f, -1.f, -1.f, 1.f }, // Near plane
            { -1.f,  1.f, -1.f, 1.f },
            { +1.f,  1.f, -1.f, 1.f },
            { +1.f, -1.f, -1.f, 1.f },
            { -1.f, -1.f,  1.f, 1.f }, // Far plane
            { -1.f,  1.f,  1.f, 1.f },
            { +1.f,  1.f,  1.f, 1.f },
            { +1.f, -1.f,  1.f, 1.f }
        };

        for (unsigned int j = 0; j < 8; ++j)
        {
            splitFrustumVerts[j] = float4(transform_coord(inverse(mul(splitProjectionMatrix, view)), splitFrustumVerts[j].xyz()), 1);
        }

        float3 frustumCentroid = float3(0, 0, 0);
        for (size_t i = 0; i < 8; ++i) frustumCentroid += splitFrustumVerts[i].xyz();
        frustumCentroid /= 8.0f;

        // Calculate the radius of a bounding sphere surrounding the frustum corners in worldspace
        // This can be precomputed if the camera frustum does not change
        float sphereRadius = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float dist = length(splitFrustumVerts[i].xyz() - frustumCentroid) * 1.0;
            sphereRadius = std::max(sphereRadius, dist);
        }

        sphereRadius = (std::ceil(sphereRadius * 32.0f) / 32.0f);

        const float3 maxExtents = float3(sphereRadius, sphereRadius, sphereRadius);
        const float3 minExtents = -maxExtents;

        const Pose cascadePose = look_at_pose_rh(frustumCentroid + lightDir * -minExtents.z, frustumCentroid);
        const float4x4 splitViewMatrix = cascadePose.view_matrix();

        const float3 cascadeExtents = maxExtents - minExtents;
        float4x4 shadowProjectionMatrix = make_orthographic_matrix(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, cascadeExtents.z);

        // Create a rounding matrix by projecting the world-space origin and determining the fractional offset in texel space
        float3 shadowOrigin = transform_coord(mul(shadowProjectionMatrix, splitViewMatrix), float3(0, 0, 0));
        shadowOrigin *= (resolution * 0.5f);

        const float4 roundedOrigin = round(float4(shadowOrigin, 1));
        float4 roundOffset = roundedOrigin - float4(shadowOrigin, 1);
        roundOffset *= 2.0f / resolution;
        roundOffset.z = 0;
        roundOffset.w = 0;
        shadowProjectionMatrix[3] += roundOffset;

        const float4x4 theShadowMatrix = mul(shadowProjectionMatrix, splitViewMatrix);

        viewMatrices.push_back(splitViewMatrix);
        projMatrices.push_back(shadowProjectionMatrix);
        shadowMatrices.push_back(theShadowMatrix);
        splitPlanes.push_back(float2(splitNear, splitFar));
        nearPlanes.push_back(-maxExtents.z);
        farPlanes.push_back(-minExtents.z);
    }
}

void stable_cascaded_shadows::pre_draw()
{
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowArrayFramebuffer);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    auto & shader = program.get()->get_variant()->shader;

    shader.bind();
    shader.uniform("u_cascadeViewMatrixArray", uniforms::NUM_CASCADES, viewMatrices);
    shader.uniform("u_cascadeProjMatrixArray", uniforms::NUM_CASCADES, projMatrices);
}

GlShader & stable_cascaded_shadows::get_program()
{
    return program.get()->get_variant()->shader;
}

void stable_cascaded_shadows::post_draw()
{
    auto & shader = program.get()->get_variant()->shader; // should this be a call to default()?
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    shader.unbind();
}

GLuint stable_cascaded_shadows::get_output_texture() const
{
    return shadowArrayDepth.id();
}

//////////////////////////////////////////
//   pbr_render_system implementation   //
//////////////////////////////////////////

void pbr_render_system::update_per_object_uniform_buffer(const Pose & p, const float3 & scale, const bool recieveShadow, const view_data & d)
{
    uniforms::per_object object = {};
    object.modelMatrix = mul(p.matrix(), make_scaling_matrix(scale));
    object.modelMatrixIT = inverse(transpose(object.modelMatrix));
    object.modelViewMatrix = mul(d.viewMatrix, object.modelMatrix);
    object.receiveShadow = static_cast<float>(recieveShadow);
    perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);
}

uint32_t pbr_render_system::get_color_texture(const uint32_t idx) const
{
    assert(idx <= settings.cameraCount);
    if (settings.tonemapEnabled)
    {
        return postTextures[idx];
    }
    return eyeTextures[idx];
}

uint32_t pbr_render_system::get_depth_texture(const uint32_t idx) const 
{
    assert(idx <= settings.cameraCount);
    return eyeDepthTextures[idx];
}

stable_cascaded_shadows * pbr_render_system::get_shadow_pass() const
{
    if (shadow) return shadow.get();
    return nullptr;
}

void pbr_render_system::run_depth_prepass(const view_data & view, const render_payload & scene)
{
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, &colorMask[0]);

    glEnable(GL_DEPTH_TEST);    // Enable depth testing
    glDepthFunc(GL_LESS);       // Nearest pixel
    glDepthMask(GL_TRUE);       // Need depth mask on
    glColorMask(0, 0, 0, 0);    // Do not write any color

    auto & shader = renderPassEarlyZ.get()->get_variant()->shader;
    shader.bind();
    for (entity e : scene.render_set)
    {
        update_per_object_uniform_buffer(obj, view);
        obj->draw();
    }
    shader.unbind();

    // Restore color mask state
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
}

void pbr_render_system::run_skybox_pass(const view_data & view, const render_payload & scene)
{
    if (!scene.skybox) return;

    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    scene.skybox->render(view.viewProjMatrix, view.pose.position, view.farClip);

    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

void pbr_render_system::run_shadow_pass(const view_data & view, const render_payload & scene)
{
    auto sunlight = directional_lights.begin()->second;

    shadow->update_cascades(view.viewMatrix,
        view.nearClip,
        view.farClip,
        aspect_from_projection(view.projectionMatrix),
        vfov_from_projection(view.projectionMatrix),
        sunlight.data.direction);

    shadow->pre_draw();

    for (entity e : scene.render_set)
    {
        if (obj->get_cast_shadow())
        {
            const float4x4 modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
            shadow->get_program().uniform("u_modelShadowMatrix", modelMatrix);
            obj->draw();
        }
    }

    shadow->post_draw();

    gl_check_error(__FILE__, __LINE__);
}

void pbr_render_system::run_forward_pass(std::vector<entity> & renderQueueMaterial, const view_data & view, const render_payload & scene)
{
    if (settings.useDepthPrepass)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // depth already comes from the prepass
    }

    for (entity e : renderQueueMaterial)
    {
        update_per_object_uniform_buffer(r, view);

        material_interface * mat = r->get_material();
        mat->update_uniforms();
        if (auto * mr = dynamic_cast<polymer_pbr_standard*>(mat))
        {
            if (settings.shadowsEnabled)
            {
                // ideally compile this out from the shader if not using shadows
                mr->update_uniforms_shadow(shadow->get_output_texture());
            }

            mr->update_uniforms_ibl(scene.ibl_irradianceCubemap.get(), scene.ibl_radianceCubemap.get());
        }
        mat->use();

        r->draw();
    }

    if (settings.useDepthPrepass)
    {
        glDepthMask(GL_TRUE); // cleanup state
    }
}

void pbr_render_system::run_post_pass(const view_data & view, const render_payload & scene)
{
    if (!settings.tonemapEnabled) return;

    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

    // Disable culling and depth testing for post processing
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, postFramebuffers[view.index]);
    glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);

    auto & shader = renderPassTonemap.get()->get_variant()->shader;
    shader.bind();
    shader.texture("s_texColor", 0, eyeTextures[view.index], GL_TEXTURE_2D);
    post_quad.draw_elements();
    shader.unbind();

    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

pbr_render_system::pbr_render_system(entity_orchestrator * orch, const renderer_settings settings) :  base_system(orch), settings(settings)
{
    register_system_for_type(this, hash(get_typename<mesh_component>()));
    register_system_for_type(this, hash(get_typename<material_component>()));
    register_system_for_type(this, hash(get_typename<point_light_component>()));
    register_system_for_type(this, hash(get_typename<directional_light_component>()));

    assert(settings.renderSize.x > 0 && settings.renderSize.y > 0);
    assert(settings.cameraCount >= 1);

    eyeFramebuffers.resize(settings.cameraCount);
    eyeTextures.resize(settings.cameraCount);
    eyeDepthTextures.resize(settings.cameraCount);

    // Generate multisample render buffers for color and depth, attach to multi-sampled framebuffer target
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0], settings.msaaSamples, GL_RGBA, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1], settings.msaaSamples, GL_DEPTH_COMPONENT, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);
    multisampleFramebuffer.check_complete();

    // Generate textures and framebuffers for |settings.cameraCount|
    for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
    {
        //GL_RGBA8
        eyeTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr, false);
        glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        // Depth tex
        eyeDepthTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glNamedFramebufferTexture2DEXT(eyeFramebuffers[camIdx], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTextures[camIdx], 0);
        glNamedFramebufferTexture2DEXT(eyeFramebuffers[camIdx], GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, eyeDepthTextures[camIdx], 0);

        eyeFramebuffers[camIdx].check_complete();
    }

    if (settings.tonemapEnabled)
    {
        postFramebuffers.resize(settings.cameraCount);
        postTextures.resize(settings.cameraCount);
        post_quad = make_fullscreen_quad();

        for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            postTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr, false);
            glTextureParameteriEXT(postTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(postTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(postTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glNamedFramebufferTexture2DEXT(postFramebuffers[camIdx], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postTextures[camIdx], 0);
            postFramebuffers[camIdx].check_complete();
        }
    }

    gl_check_error(__FILE__, __LINE__);

    // Only create shadow resources if the user has requested them. 
    if (settings.shadowsEnabled)
    {
        shadow.reset(new stable_cascaded_shadows());
    }

    // Respect performance profiling settings on construction
    gpuProfiler.set_enabled(settings.performanceProfiling);
    cpuProfiler.set_enabled(settings.performanceProfiling);

    timer.start();
}

pbr_render_system::~pbr_render_system()
{
    timer.stop();
}

void pbr_render_system::render_frame(const render_payload & scene)
{
    assert(settings.cameraCount == scene.views.size());
    assert(directional_lights.size() == 1);

    base_system * xform_base = orchestrator->get_system(get_typeid<world_transform_component>());
    transform_system * xform_system = dynamic_cast<transform_system *>(xform_base);

    cpuProfiler.begin("build-transient_renderable");
    std::vector<transient_renderable> transient_renderables(scene.render_set.size());

    for (int i = 0; i < scene.render_set.size(); ++i)
    {

    }

    for (entity e : scene.render_set)
    {
        transient_renderable
    }
    cpuProfiler.end("build-transient_renderable");

    cpuProfiler.begin("renderloop");

    // Renderer default state
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_object::binding, perObject);

    // Update per-scene uniform buffer
    uniforms::per_scene b = {};
    b.time = timer.milliseconds().count() / 1000.f; // expressed in seconds
    b.resolution = float2(settings.renderSize);
    b.invResolution = 1.f / b.resolution;
    b.activePointLights = static_cast<int>(point_lights.size());

    auto sunlight = directional_lights.begin()->second;

    b.directional_light.color = sunlight.data.color;
    b.directional_light.direction = sunlight.data.direction;
    b.directional_light.amount = sunlight.data.amount;

    assert(point_lights.size() <= uniforms::MAX_POINT_LIGHTS);

    uint32_t lightIdx = 0;
    for (auto & light : point_lights)
    {
        if (!light.second.enabled) continue;
        b.point_lights[lightIdx] = light.second.data;
        lightIdx++;
    }

    GLfloat defaultColor[] = { scene.clear_color.x, scene.clear_color.y, scene.clear_color.z, scene.clear_color.w };
    GLfloat defaultDepth = 1.f;

    view_data shadowAndCullingView = scene.views[0];

    if (settings.cameraCount == 2)
    {
        cpuProfiler.begin("center-view");

        // Take the mid-point between the eyes
        shadowAndCullingView.pose = Pose(scene.views[0].pose.orientation, (scene.views[0].pose.position + scene.views[1].pose.position) * 0.5f);

        // Compute the interocular distance
        const float3 interocularDistance = scene.views[1].pose.position - scene.views[0].pose.position;

        // Generate the superfrustum projection matrix and the value we need to move the midpoint in Z
        float3 centerOffsetZ;
        compute_center_view(scene.views[0].projectionMatrix, scene.views[1].projectionMatrix, interocularDistance.x, shadowAndCullingView.projectionMatrix, centerOffsetZ);

        // Regenerate the view matrix and near/far clip planes
        shadowAndCullingView.viewMatrix = inverse(mul(shadowAndCullingView.pose.matrix(), make_translation_matrix(centerOffsetZ)));
        near_far_clip_from_projection(shadowAndCullingView.projectionMatrix, shadowAndCullingView.nearClip, shadowAndCullingView.farClip);
        cpuProfiler.end("center-view");
    }

    if (settings.shadowsEnabled)
    {
        gpuProfiler.begin("shadowpass");
        run_shadow_pass(shadowAndCullingView, scene);
        gpuProfiler.end("shadowpass");

        for (int c = 0; c < uniforms::NUM_CASCADES; c++)
        {
            b.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
            b.cascadesMatrix[c] = shadow->shadowMatrices[c];
            b.cascadesNear[c] = shadow->nearPlanes[c];
            b.cascadesFar[c] = shadow->farPlanes[c];
        }

        gl_check_error(__FILE__, __LINE__);
    }

    // Per-scene can be uploaded now that the shadow pass has completed
    perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

    // We follow the sorting strategy outlined here: http://realtimecollisiondetection.net/blog/?p=86
    auto materialSortFunc = [xform_system, shadowAndCullingView, this](entity lhs, entity rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, xform_system->get_world_transform(lhs)->world_pose.position);
        const float rDist = distance(shadowAndCullingView.pose.position, xform_system->get_world_transform(rhs)->world_pose.position);

        // Sort by material (expensive shader state change)
        auto lid = materials[lhs].material.get()->id();
        auto rid = materials[rhs].material.get()->id();
        if (lid != rid) return lid > rid;

        // Otherwise sort by distance
        return lDist < rDist;
    };

    auto distanceSortFunc = [xform_system, shadowAndCullingView](entity lhs, entity rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, xform_system->get_world_transform(lhs)->world_pose.position);
        const float rDist = distance(shadowAndCullingView.pose.position, xform_system->get_world_transform(rhs)->world_pose.position);
        return lDist < rDist;
    };

    cpuProfiler.begin("push-queue");
    std::priority_queue<entity, std::vector<entity>, decltype(materialSortFunc)> renderQueueMaterial(materialSortFunc);
    for (const entity ent : scene.render_set) { renderQueueMaterial.push(ent); }
    cpuProfiler.end("push-queue");

    cpuProfiler.begin("flatten-queue");
    // Resolve render queues into flat lists
    std::vector<entity> materialRenderList;
    while (!renderQueueMaterial.empty())
    {
        entity top = renderQueueMaterial.top();
        renderQueueMaterial.pop();
        materialRenderList.push_back(top);
    }

    cpuProfiler.end("flatten-queue");

    for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
    {
        // Update per-view uniform buffer
        uniforms::per_view v = {};
        v.view = scene.views[camIdx].viewMatrix;
        v.viewProj = scene.views[camIdx].viewProjMatrix;
        v.eyePos = float4(scene.views[camIdx].pose.position, 1);
        perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

        // Render into multisampled fbo
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
        glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

        gl_check_error(__FILE__, __LINE__);

        // Execute the forward passes
        if (settings.useDepthPrepass)
        {
            gpuProfiler.begin("depth-prepass");
            run_depth_prepass(scene.views[camIdx], scene);
            gpuProfiler.end("depth-prepass");
        }

        gl_check_error(__FILE__, __LINE__);

        gpuProfiler.begin("forward-pass");
        cpuProfiler.begin("skybox");
        run_skybox_pass(scene.views[camIdx], scene);
        cpuProfiler.end("skybox");
        cpuProfiler.begin("forward");
        run_forward_pass(materialRenderList, scene.views[camIdx], scene);
        cpuProfiler.end("forward");
        gpuProfiler.end("forward-pass");

        gl_check_error(__FILE__, __LINE__);

        glDisable(GL_MULTISAMPLE);

        // Resolve multisample into per-view framebuffer
        {
            gpuProfiler.begin("blit");

            // blit color 
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // blit depth
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            gpuProfiler.end("blit");
        }

        gl_check_error(__FILE__, __LINE__);
    }

    // Execute the post passes after having resolved the multisample framebuffers
    {
        gpuProfiler.begin("postprocess");
        cpuProfiler.begin("post");
        for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            run_post_pass(scene.views[camIdx], scene);
        }
        cpuProfiler.end("post");
        gpuProfiler.end("postprocess");
    }

    glDisable(GL_FRAMEBUFFER_SRGB);

    cpuProfiler.end("renderloop");

    gl_check_error(__FILE__, __LINE__);
}

bool pbr_render_system::create(entity e, poly_typeid hash, void * data)
{

}

void pbr_render_system::destroy(entity e)
{

}
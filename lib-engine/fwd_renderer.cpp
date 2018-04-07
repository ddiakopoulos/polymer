#include "fwd_renderer.hpp"
#include "material.hpp"
#include "math-spatial.hpp"
#include "geometry.hpp"

// Update per-object uniform buffer
void forward_renderer::update_per_object_uniform_buffer(Renderable * r, const view_data & d)
{
    uniforms::per_object object = {};
    object.modelMatrix = mul(r->get_pose().matrix(), make_scaling_matrix(r->get_scale()));
    object.modelMatrixIT = inverse(transpose(object.modelMatrix));
    object.modelViewMatrix = mul(d.viewMatrix, object.modelMatrix);
    object.receiveShadow = (float)r->get_receive_shadow();
    perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);
}

uint32_t forward_renderer::get_color_texture(const uint32_t idx) const
{
    assert(idx <= settings.cameraCount);
    if (settings.tonemapEnabled)
    {
        return postTextures[idx];
    }
    return eyeTextures[idx];
}

uint32_t forward_renderer::get_depth_texture(const uint32_t idx) const 
{
    assert(idx <= settings.cameraCount);
    return eyeDepthTextures[idx];
}

stable_cascaded_shadows * forward_renderer::get_shadow_pass() const
{
    if (shadow) return shadow.get();
    return nullptr;
}

void forward_renderer::run_depth_prepass(const view_data & view, const render_payload & scene)
{
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, &colorMask[0]);

    glEnable(GL_DEPTH_TEST);    // Enable depth testing
    glDepthFunc(GL_LESS);       // Nearest pixel
    glDepthMask(GL_TRUE);       // Need depth mask on
    glColorMask(0, 0, 0, 0);    // Do not write any color

    auto & shader = earlyZPass.get();
    shader.bind();
    for (auto obj : scene.renderSet)
    {
        update_per_object_uniform_buffer(obj, view);
        obj->draw();
    }
    shader.unbind();

    // Restore color mask state
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
}

void forward_renderer::run_skybox_pass(const view_data & view, const render_payload & scene)
{
    if (!scene.skybox) return;

    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    /* fixme
    auto & program = GlShaderHandle("ibl").get();
    program.bind();
    program.uniform("u_mvp", mul(view.projectionMatrix, rotation_matrix(qconj(view.pose.orientation))));
    program.texture("sc_ibl", 0, GlTextureHandle("wells-radiance-cubemap").get(), GL_TEXTURE_CUBE_MAP);
    GlMeshHandle("cube").get().draw_elements();
    program.unbind();
    */

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    scene.skybox->render(view.viewProjMatrix, view.pose.position, view.farClip);

    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

void forward_renderer::run_shadow_pass(const view_data & view, const render_payload & scene)
{
    shadow->update_cascades(view.viewMatrix,
        view.nearClip,
        view.farClip,
        aspect_from_projection(view.projectionMatrix),
        vfov_from_projection(view.projectionMatrix),
        scene.sunlight.direction);

    shadow->pre_draw();

    for (Renderable * obj : scene.renderSet)
    {
        if (obj->get_cast_shadow())
        {
            float4x4 modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
            shadow->get_program().uniform("u_modelShadowMatrix", modelMatrix);
            obj->draw();
        }
    }

    shadow->post_draw();

    gl_check_error(__FILE__, __LINE__);
}

void forward_renderer::run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const view_data & view, const render_payload & scene)
{
    if (settings.useDepthPrepass)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // depth already comes from the prepass
    }

    for (auto r : renderQueueMaterial)
    {
        update_per_object_uniform_buffer(r, view);

        Material * mat = r->get_material();
        mat->update_uniforms();
        if (auto * mr = dynamic_cast<MetallicRoughnessMaterial*>(mat))
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

    // We assume that objects without a valid material take care of their own shading in the `draw()` function. 
    for (auto r : renderQueueDefault)
    {
        update_per_object_uniform_buffer(r, view);
        r->draw();
    }

    if (settings.useDepthPrepass)
    {
        glDepthMask(GL_TRUE); // cleanup state
    }
}

void forward_renderer::run_post_pass(const view_data & view, const render_payload & scene)
{
    if (!settings.tonemapEnabled) return;

    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

    // Disable culling and depth testing for post processing
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, postFramebuffers[view.index]);
    glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);

    auto & tonemapProgram = hdr_tonemapShader.get();
    tonemapProgram.bind();
    tonemapProgram.texture("s_texColor", 0, eyeTextures[view.index], GL_TEXTURE_2D);
    post_quad.draw_elements();
    tonemapProgram.unbind();

    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

forward_renderer::forward_renderer(const renderer_settings settings) : settings(settings)
{
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

    // Generate textures and framebuffers for `settings.cameraCount`
    for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
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

        for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
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

forward_renderer::~forward_renderer()
{
    timer.stop();
}

void forward_renderer::render_frame(const render_payload & scene)
{
    assert(settings.cameraCount == scene.views.size());

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
    b.activePointLights = scene.pointLights.size();

    b.directional_light.color = scene.sunlight.color;
    b.directional_light.direction = scene.sunlight.direction;
    b.directional_light.amount = scene.sunlight.amount;
    for (int i = 0; i < (int) std::min(scene.pointLights.size(), size_t(uniforms::MAX_POINT_LIGHTS)); ++i) b.point_lights[i] = scene.pointLights[i];

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
    }

    // Per-scene can be uploaded now that the shadow pass has completed
    perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

    // We follow the sorting strategy outlined here: http://realtimecollisiondetection.net/blog/?p=86
    auto materialSortFunc = [shadowAndCullingView](Renderable * lhs, Renderable * rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, lhs->get_pose().position);
        const float rDist = distance(shadowAndCullingView.pose.position, rhs->get_pose().position);

        // Sort by material (expensive shader state change)
        auto lid = lhs->get_material()->id();
        auto rid = rhs->get_material()->id();
        if (lid != rid) return lid > rid;

        // Otherwise sort by distance
        return lDist < rDist;
    };

    auto distanceSortFunc = [shadowAndCullingView](Renderable * lhs, Renderable * rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, lhs->get_pose().position);
        const float rDist = distance(shadowAndCullingView.pose.position, rhs->get_pose().position);
        return lDist < rDist;
    };

    std::priority_queue<Renderable *, std::vector<Renderable*>, decltype(materialSortFunc)> renderQueueMaterial(materialSortFunc);
    std::priority_queue<Renderable *, std::vector<Renderable*>, decltype(distanceSortFunc)> renderQueueDefault(distanceSortFunc);

    cpuProfiler.begin("push-queue");
    for (auto obj : scene.renderSet)
    {
        // Can't sort by material if the renderable doesn't *have* a material; bucket all other objects 
        if (obj->get_material() != nullptr) renderQueueMaterial.push(obj);
        else renderQueueDefault.push(obj);
    }
    cpuProfiler.end("push-queue");

    cpuProfiler.begin("flatten-queue");
    // Resolve render queues into flat lists
    std::vector<Renderable *> materialRenderList;
    while (!renderQueueMaterial.empty())
    {
        Renderable * top = renderQueueMaterial.top();
        renderQueueMaterial.pop();
        materialRenderList.push_back(top);
    }

    std::vector<Renderable *> defaultRenderList;
    while (!renderQueueDefault.empty())
    {
        Renderable * top = renderQueueDefault.top();
        renderQueueDefault.pop();
        defaultRenderList.push_back(top);
    }
    cpuProfiler.end("flatten-queue");

    for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
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

        // Execute the forward passes
        if (settings.useDepthPrepass)
        {
            gpuProfiler.begin("depth-prepass");
            run_depth_prepass(scene.views[camIdx], scene);
            gpuProfiler.end("depth-prepass");
        }

        gpuProfiler.begin("forward-pass");
        cpuProfiler.begin("skybox");
        run_skybox_pass(scene.views[camIdx], scene);
        cpuProfiler.end("skybox");
        cpuProfiler.begin("forward");
        run_forward_pass(materialRenderList, defaultRenderList, scene.views[camIdx], scene);
        cpuProfiler.end("forward");
        gpuProfiler.end("forward-pass");

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
        for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
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
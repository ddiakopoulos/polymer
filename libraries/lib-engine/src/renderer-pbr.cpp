#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/geometry.hpp"

#include "polymer-engine/material.hpp"
#include "polymer-engine/system/system-render.hpp"
#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"

using namespace polymer;

////////////////////////////////////////////////
//   stable_cascaded_shadows implementation   //
////////////////////////////////////////////////

stable_cascaded_shadows::stable_cascaded_shadows()
{
    const GLsizei size = static_cast<GLsizei>(resolution);
    shadowArrayDepth.setup(GL_TEXTURE_2D_ARRAY, size, size, uniforms::NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture(shadowArrayFramebuffer, GL_DEPTH_ATTACHMENT, shadowArrayDepth, 0);
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
            splitFrustumVerts[j] = float4(transform_coord(inverse((splitProjectionMatrix * view)), splitFrustumVerts[j].xyz), 1);
        }

        float3 frustumCentroid = float3(0, 0, 0);
        for (size_t i = 0; i < 8; ++i) frustumCentroid += splitFrustumVerts[i].xyz;
        frustumCentroid /= 8.0f;

        // Calculate the radius of a bounding sphere surrounding the frustum corners in worldspace
        // This can be precomputed if the camera frustum does not change
        float sphereRadius = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float dist = length(splitFrustumVerts[i].xyz - frustumCentroid) * 1.0f;
            sphereRadius = std::max(sphereRadius, dist);
        }

        sphereRadius = (std::ceil(sphereRadius * 16.0f) / 16.0f);

        const float3 maxExtents = float3(sphereRadius, sphereRadius, sphereRadius);
        const float3 minExtents = -maxExtents;

        const transform cascadePose = lookat_rh(frustumCentroid + lightDir * -minExtents.z, frustumCentroid);
        const float4x4 splitViewMatrix = cascadePose.view_matrix();

        const float3 cascadeExtents = maxExtents - minExtents;
        float4x4 shadowProjectionMatrix = make_orthographic_matrix(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, cascadeExtents.z);

        // Create a rounding matrix by projecting the world-space origin and determining the fractional offset in texel space
        float3 shadowOrigin = transform_coord((shadowProjectionMatrix * splitViewMatrix), float3(0, 0, 0));
        shadowOrigin *= (resolution * 0.5f);

        const float4 roundedOrigin = round(float4(shadowOrigin, 1));
        float4 roundOffset = roundedOrigin - float4(shadowOrigin, 1);
        roundOffset *= 2.0f / resolution;
        roundOffset.z = 0;
        roundOffset.w = 0;
        shadowProjectionMatrix[3] += roundOffset;

        const float4x4 theShadowMatrix = (shadowProjectionMatrix * splitViewMatrix);

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
    glViewport(0, 0, static_cast<GLsizei>(resolution), static_cast<GLsizei>(resolution));
    glClear(GL_DEPTH_BUFFER_BIT);

    auto & shader = program.get()->get_variant()->shader;

    shader.bind();
    shader.uniform("u_cascadeViewMatrixArray", uniforms::NUM_CASCADES, viewMatrices);
    shader.uniform("u_cascadeProjMatrixArray", uniforms::NUM_CASCADES, projMatrices);
}

void stable_cascaded_shadows::update_shadow_matrix(const float4x4 & shadowModelMatrix)
{
    auto & shader = program.get()->get_variant()->shader;
    shader.uniform("u_modelShadowMatrix", shadowModelMatrix);
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

/////////////////////////////////////
//   pbr_renderer implementation   //
/////////////////////////////////////

void pbr_renderer::update_per_object_uniform_buffer(const float4x4 & model_matrix, const bool recieveShadow, const view_data & d)
{
    uniforms::per_object object = {};
    object.modelMatrix = model_matrix;
    object.modelMatrixIT = inverse(transpose(object.modelMatrix));
    object.modelViewMatrix = d.viewMatrix * object.modelMatrix;
    object.receiveShadow = static_cast<float>(recieveShadow);
    perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);
}

void pbr_renderer::run_stencil_prepass(const view_data & view, const render_payload & scene)
{
    gl_check_error(__FILE__, __LINE__);

    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, &colorMask[0]);
    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean wasBlendingEnabled = glIsEnabled(GL_BLEND);

    glColorMask(0, 0, 0, 0);                                // do not write color
    glDepthMask(GL_FALSE);                                  // do not write depth
    glStencilMask(GL_TRUE);                                 // only write stencil

    glDisable(GL_BLEND);                                    // 0 into alpha
    glDisable(GL_DEPTH_TEST);                               // disable depth
    glDisable(GL_CULL_FACE);                                // do not cull since winding will might be flipped per-eye
    glEnable(GL_STENCIL_TEST);                              // enable stencil test

    glStencilFunc(GL_ALWAYS, 1, 1);                         // set stencil func
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);        // set stencil op (GL_REPLACE, GL_REPLACE, GL_REPLACE)

    auto & shader = no_op.get()->get_variant()->shader;
    shader.bind();
    if (view.index == 0) left_stencil_mask.draw_elements();
    else if (view.index == 1) right_stencil_mask.draw_elements();
    shader.unbind();

    glStencilFunc(GL_EQUAL, 0, 1);                          // reset stencil func
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);                 // reset stencil op

    glDepthMask(GL_TRUE);                                   // it's ok to write depth
    glStencilMask(GL_FALSE);                                // no other passes should write to stencil
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]); // Restore color mask state
    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
    if (wasBlendingEnabled) glEnable(GL_BLEND);

    gl_check_error(__FILE__, __LINE__);
}

void pbr_renderer::set_stencil_mask(const uint32_t idx, gl_mesh && m)
{
    if (!m.has_data()) return; // oculus sdk doesn't provide one through OpenVR
    if (idx == 0) left_stencil_mask = std::move(m);
    else if (idx == 1) right_stencil_mask = std::move(m);
    else throw std::invalid_argument("invalid index");
    using_stencil_mask = true;
}

uint32_t pbr_renderer::get_color_texture(const uint32_t idx) const
{
    assert(idx <= settings.cameraCount);
    if (settings.tonemapEnabled)
    {
        return postTextures[idx];
    }
    return eyeTextures[idx];
}

uint32_t pbr_renderer::get_depth_texture(const uint32_t idx) const 
{
    assert(idx <= settings.cameraCount);
    return eyeDepthTextures[idx];
}

stable_cascaded_shadows * pbr_renderer::get_shadow_pass() const
{
    if (shadow) return shadow.get();
    return nullptr;
}

void pbr_renderer::run_depth_prepass(const view_data & view, const render_payload & scene)
{
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, &colorMask[0]);

    glEnable(GL_DEPTH_TEST);    // Enable depth testing
    glDepthFunc(GL_LESS);       // Nearest pixel
    glDepthMask(GL_TRUE);       // Need depth mask on
    glColorMask(0, 0, 0, 0);    // Do not write any color

    auto & shader = renderPassEarlyZ.get()->get_variant()->shader;
    shader.bind();

    for (const render_component & r : scene.render_components)
    {
        update_per_object_uniform_buffer(r.world_matrix, r.material->receive_shadow, view);
        r.mesh->draw();
    }

    shader.unbind();

    // Restore color mask state
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
}

void pbr_renderer::run_skybox_pass(const view_data & view, const render_payload & scene)
{
    if (!scene.procedural_skybox) return;

    const GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    if (scene.procedural_skybox)
    {
        scene.procedural_skybox->sky.render(view.viewProjMatrix, view.pose.position, view.farClip);
    }

    if (scene.ibl_cubemap)
    {
        // This will draw order the procedural skybox
        if (scene.ibl_cubemap->force_draw)
        {
            auto & cubemapProgram = renderPassCubemap.get()->get_variant()->shader;
            cubemapProgram.bind();
            cubemapProgram.uniform("u_mvp", (view.projectionMatrix * make_rotation_matrix(conjugate(view.pose.orientation))));
            cubemapProgram.texture("sc_ibl", 0, scene.ibl_cubemap->ibl_radianceCubemap.get(), GL_TEXTURE_CUBE_MAP);
            cubemap_box.draw_elements();
            cubemapProgram.unbind();
        }
    }

    glDepthMask(GL_TRUE);
    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

void pbr_renderer::run_shadow_pass(const view_data & view, const render_payload & scene)
{
    shadow->update_cascades(view.viewMatrix,
        view.nearClip,
        view.farClip,
        aspect_from_projection(view.projectionMatrix),
        vfov_from_projection(view.projectionMatrix),
        scene.sunlight->data.direction);

    shadow->pre_draw();

    for (const render_component & r : scene.render_components)
    {
        if (r.material->material.get()->cast_shadows)
        {
            // const float4x4 modelMatrix = (r.world_transform->world_pose.matrix() * make_scaling_matrix(r.local_transform->local_scale));
            shadow->update_shadow_matrix(r.world_matrix);
            r.mesh->draw();
        }
    }

    shadow->post_draw();

    gl_check_error(__FILE__, __LINE__);
}

void pbr_renderer::run_forward_pass(std::vector<const render_component *> & render_queue, const view_data & view, const render_payload & scene)
{
    if (settings.useDepthPrepass)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // depth already comes from the prepass
    }

    for (const render_component * render_comp : render_queue)
    {
        update_per_object_uniform_buffer(
            render_comp->world_matrix,
            render_comp->material->receive_shadow, 
            view);

        base_material * the_material = render_comp->material->material.get().get();

        // Debugging -
        // if (view.index == 0)
        // {
        //     std::cout << ">> material:  " << render_comp->get_entity() << " : " << render_comp->material->material.name << std::endl;
        //     std::cout << "\t >> shader: " << the_material->shader.name << std::endl;
        // }

        // @todo - handle other specific material requirements here
        if (auto * mr = dynamic_cast<polymer_pbr_standard*>(the_material))
        {
            if (settings.shadowsEnabled) mr->update_uniforms_shadow(shadow->get_output_texture());
            if (scene.ibl_cubemap)
            {
                mr->update_uniforms_ibl(scene.ibl_cubemap->ibl_irradianceCubemap.get(),
                    scene.ibl_cubemap->ibl_radianceCubemap.get(),
                    dfg_lut.id());
            }
        }

        if (auto * mr = dynamic_cast<polymer_blinn_phong_standard*>(the_material))
        {
            if (settings.shadowsEnabled) mr->update_uniforms_shadow(shadow->get_output_texture());
        }

        the_material->update_uniforms(render_comp->material);
        the_material->use();
        render_comp->mesh->draw();
    }

    if (settings.useDepthPrepass)
    {
        glDepthMask(GL_TRUE); // cleanup state
    }
}

void pbr_renderer::run_particle_pass(const view_data & view, const render_payload & scene)
{
    if (!scene.particle_systems.size()) return;

    glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

    auto & particle_shader = renderPassParticle.get()->get_variant()->shader;

	for (auto & system : scene.particle_systems)
	{
		system->draw(view.viewMatrix, view.projectionMatrix, particle_shader, (view.index == 1) ? true : false);
	}
}

void pbr_renderer::run_post_pass(const view_data & view, const render_payload & scene)
{
    if (!settings.tonemapEnabled) return;

    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, postFramebuffers[view.index]);
    glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);

    auto & shader = renderPassTonemap.get()->get_variant()->shader;
    shader.bind();
    shader.uniform("u_exposure", settings.exposure);
    shader.uniform("u_gamma", settings.gamma);
    shader.uniform("u_tonemapMode", settings.tonemapMode);
    shader.texture("s_texColor", 0, eyeTextures[view.index], GL_TEXTURE_2D);
    post_quad.draw_elements();
    shader.unbind();

    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

pbr_renderer::pbr_renderer(const renderer_settings settings) : settings(settings)
{
    assert(settings.renderSize.x > 0 && settings.renderSize.y > 0);
    assert(settings.cameraCount >= 1);

    post_quad = make_fullscreen_quad();
    cubemap_box = make_cube_mesh();

    eyeFramebuffers.resize(settings.cameraCount);
    eyeTextures.resize(settings.cameraCount);
    eyeDepthTextures.resize(settings.cameraCount);

    // Generate multisample render buffers for color and depth, attach to multi-sampled framebuffer target
    glNamedRenderbufferStorageMultisample(multisampleRenderbuffers[0], settings.msaaSamples, GL_RGBA16F, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbuffer(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
    glNamedRenderbufferStorageMultisample(multisampleRenderbuffers[1], settings.msaaSamples, GL_DEPTH24_STENCIL8, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbuffer(multisampleFramebuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);

    multisampleFramebuffer.check_complete();

    // Generate textures and framebuffers for |settings.cameraCount|
    for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
    {
        // Depth
        eyeDepthTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        // Color (16-bit float for HDR precision, eliminates banding on smooth gradients)
        eyeTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr, false);
        glTextureParameteri(eyeTextures[camIdx], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(eyeTextures[camIdx], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(eyeTextures[camIdx], GL_TEXTURE_MAX_LEVEL, 0);

        // Attachments
        glNamedFramebufferTexture(eyeFramebuffers[camIdx], GL_COLOR_ATTACHMENT0, eyeTextures[camIdx], 0);
        glNamedFramebufferTexture(eyeFramebuffers[camIdx], GL_DEPTH_ATTACHMENT, eyeDepthTextures[camIdx], 0);

        eyeFramebuffers[camIdx].check_complete();
    }

    if (settings.tonemapEnabled)
    {
        postFramebuffers.resize(settings.cameraCount);
        postTextures.resize(settings.cameraCount);

        for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            postTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr, false);
            glTextureParameteri(postTextures[camIdx], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(postTextures[camIdx], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(postTextures[camIdx], GL_TEXTURE_MAX_LEVEL, 0);
            glNamedFramebufferTexture(postFramebuffers[camIdx], GL_COLOR_ATTACHMENT0, postTextures[camIdx], 0);
            postFramebuffers[camIdx].check_complete();
        }
    }

    gl_check_error(__FILE__, __LINE__);

    // Only create shadow resources if the user has requested them.
    if (settings.shadowsEnabled)
    {
        shadow.reset(new stable_cascaded_shadows());
    }

    // Generate DFG LUT for IBL specular
    dfg_lut = generate_dfg_lut(128);
    gl_check_error(__FILE__, __LINE__);

    // Respect performance profiling settings on construction
    gpuProfiler.set_enabled(settings.performanceProfiling);
    cpuProfiler.set_enabled(settings.performanceProfiling);

    timer.start();
}

pbr_renderer::~pbr_renderer()
{
    timer.stop();
}

void pbr_renderer::render_frame(const render_payload & scene)
{
    assert(settings.cameraCount == scene.views.size());

    // @fixme - refactor to make optional
    auto validate_materials = [this, scene]()
    {
        for (const auto & render_comp : scene.render_components)
        {
            // In some workflows, it's useful to hand edit scene files and materials
            // but this often results in a typo, forgotten copy-and-paste, or other
            // types of human-introduced issues. Formerly, we would crash below
            // when trying to sort materials, but this is now an explicit check. 
            if (!render_comp.material->material.get())
            {
                std::cout << "[pbr_renderer] material was not assigned - " << render_comp.material->material.name << std::endl;
                throw std::runtime_error("unassigned material"); // could also possibly assign the default one
            }
        }
    };

    validate_materials();

    cpuProfiler.begin("render_frame");

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
    b.activePointLights = static_cast<int>(scene.point_lights.size());
    b.sunlightActive = 0;

    if (scene.sunlight)
    {
        b.sunlightActive = 1;
        b.directional_light.color = scene.sunlight->data.color;
        b.directional_light.direction = scene.sunlight->data.direction;
        b.directional_light.amount = scene.sunlight->data.amount;
    }

    assert(scene.point_lights.size() <= uniforms::MAX_POINT_LIGHTS);
    uint32_t lightIdx = 0;
    for (auto & light : scene.point_lights)
    {
        if (!light->enabled) continue;
        b.point_lights[lightIdx] = light->data;
        lightIdx++;
    }

    GLfloat defaultColor[] = { scene.clear_color.x, scene.clear_color.y, scene.clear_color.z, scene.clear_color.w };
    GLfloat defaultDepth = 1.f;
    GLuint  defaultStencil = 0;

    view_data shadowAndCullingView = scene.views[0];

    // For stereo rendering, we project the shadows from a center view frustum combining both eyes
    if (settings.cameraCount == 2)
    {
        // Take the mid-point between the eyes
        shadowAndCullingView.pose = transform(scene.views[0].pose.orientation, (scene.views[0].pose.position + scene.views[1].pose.position) * 0.5f);

        // Compute the interocular distance
        const float3 interocularDistance = scene.views[1].pose.position - scene.views[0].pose.position;

        // Generate the superfrustum projection matrix and the value we need to move the midpoint in Z
        float3 centerOffsetZ;
        compute_center_view(scene.views[0].projectionMatrix, scene.views[1].projectionMatrix, interocularDistance.x, shadowAndCullingView.projectionMatrix, centerOffsetZ);

        // Regenerate the view matrix and near/far clip planes
        shadowAndCullingView.viewMatrix = inverse((shadowAndCullingView.pose.matrix() * make_translation_matrix(centerOffsetZ)));
        near_far_clip_from_projection(shadowAndCullingView.projectionMatrix, shadowAndCullingView.nearClip, shadowAndCullingView.farClip);
    }

    // Shadow pass can only run if we've configured a directional sunlight

    if (settings.shadowsEnabled && scene.sunlight)
    {
        cpuProfiler.begin("run_shadow_pass");
        gpuProfiler.begin("run_shadow_pass");
        run_shadow_pass(shadowAndCullingView, scene);
        gpuProfiler.end("run_shadow_pass");
        cpuProfiler.end("run_shadow_pass");

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
    //auto materialSortFunc = [this, scene, shadowAndCullingView](const render_component * lhs, const render_component * rhs)
    //{
    //    // Sort by material (expensive shader state change)
    //    // @fixme - why is this so expensive? Adds an additional ~2ms to the sort
    //    auto lid = lhs->material->material.get()->id();
    //    auto rid = rhs->material->material.get()->id();
    //    if (lid != rid) return lid < rid;
    //
    //    // Otherwise sort by distance
    //    const float lDist = distance(shadowAndCullingView.pose.position, lhs->world_transform->world_pose.position);
    //    const float rDist = distance(shadowAndCullingView.pose.position, rhs->world_transform->world_pose.position);
    //    return lDist < rDist;
    //};

    cpuProfiler.begin("sort-render_queue_material");

    std::vector<const render_component *> render_queue(scene.render_components.size());
    auto render_priority_sort = [this, scene, shadowAndCullingView](const render_component * lhs, const render_component * rhs)
    {
        const uint32_t lso = lhs->render_sort_order;
        const uint32_t rso = rhs->render_sort_order;
        return lso < rso;
    };

    for (int i = 0; i < scene.render_components.size(); ++i) 
    { 
        render_queue[i] = (&(scene.render_components[i]));
    }
    std::sort(render_queue.begin(), render_queue.end(), render_priority_sort);

    cpuProfiler.end("sort-render_queue_material");

    for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
    {
        // Update per-view uniform buffer
        uniforms::per_view v = {};
        v.view = scene.views[camIdx].viewMatrix;
        v.viewProj = scene.views[camIdx].viewProjMatrix;
        v.eyePos = float4(scene.views[camIdx].pose.position, 1);

        const float one_minus_far_near = 1.0f - scene.views[camIdx].farClip / scene.views[camIdx].nearClip;
        const float far_near = scene.views[camIdx].farClip / scene.views[camIdx].nearClip;

        // x = 1-far/near, y = far/near, z = x/far, w = y/far
        v.zBufferParams = float4(one_minus_far_near, far_near, one_minus_far_near / scene.views[camIdx].farClip, far_near / scene.views[camIdx].farClip);

        // x = 1 or -1 (-1 if projection is flipped), y = near plane, z = far plane, w = 1/far plane
        v.projectionParams = float4(1, scene.views[camIdx].nearClip, scene.views[camIdx].farClip, 1.f / scene.views[camIdx].farClip);

        perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

        // Render into multisampled fbo
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
        glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);
        if (using_stencil_mask) glClearNamedFramebufferuiv(multisampleFramebuffer, GL_STENCIL, 0, &defaultStencil);

        if (settings.useDepthPrepass)
        {
            gpuProfiler.begin("depth-prepass-" + std::to_string(camIdx));
            run_depth_prepass(scene.views[camIdx], scene);
            gpuProfiler.end("depth-prepass-" + std::to_string(camIdx));
        }

        // Hidden area mesh for stereo rendering with openvr
        if (using_stencil_mask)
        {
            cpuProfiler.begin("run_stencil_prepass-" + std::to_string(camIdx));
            gpuProfiler.begin("run_stencil_prepass-" + std::to_string(camIdx));
            run_stencil_prepass(scene.views[camIdx], scene);
            gpuProfiler.end("run_stencil_prepass-" + std::to_string(camIdx));
            cpuProfiler.end("run_stencil_prepass-" + std::to_string(camIdx));
        }

        // Execute the forward passes
        gpuProfiler.begin("run_skybox_pass-" + std::to_string(camIdx));
        cpuProfiler.begin("run_skybox_pass-" + std::to_string(camIdx));
        run_skybox_pass(scene.views[camIdx], scene);
        cpuProfiler.end("run_skybox_pass-" + std::to_string(camIdx));
        gpuProfiler.end("run_skybox_pass-" + std::to_string(camIdx));

        gpuProfiler.begin("run_forward_pass-" + std::to_string(camIdx));
        cpuProfiler.begin("run_forward_pass-" + std::to_string(camIdx));
        run_forward_pass(render_queue, scene.views[camIdx], scene);
        cpuProfiler.end("run_forward_pass-" + std::to_string(camIdx));
        gpuProfiler.end("run_forward_pass-" + std::to_string(camIdx));

        gpuProfiler.begin("run_particle_pass-" + std::to_string(camIdx));
        cpuProfiler.begin("run_particle_pass-" + std::to_string(camIdx));
        run_particle_pass(scene.views[camIdx], scene);
        cpuProfiler.end("run_particle_pass-" + std::to_string(camIdx));
        gpuProfiler.end("run_particle_pass-" + std::to_string(camIdx));

        glDisable(GL_MULTISAMPLE);

        // Resolve multisample into per-view framebuffer
        {
            gpuProfiler.begin("blit-" + std::to_string(camIdx));

            // blit color 
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // blit depth
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            gpuProfiler.end("blit-" + std::to_string(camIdx));
        }
    }

    // Execute the post passes after having resolved the multisample framebuffers
    {
        gpuProfiler.begin("run_post_pass");
        cpuProfiler.begin("run_post_pass");
        for (uint32_t camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            run_post_pass(scene.views[camIdx], scene);
        }
        cpuProfiler.end("run_post_pass");
        gpuProfiler.end("run_post_pass");
    }

    glDisable(GL_FRAMEBUFFER_SRGB);
    cpuProfiler.end("render_frame");

    gl_check_error(__FILE__, __LINE__);
}
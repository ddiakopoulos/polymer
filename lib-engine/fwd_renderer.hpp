#pragma once

#ifndef polymer_renderer_hpp
#define polymer_renderer_hpp

#include "math-core.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "circular_buffer.hpp"
#include "human_time.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"
#include "profiling.hpp"
#include "scene.hpp"

using namespace polymer;

#undef near
#undef far

////////////////////////////////////////
//   Stable Cascaded Shadow Mapping   //
////////////////////////////////////////

class stable_cascaded_shadows
{
    /*
    * To Do - 3.25.2017
    * [X] Set number of cascades used at compile time (default 2)
    * [X] Configurable filtering modes (ESM, PCF, PCSS + PCF)
    * [ ] Frustum depth-split is a good candidate for compute shader experimentation (default far-near/4)
    * [ ] Blending / overlap between cascades
    * [ ] Performance profiling
    * - http://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
    * - https://www.gamedev.net/forums/topic/497259-stable-cascaded-shadow-maps/
    * - https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MeshRenderer.cpp
    * - http://the-witness.net/news/2010/03/graphics-tech-shadow-maps-part-1/
    * - https://chetanjags.wordpress.com/2015/02/05/real-time-shadows-cascaded-shadow-maps/
    */ 

    GlTexture3D shadowArrayDepth;
    GlFramebuffer shadowArrayFramebuffer;
    shader_handle program = { "cascaded-shadows" };

public:

    float resolution = 4096;        // cascade resolution
    float splitLambda = 0.675f;     // frustum split constant

    std::vector<float2> splitPlanes;
    std::vector<float> nearPlanes;
    std::vector<float> farPlanes;

    std::vector<float4x4> viewMatrices;
    std::vector<float4x4> projMatrices;
    std::vector<float4x4> shadowMatrices;

    stable_cascaded_shadows()
    {
        shadowArrayDepth.setup(GL_TEXTURE_2D_ARRAY, resolution, resolution, uniforms::NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glNamedFramebufferTextureEXT(shadowArrayFramebuffer, GL_DEPTH_ATTACHMENT, shadowArrayDepth, 0);
        shadowArrayFramebuffer.check_complete();
        gl_check_error(__FILE__, __LINE__);
    }

    void update_cascades(const float4x4 & view, const float near, const float far, const float aspectRatio, const float vfov, const float3 & lightDir)
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
                float dist = length(splitFrustumVerts[i].xyz() - frustumCentroid) * 1.0;
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

    void pre_draw()
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
    
    GlShader & get_program()
    {
       return program.get()->get_variant()->shader;
    }

    void post_draw()
    {
        auto & shader = program.get()->get_variant()->shader;
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        shader.unbind();
    }

    GLuint get_output_texture() const
    {
        return shadowArrayDepth.id();
    }
};

template<class F> void visit_fields(stable_cascaded_shadows & o, F f)
{
    f("shadowmap_resolution", o.resolution);
    f("cascade_split", o.splitLambda, range_metadata<float>{ 0.1f, 1.0f });
}

struct renderer_settings
{
    int2 renderSize{ 0, 0 };
    int cameraCount = 1;
    int msaaSamples = 4;
    bool performanceProfiling = true;
    bool useDepthPrepass = false;
    bool tonemapEnabled = true;
    bool shadowsEnabled = true;
};

struct view_data
{
    uint32_t index;
    Pose pose;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
    float nearClip;
    float farClip;

    view_data(const uint32_t idx, const Pose & p, const float4x4 & projMat)
    {
        index = idx;
        pose = p;
        viewMatrix = p.view_matrix();
        projectionMatrix = projMat;
        viewProjMatrix = mul(projMat, viewMatrix);
        near_far_clip_from_projection(projectionMatrix, nearClip, farClip);
    }
};

struct render_payload
{
    ProceduralSky * skybox{ nullptr };
    std::vector<Renderable *> renderSet;
    std::vector<uniforms::point_light> pointLights;
    uniforms::directional_light sunlight;
    std::vector<view_data> views;
    float4 clear_color{ 1, 0, 0, 1 };
    texture_handle ibl_radianceCubemap;
    texture_handle ibl_irradianceCubemap;
};

/////////////////////////////////////////
//   Primary Renderer Implementation   //
/////////////////////////////////////////

class forward_renderer
{
    simple_cpu_timer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    // MSAA 
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    // Non-MSAA Targets
    std::vector<GlFramebuffer> eyeFramebuffers;
    std::vector<GlTexture2D> eyeTextures;
    std::vector<GlTexture2D> eyeDepthTextures;

    std::unique_ptr<stable_cascaded_shadows> shadow;
    GlMesh post_quad;

    shader_handle earlyZPass = { "depth-prepass" };
    shader_handle hdr_tonemapShader = { "post-tonemap" };

    // Update per-object uniform buffer
    void update_per_object_uniform_buffer(Renderable * top, const view_data & d);

    void run_depth_prepass(const view_data & view, const render_payload & scene);
    void run_skybox_pass(const view_data & view, const render_payload & scene);
    void run_shadow_pass(const view_data & view, const render_payload & scene);
    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const view_data & view, const render_payload & scene);
    void run_post_pass(const view_data & view, const render_payload & scene);

public:

    std::vector<GlFramebuffer> postFramebuffers;
    std::vector<GlTexture2D> postTextures;

    renderer_settings settings;
    profiler<simple_cpu_timer> cpuProfiler;
    profiler<gl_gpu_timer> gpuProfiler;

    forward_renderer(const renderer_settings settings);
    ~forward_renderer();

    void render_frame(const render_payload & scene);

    uint32_t get_color_texture(const uint32_t idx) const;
    uint32_t get_depth_texture(const uint32_t idx) const;

    stable_cascaded_shadows * get_shadow_pass() const;
};

template<class F> void visit_fields(forward_renderer & o, F f)
{
    f("num_cameras", o.settings.cameraCount, editor_hidden{});
    f("num_msaa_samples", o.settings.msaaSamples, editor_hidden{});
    f("render_size", o.settings.renderSize);
    f("performance_profiling", o.settings.performanceProfiling);
    f("depth_prepass", o.settings.useDepthPrepass);
    f("tonemap_pass", o.settings.tonemapEnabled);
    f("shadow_pass", o.settings.shadowsEnabled);
}

#endif // end polymer_renderer_hpp
  
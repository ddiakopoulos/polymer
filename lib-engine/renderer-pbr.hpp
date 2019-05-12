#pragma once

#ifndef polymer_renderer_pbr_hpp
#define polymer_renderer_pbr_hpp

#include "math-core.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "queue-circular.hpp"
#include "human_time.hpp"
#include "profiling.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "environment.hpp"

#undef near
#undef far

namespace polymer
{
    /////////////////////////////////
    //   stable_cascaded_shadows   //
    /////////////////////////////////

    class stable_cascaded_shadows
    {
        gl_texture_3d shadowArrayDepth;
        gl_framebuffer shadowArrayFramebuffer;
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

        stable_cascaded_shadows();

        void update_cascades(const float4x4 & view, const float near, const float far, const float aspectRatio, const float vfov, const float3 & lightDir);
        void update_shadow_matrix(const float4x4 & shadowModelMatrix);
        void pre_draw();
        void post_draw();

        GLuint get_output_texture() const;
    };

    template<class F> void visit_fields(stable_cascaded_shadows & o, F f)
    {
        f("shadowmap_resolution", o.resolution);
        f("cascade_split", o.splitLambda, range_metadata<float>{ 0.1f, 1.0f });
    }

    ////////////////////////////////////////
    //   render system data + utilities   //
    ////////////////////////////////////////

    struct renderer_settings
    {
        int2 renderSize{ 0, 0 };
        uint32_t cameraCount{ 1 };
        uint32_t msaaSamples{ 4 };
        bool performanceProfiling{ true };
        bool useDepthPrepass{ false };
        bool tonemapEnabled{ true };
        bool shadowsEnabled{ true };
    };

    struct view_data
    {
        uint32_t index;
        transform pose;
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float4x4 viewProjMatrix;
        float nearClip;
        float farClip;

        view_data(const uint32_t idx, const transform & p, const float4x4 & projMat)
        {
            index = idx;
            pose = p;
            viewMatrix = p.view_matrix();
            projectionMatrix = projMat;
            viewProjMatrix = projMat * viewMatrix;
            near_far_clip_from_projection(projectionMatrix, nearClip, farClip);
        }
    };

    class render_system;
    struct render_payload
    {
        std::vector<view_data> views;
        std::vector<render_component> render_components;
        std::vector<point_light_component *> point_lights;
        directional_light_component * sunlight{ nullptr };
        skybox_component * skybox{ nullptr };
        float4 clear_color{ 1, 0, 0, 1 };
        texture_handle ibl_radianceCubemap;
        texture_handle ibl_irradianceCubemap;
        void reset()
        {
           views.clear();
           render_components.clear();
           point_lights.clear();
           sunlight = nullptr;
           skybox = nullptr;
        }
    };

    //////////////////////
    //   pbr_renderer   //
    //////////////////////

    class pbr_renderer 
    {
        simple_cpu_timer timer;

        gl_buffer perScene;
        gl_buffer perView;
        gl_buffer perObject;

        // MSAA Targets
        gl_renderbuffer multisampleRenderbuffers[2]; // color, depth/stencil
        gl_framebuffer multisampleFramebuffer;

        // Non-MSAA Targets
        std::vector<gl_framebuffer> eyeFramebuffers;
        std::vector<gl_texture_2d> eyeTextures, eyeDepthTextures;

        std::unique_ptr<stable_cascaded_shadows> shadow;
        gl_mesh post_quad;

        gl_mesh left_stencil_mask, right_stencil_mask;
        bool using_stencil_mask{ false };

        shader_handle renderPassEarlyZ = { "depth-prepass" };
        shader_handle renderPassTonemap = { "post-tonemap" };
        shader_handle no_op = { "no-op" };

        void update_per_object_uniform_buffer(const transform & p, const float3 & scale, const bool receiveShadow, const view_data & d);
        void run_stencil_prepass(const view_data & view, const render_payload & scene);
        void run_depth_prepass(const view_data & view, const render_payload & scene);
        void run_skybox_pass(const view_data & view, const render_payload & scene);
        void run_shadow_pass(const view_data & view, const render_payload & scene);
        void run_forward_pass(std::vector<const render_component *> & render_queue, const view_data & view, const render_payload & scene);
        void run_post_pass(const view_data & view, const render_payload & scene);

    public:

        std::vector<gl_framebuffer> postFramebuffers;
        std::vector<gl_texture_2d> postTextures;

        renderer_settings settings;
        profiler<simple_cpu_timer> cpuProfiler;
        profiler<gl_gpu_timer> gpuProfiler;

        pbr_renderer(const renderer_settings settings);
        ~pbr_renderer();

        void render_frame(const render_payload & scene);

        uint32_t get_color_texture(const uint32_t idx = 0) const;
        uint32_t get_depth_texture(const uint32_t idx = 0) const;
        void set_stencil_mask(const uint32_t idx, gl_mesh && m);

        stable_cascaded_shadows * get_shadow_pass() const;
    };

    template<class F> void visit_fields(pbr_renderer & o, F f)
    {
        f("num_cameras", o.settings.cameraCount, editor_hidden{});
        f("num_msaa_samples", o.settings.msaaSamples, editor_hidden{});
        f("render_size", o.settings.renderSize);
        f("performance_profiling", o.settings.performanceProfiling);
        f("depth_prepass", o.settings.useDepthPrepass);
        f("tonemap_pass", o.settings.tonemapEnabled);
        f("shadow_pass", o.settings.shadowsEnabled);
    }

}

#endif // end polymer_renderer_pbr_hpp
  
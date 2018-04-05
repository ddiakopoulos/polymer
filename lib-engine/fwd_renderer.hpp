#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "math-core.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "circular_buffer.hpp"
#include "human_time.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"

#include "scene.hpp"
#include "shadow_pass.hpp"

using namespace polymer;

struct renderer_settings
{
    float2 renderSize{ 0, 0 };
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

struct scene_data
{
    ProceduralSky * skybox{ nullptr };
    std::vector<Renderable *> renderSet;
    std::vector<uniforms::point_light> pointLights;
    uniforms::directional_light sunlight;
    std::vector<view_data> views;
};

template<typename T>
struct profiler
{
    struct data_point
    {
        CircularBuffer<double> average{ 30 };
        T timer;
    };

    std::unordered_map<std::string, data_point> dataPoints;

    bool enabled{ true };

    profiler() { }

    void set_enabled(bool newState) 
    { 
        enabled = newState;
        dataPoints.clear();
    }

    void begin(const std::string & id)
    { 
        if (!enabled) return;
        dataPoints[id].timer.start();
    }

    void end(const std::string & id) 
    { 
        if (!enabled) return;
        dataPoints[id].timer.stop();
        const double t = dataPoints[id].timer.elapsed_ms();
        if (t > 0.0) dataPoints[id].average.put(t);
    }
};

class forward_renderer
{
    SimpleTimer timer;

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

    std::unique_ptr<StableCascadedShadowPass> shadow;

    GlMesh post_quad;

    GlShaderHandle earlyZPass = { "depth-prepass" };
    GlShaderHandle hdr_tonemapShader = { "post-tonemap" };

    // Update per-object uniform buffer
    void update_per_object_uniform_buffer(Renderable * top, const view_data & d);

    void run_depth_prepass(const view_data & view, const scene_data & scene);
    void run_skybox_pass(const view_data & view, const scene_data & scene);
    void run_shadow_pass(const view_data & view, const scene_data & scene);
    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const view_data & view, const scene_data & scene);
    void run_post_pass(const view_data & view, const scene_data & scene);

public:

    renderer_settings settings;
    profiler<SimpleTimer> cpuProfiler;
    profiler<GlGpuTimer> gpuProfiler;

    forward_renderer(const renderer_settings settings);
    ~forward_renderer();

    void render_frame(const scene_data & scene);

    uint32_t get_color_texture(const uint32_t idx) const;
    uint32_t get_depth_texture(const uint32_t idx) const;

    StableCascadedShadowPass & get_shadow_pass() const;
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
};

#endif // end vr_renderer_hpp
  
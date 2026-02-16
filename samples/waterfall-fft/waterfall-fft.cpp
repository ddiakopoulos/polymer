// This sample demonstrates real-time audio FFT visualization using a 3D waterfall
// display with orbit camera, displaced mesh geometry, and colormap-based vertex coloring.
// It uses kissfft for FFT processing and supports multiple window functions, colormaps,
// and visualization parameters. Features include HDR rendering with bloom post-processing,
// holographic wireframe mode, and edge fade effects.

#include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-gfx-gl/gl-procedural-mesh.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include "kiss_fftr.h"

#include <fstream>
#include <cmath>
#include <future>
#include <chrono>

using namespace polymer;
using namespace gui;


enum class window_type : uint32_t
{
    rectangular,
    hann,
    hamming,
    blackman
};

enum class scale_type : uint32_t
{
    linear,
    logarithmic
};

enum class render_mode : uint32_t
{
    solid,
    wireframe
};

struct wav_data
{
    uint32_t sample_rate = 0;
    uint16_t num_channels = 0;
    uint16_t bits_per_sample = 0;
    std::vector<float> samples;  // Normalized [-1, 1], mono
};

struct spectrogram_params
{
    int32_t fft_size = 2048;
    float overlap_percent = 75.0f;
    window_type window = window_type::hann;
    scale_type scale = scale_type::logarithmic;
    float dynamic_range_db = 80.0f;
    colormap::colormap_t colormap = colormap::colormap_t::ampl;
};

struct visualization_params
{
    float time_window_seconds = 5.0f;
    float height_scale = 1.0f;
    float mesh_width = 10.0f;
    float mesh_depth = 10.0f;
    int32_t frequency_resolution = 256;
    float edge_fade_intensity = 1.0f;
    float edge_fade_distance = 0.50f;
    float grid_density = 1.0f;  // >1 = more lines (subdivisions), <1 = fewer lines (sparser grid)
    float wireframe_distance_fade_start = 0.2f; // normalized [0,1] of view distance
    float wireframe_distance_fade_end = 0.9f;   // normalized [0,1] of view distance
    float wireframe_width_boost = 2.0f;
    int32_t wireframe_blend_mode = 0; // 0=Additive, 1=Alpha, 2=Premultiplied
    bool enable_time_smoothing = true;
    float time_smoothing_alpha = 0.30f;
    bool enable_freq_smoothing = true;
    float freq_smoothing_strength = 0.50f;
};

struct post_processing_params
{
    bool bloom_enabled = true;
    float bloom_threshold = 0.8f;
    float bloom_knee = 0.5f;
    float bloom_strength = 1.0f;
    float bloom_radius = 0.5f;
    float exposure = 1.0f;
    float gamma = 2.2f;
    int32_t tonemap_mode = 1;  // 0=none, 1=Reinhard, 2=ACES
};

struct taa_params
{
    bool enabled = true;
    int32_t debug_mode = 0;  // 0=off, 1=velocity, 2=current
    int32_t jitter_sequence_length = 16;
    float feedback_min = 0.88f;
    float feedback_max = 0.97f;
    float depth_threshold = 0.001f;
    float velocity_feedback_scale = 120.0f;
};

struct taa_state
{
    int32_t jitter_index = 0;
    float2 current_jitter = {0.0f, 0.0f};
    float2 previous_jitter = {0.0f, 0.0f};
    float4x4 current_view_matrix;
    float4x4 previous_view_matrix;
    float4x4 current_proj_jittered;
    float4x4 current_proj_unjittered;
    float4x4 previous_viewproj;
    int32_t history_index = 0;
    bool first_frame = true;
};

// ============================================================================
// Inline Free Functions
// ============================================================================

inline wav_data load_wav_file(const std::string & path)
{
    wav_data result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open WAV file: " + path);
    }

    // Read RIFF header
    char riff_header[4];
    file.read(riff_header, 4);
    if (std::strncmp(riff_header, "RIFF", 4) != 0)
    {
        throw std::runtime_error("Invalid WAV file: missing RIFF header");
    }

    uint32_t file_size;
    file.read(reinterpret_cast<char *>(&file_size), 4);

    char wave_header[4];
    file.read(wave_header, 4);
    if (std::strncmp(wave_header, "WAVE", 4) != 0)
    {
        throw std::runtime_error("Invalid WAV file: missing WAVE header");
    }

    // Find fmt chunk
    while (file.good())
    {
        char chunk_id[4];
        uint32_t chunk_size;
        file.read(chunk_id, 4);
        file.read(reinterpret_cast<char *>(&chunk_size), 4);

        if (std::strncmp(chunk_id, "fmt ", 4) == 0)
        {
            uint16_t audio_format;
            file.read(reinterpret_cast<char *>(&audio_format), 2);
            if (audio_format != 1)
            {
                throw std::runtime_error("Only PCM format is supported");
            }

            file.read(reinterpret_cast<char *>(&result.num_channels), 2);
            file.read(reinterpret_cast<char *>(&result.sample_rate), 4);

            uint32_t byte_rate;
            uint16_t block_align;
            file.read(reinterpret_cast<char *>(&byte_rate), 4);
            file.read(reinterpret_cast<char *>(&block_align), 2);
            file.read(reinterpret_cast<char *>(&result.bits_per_sample), 2);

            // Skip any extra fmt data
            if (chunk_size > 16)
            {
                file.seekg(chunk_size - 16, std::ios::cur);
            }
        }
        else if (std::strncmp(chunk_id, "data", 4) == 0)
        {
            uint32_t num_samples = chunk_size / (result.bits_per_sample / 8) / result.num_channels;
            result.samples.resize(num_samples);

            if (result.bits_per_sample == 16)
            {
                std::vector<int16_t> raw_samples(num_samples * result.num_channels);
                file.read(reinterpret_cast<char *>(raw_samples.data()), chunk_size);

                // Convert to mono float normalized to [-1, 1]
                for (uint32_t i = 0; i < num_samples; ++i)
                {
                    float sum = 0.0f;
                    for (uint16_t ch = 0; ch < result.num_channels; ++ch)
                    {
                        sum += static_cast<float>(raw_samples[i * result.num_channels + ch]) / 32768.0f;
                    }
                    result.samples[i] = sum / static_cast<float>(result.num_channels);
                }
            }
            else if (result.bits_per_sample == 8)
            {
                std::vector<uint8_t> raw_samples(num_samples * result.num_channels);
                file.read(reinterpret_cast<char *>(raw_samples.data()), chunk_size);

                for (uint32_t i = 0; i < num_samples; ++i)
                {
                    float sum = 0.0f;
                    for (uint16_t ch = 0; ch < result.num_channels; ++ch)
                    {
                        sum += (static_cast<float>(raw_samples[i * result.num_channels + ch]) - 128.0f) / 128.0f;
                    }
                    result.samples[i] = sum / static_cast<float>(result.num_channels);
                }
            }
            else
            {
                throw std::runtime_error("Unsupported bits per sample: " + std::to_string(result.bits_per_sample));
            }
            break;
        }
        else
        {
            // Skip unknown chunk
            file.seekg(chunk_size, std::ios::cur);
        }
    }

    return result;
}

inline void compute_window_coefficients(std::vector<float> & coefficients, int32_t size, window_type type)
{
    coefficients.resize(size);
    const float pi = static_cast<float>(POLYMER_PI);

    switch (type)
    {
    case window_type::rectangular:
        for (int32_t i = 0; i < size; ++i) coefficients[i] = 1.0f;
        break;

    case window_type::hann:
        for (int32_t i = 0; i < size; ++i)
        {
            coefficients[i] = 0.5f * (1.0f - std::cos(2.0f * pi * i / (size - 1)));
        }
        break;

    case window_type::hamming:
        for (int32_t i = 0; i < size; ++i)
        {
            coefficients[i] = 0.54f - 0.46f * std::cos(2.0f * pi * i / (size - 1));
        }
        break;

    case window_type::blackman:
        for (int32_t i = 0; i < size; ++i)
        {
            float t = 2.0f * pi * i / (size - 1);
            coefficients[i] = 0.42f - 0.5f * std::cos(t) + 0.08f * std::cos(2.0f * t);
        }
        break;
    }
}

inline float compute_magnitude_db(float real, float imag, float dynamic_range_db)
{
    float magnitude = std::sqrt(real * real + imag * imag);
    if (magnitude < 1e-10f) magnitude = 1e-10f;

    float db = 20.0f * std::log10(magnitude);
    float normalized = (db + dynamic_range_db) / dynamic_range_db;
    return clamp<float>(normalized, 0.0f, 1.0f);
}

inline float halton_sequence(int32_t index, int32_t base)
{
    float result = 0.0f;
    float f = 1.0f;
    int32_t i = index;
    while (i > 0)
    {
        f = f / static_cast<float>(base);
        result = result + f * static_cast<float>(i % base);
        i = i / base;
    }
    return result;
}

inline float2 halton_2_3(int32_t index)
{
    return float2(
        halton_sequence(index + 1, 2) - 0.5f,
        halton_sequence(index + 1, 3) - 0.5f
    );
}

inline float4x4 apply_jitter_to_projection(float4x4 proj, float2 jitter_pixels, float2 screen_size)
{
    float2 jitter_ndc = float2(
        2.0f * jitter_pixels.x / screen_size.x,
        2.0f * jitter_pixels.y / screen_size.y
    );
    float4x4 jittered = proj;
    // Note: projection's [2][0/1] contributes with a negative sign in NDC for this matrix layout.
    jittered[2][0] -= jitter_ndc.x;
    jittered[2][1] -= jitter_ndc.y;
    return jittered;
}

inline std::vector<float> compute_gaussian_weights(int32_t kernel_radius)
{
    std::vector<float> weights(kernel_radius + 1);
    float sigma = static_cast<float>(kernel_radius) / 3.0f;
    for (int32_t i = 0; i <= kernel_radius; ++i)
        weights[i] = 0.39894228f * std::exp(-0.5f * i * i / (sigma * sigma)) / sigma;
    return weights;
}

struct sample_waterfall_fft final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    wav_data audio;
    float playback_position = 0.0f;
    bool is_playing = false;
    bool audio_loaded = false;
    bool loop_enabled = true;
    bool show_imgui = true;

    kiss_fftr_cfg fft_cfg = nullptr;
    kiss_fftr_cfg fft_cfg_async = nullptr;
    spectrogram_params params;
    visualization_params viz_params;
    post_processing_params post_params;
    taa_params taa_config;
    taa_state taa;
    std::vector<float> window_coefficients;
    std::vector<float> fft_input;
    std::vector<kiss_fft_cpx> fft_output;

    // Ring buffer for FFT history
    std::vector<std::vector<float>> fft_history;
    int32_t history_rows = 0;
    int32_t current_history_row = 0;

    // 3D mesh
    gl_mesh waterfall_mesh;
    std::vector<float> vertex_buffer;
    std::vector<uint3> index_buffer;
    std::vector<float> spectrum_raw;
    std::vector<float> spectrum_time;
    std::vector<float> spectrum_freq;
    std::future<std::vector<float>> fft_future;
    bool fft_task_active = false;

    // Camera
    camera_controller_orbit cam;

    // Shaders
    gl_shader waterfall_shader;
    gl_shader waterfall_wireframe_shader;
    gl_shader brightness_shader;
    gl_shader blur_shader;
    gl_shader composite_shader;
    gl_shader taa_velocity_shader;
    gl_shader taa_resolve_shader;

    // Render mode
    render_mode current_render_mode = render_mode::wireframe;
    float wireframe_line_width = 0.5f;
    float wireframe_glow_intensity = 1.0f;

    // HDR framebuffer resources
    gl_framebuffer hdr_framebuffer;
    gl_texture_2d hdr_color_texture;
    gl_texture_2d hdr_depth_texture;

    // Multi-mip bloom buffers (5 levels)
    gl_framebuffer bloom_fb_h[5];
    gl_framebuffer bloom_fb_v[5];
    gl_texture_2d bloom_tex_h[5];
    gl_texture_2d bloom_tex_v[5];

    // TAA framebuffer resources
    gl_framebuffer velocity_fb;
    gl_texture_2d velocity_texture; // RG16F
    gl_framebuffer taa_history_fb[2];
    gl_texture_2d taa_history_tex[2]; // RGBA16F ping-pong
    std::vector<float> previous_vertex_buffer;
    gl_mesh velocity_mesh;
    bool mesh_updated_this_frame = false;

    gl_vertex_array_object fullscreen_vao;

    int32_t current_width = 0;
    int32_t current_height = 0;

    int32_t selected_fft_size_index = 3;   // Default 2048
    int32_t selected_window_index = 1;     // Default Hann
    int32_t selected_scale_index = 1;      // Default Logarithmic
    int32_t selected_colormap_index = 7;   // Default Viridis
    int32_t selected_render_mode_index = 0; // Default Solid

    sample_waterfall_fft();
    ~sample_waterfall_fft();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector<std::string> names) override;

    void setup_fft();
    void setup_waterfall_mesh();
    void setup_post_processing(int32_t width, int32_t height);
    void setup_taa_buffers(int32_t width, int32_t height);
    void rebuild_mesh_vertices();
    void update_velocity_mesh();
    void load_audio(const std::string & path);
    std::vector<float> compute_fft_spectrum(int32_t sample_index, int32_t fft_size, const std::vector<float> & window, int32_t freq_bins, scale_type scale_mode, float dynamic_range_db);
    void wait_for_fft_task();
    void process_fft_frame();
    void try_consume_fft_task();
    void render_bloom_pass(int32_t width, int32_t height);
    void update_taa_jitter(int32_t width, int32_t height);
    void render_velocity_pass(int32_t width, int32_t height);
    void render_taa_resolve_pass(int32_t width, int32_t height);
};

sample_waterfall_fft::sample_waterfall_fft() : polymer_app(1920, 1200, "waterfall-fft", 4)
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    imgui.reset(new gui::imgui_instance(window, true));
    gui::make_light_theme();

    // Initialize orbit camera
    cam.set_eye_position({0, 5, 15});
    cam.set_target({0, 0, 0});
    cam.yfov = to_radians(65.0);
    cam.near_clip = 0.1f;
    cam.far_clip = 24.0f;

    std::string asset_base = global_asset_dir::get()->get_asset_dir();

    // Load solid shader
    waterfall_shader = gl_shader(
        read_file_text(asset_base + "/shaders/waterfall_vert.glsl"),
        read_file_text(asset_base + "/shaders/waterfall_frag.glsl"));

    // Load wireframe shader with geometry stage
    waterfall_wireframe_shader = gl_shader(
        read_file_text(asset_base + "/shaders/waterfall_wireframe_vert.glsl"),
        read_file_text(asset_base + "/shaders/waterfall_wireframe_frag.glsl"),
        read_file_text(asset_base + "/shaders/waterfall_wireframe_geom.glsl"));

    std::string fullscreen_vert = read_file_text(asset_base + "/shaders/waterfall_fullscreen_vert.glsl");
    brightness_shader = gl_shader(fullscreen_vert, read_file_text(asset_base + "/shaders/bloom/bloom_brightness_frag.glsl"));
    blur_shader = gl_shader(fullscreen_vert, read_file_text(asset_base + "/shaders/bloom/bloom_blur_frag.glsl"));
    composite_shader = gl_shader(fullscreen_vert, read_file_text(asset_base + "/shaders/bloom/bloom_composite_frag.glsl"));

    taa_velocity_shader = gl_shader( read_file_text(asset_base + "/shaders/waterfall_taa_velocity_vert.glsl"), read_file_text(asset_base + "/shaders/waterfall_taa_velocity_frag.glsl"));
    taa_resolve_shader = gl_shader(fullscreen_vert, read_file_text(asset_base + "/shaders/waterfall_taa_resolve_frag.glsl"));

    setup_fft();
    setup_waterfall_mesh();
    setup_post_processing(width, height);
    if (taa_config.enabled) setup_taa_buffers(width, height);

    current_width = width;
    current_height = height;
}

sample_waterfall_fft::~sample_waterfall_fft()
{
    wait_for_fft_task();
    if (fft_cfg) kiss_fftr_free(fft_cfg);
    if (fft_cfg_async) kiss_fftr_free(fft_cfg_async);
}

void sample_waterfall_fft::wait_for_fft_task()
{
    if (fft_task_active && fft_future.valid())
    {
        fft_future.wait();
        fft_task_active = false;
    }
}

void sample_waterfall_fft::setup_fft()
{
    wait_for_fft_task();

    if (fft_cfg)
    {
        kiss_fftr_free(fft_cfg);
        fft_cfg = nullptr;
    }
    if (fft_cfg_async)
    {
        kiss_fftr_free(fft_cfg_async);
        fft_cfg_async = nullptr;
    }

    fft_cfg = kiss_fftr_alloc(params.fft_size, 0, nullptr, nullptr);
    fft_cfg_async = kiss_fftr_alloc(params.fft_size, 0, nullptr, nullptr);
    fft_input.resize(params.fft_size);
    fft_output.resize(params.fft_size / 2 + 1);
    compute_window_coefficients(window_coefficients, params.fft_size, params.window);
}

void sample_waterfall_fft::setup_waterfall_mesh()
{
    wait_for_fft_task();

    // Calculate grid dimensions based on time window
    float hop_size = params.fft_size * (1.0f - params.overlap_percent / 100.0f);
    float frames_per_second = audio_loaded ? (audio.sample_rate / hop_size) : 44100.0f / hop_size;
    history_rows = static_cast<int32_t>(viz_params.time_window_seconds * frames_per_second);
    history_rows = clamp<int32_t>(history_rows, 10, 2000);

    int32_t freq_bins = std::min(viz_params.frequency_resolution, params.fft_size / 2);

    // Resize FFT history buffer
    fft_history.resize(history_rows);
    for (std::vector<float> & row : fft_history)
    {
        row.resize(freq_bins, 0.0f);
    }
    current_history_row = 0;

    spectrum_raw.assign(freq_bins, 0.0f);
    spectrum_time.assign(freq_bins, 0.0f);
    spectrum_freq.assign(freq_bins, 0.0f);

    // Build index buffer (triangles for grid)
    index_buffer.clear();
    for (int32_t z = 0; z < history_rows - 1; ++z)
    {
        for (int32_t x = 0; x < freq_bins - 1; ++x)
        {
            uint32_t tl = z * freq_bins + x;
            uint32_t tr = z * freq_bins + x + 1;
            uint32_t bl = (z + 1) * freq_bins + x;
            uint32_t br = (z + 1) * freq_bins + x + 1;
            index_buffer.push_back({tl, bl, tr});
            index_buffer.push_back({tr, bl, br});
        }
    }

    // Allocate vertex buffer
    int32_t vertex_count = history_rows * freq_bins;
    vertex_buffer.resize(vertex_count * 8); // 3 pos + 3 color + 2 grid_uv

    rebuild_mesh_vertices();
}

void sample_waterfall_fft::setup_post_processing(int32_t width, int32_t height)
{
    // HDR framebuffer at full resolution
    hdr_color_texture.setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    hdr_depth_texture.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glNamedFramebufferTexture(hdr_framebuffer, GL_COLOR_ATTACHMENT0, hdr_color_texture, 0);
    glNamedFramebufferTexture(hdr_framebuffer, GL_DEPTH_ATTACHMENT, hdr_depth_texture, 0);
    hdr_framebuffer.check_complete();

    // Multi-mip bloom buffers (5 levels, each half the previous)
    int32_t mip_w = width / 2;
    int32_t mip_h = height / 2;

    for (int32_t i = 0; i < 5; ++i)
    {
        bloom_tex_h[i].setup(mip_w, mip_h, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(bloom_tex_h[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(bloom_fb_h[i], GL_COLOR_ATTACHMENT0, bloom_tex_h[i], 0);
        bloom_fb_h[i].check_complete();

        bloom_tex_v[i].setup(mip_w, mip_h, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(bloom_tex_v[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(bloom_fb_v[i], GL_COLOR_ATTACHMENT0, bloom_tex_v[i], 0);
        bloom_fb_v[i].check_complete();

        mip_w = std::max(1, mip_w / 2);
        mip_h = std::max(1, mip_h / 2);
    }

    gl_check_error(__FILE__, __LINE__);
}

void sample_waterfall_fft::setup_taa_buffers(int32_t width, int32_t height)
{
    // Velocity buffer (RG16F)
    velocity_texture.setup(width, height, GL_RG16F, GL_RG, GL_FLOAT, nullptr);
    glTextureParameteri(velocity_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(velocity_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(velocity_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(velocity_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glNamedFramebufferTexture(velocity_fb, GL_COLOR_ATTACHMENT0, velocity_texture, 0);
    glNamedFramebufferTexture(velocity_fb, GL_DEPTH_ATTACHMENT, hdr_depth_texture, 0);
    velocity_fb.check_complete();

    // History buffers (RGBA16F ping-pong)
    for (int32_t i = 0; i < 2; ++i)
    {
        taa_history_tex[i].setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(taa_history_tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(taa_history_tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(taa_history_tex[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(taa_history_tex[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glNamedFramebufferTexture(taa_history_fb[i], GL_COLOR_ATTACHMENT0, taa_history_tex[i], 0);
        taa_history_fb[i].check_complete();
    }

    gl_check_error(__FILE__, __LINE__);
}

void sample_waterfall_fft::rebuild_mesh_vertices()
{
    if (fft_history.empty() || fft_history[0].empty()) return;

    int32_t freq_bins = static_cast<int32_t>(fft_history[0].size());
    float x_scale = viz_params.mesh_width / freq_bins;
    float z_scale = viz_params.mesh_depth / history_rows;

    int32_t vi = 0;
    for (int32_t z = 0; z < history_rows; ++z)
    {
        // Map ring buffer row to mesh row (newest at front)
        int32_t buffer_row = (current_history_row - z + history_rows) % history_rows;

        for (int32_t x = 0; x < freq_bins; ++x)
        {
            float magnitude = fft_history[buffer_row][x];

            // Position
            float px = (x - freq_bins * 0.5f) * x_scale;
            float py = magnitude * viz_params.height_scale;
            float pz = (z - history_rows * 0.5f) * z_scale;

            // Color from colormap
            double3 color = colormap::get_color(static_cast<double>(magnitude), params.colormap);

            // Grid UV for quad-based wireframe (normalized [0,1] across entire grid)
            float grid_u = static_cast<float>(x) / static_cast<float>(freq_bins - 1);
            float grid_v = static_cast<float>(z) / static_cast<float>(history_rows - 1);

            vertex_buffer[vi++] = px;
            vertex_buffer[vi++] = py;
            vertex_buffer[vi++] = pz;
            vertex_buffer[vi++] = static_cast<float>(color.x);
            vertex_buffer[vi++] = static_cast<float>(color.y);
            vertex_buffer[vi++] = static_cast<float>(color.z);
            vertex_buffer[vi++] = grid_u;
            vertex_buffer[vi++] = grid_v;
        }
    }

    // Upload to GPU
    waterfall_mesh.set_vertex_data(vertex_buffer.size() * sizeof(float), vertex_buffer.data(), GL_DYNAMIC_DRAW);
    waterfall_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(0));
    waterfall_mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    waterfall_mesh.set_attribute(3, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(6 * sizeof(float)));

    if (!index_buffer.empty())
    {
        waterfall_mesh.set_elements(index_buffer, GL_STATIC_DRAW);
    }

    mesh_updated_this_frame = true;
}

void sample_waterfall_fft::update_velocity_mesh()
{
    if (vertex_buffer.empty())
    {
        return;
    }

    if (previous_vertex_buffer.empty() || previous_vertex_buffer.size() != vertex_buffer.size())
    {
        previous_vertex_buffer = vertex_buffer;
    }

    int32_t vertex_count = static_cast<int32_t>(vertex_buffer.size() / 8);
    std::vector<float> velocity_data(vertex_count * 11);

    for (int32_t i = 0; i < vertex_count; ++i)
    {
        int32_t src = i * 8;
        int32_t dst = i * 11;

        // Current position
        velocity_data[dst + 0] = vertex_buffer[src + 0];
        velocity_data[dst + 1] = vertex_buffer[src + 1];
        velocity_data[dst + 2] = vertex_buffer[src + 2];
        // Previous position
        velocity_data[dst + 3] = previous_vertex_buffer[src + 0];
        velocity_data[dst + 4] = previous_vertex_buffer[src + 1];
        velocity_data[dst + 5] = previous_vertex_buffer[src + 2];
        // Color
        velocity_data[dst + 6] = vertex_buffer[src + 3];
        velocity_data[dst + 7] = vertex_buffer[src + 4];
        velocity_data[dst + 8] = vertex_buffer[src + 5];
        // UV
        velocity_data[dst + 9] = vertex_buffer[src + 6];
        velocity_data[dst + 10] = vertex_buffer[src + 7];
    }

    velocity_mesh.set_vertex_data(velocity_data.size() * sizeof(float), velocity_data.data(), GL_DYNAMIC_DRAW);
    velocity_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), reinterpret_cast<void *>(0));
    velocity_mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    velocity_mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), reinterpret_cast<void *>(6 * sizeof(float)));
    velocity_mesh.set_attribute(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), reinterpret_cast<void *>(9 * sizeof(float)));

    if (!index_buffer.empty())
    {
        velocity_mesh.set_elements(index_buffer, GL_STATIC_DRAW);
    }

    // Advance previous positions for next frame if the mesh doesn't change.
    previous_vertex_buffer = vertex_buffer;
    mesh_updated_this_frame = false;
}

void sample_waterfall_fft::load_audio(const std::string & path)
{
    try
    {
        wait_for_fft_task();

        audio = load_wav_file(path);
        playback_position = 0.0f;
        audio_loaded = true;

        // Reset FFT history and rebuild mesh
        setup_waterfall_mesh();

        // Auto-play on drop
        is_playing = true;

        std::cout << "Loaded audio: " << path << std::endl;
        std::cout << "  Sample rate: " << audio.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << audio.num_channels << std::endl;
        std::cout << "  Duration: " << (audio.samples.size() / static_cast<float>(audio.sample_rate)) << " seconds" << std::endl;
    }
    catch (const std::exception & e)
    {
        std::cerr << "Failed to load audio: " << e.what() << std::endl;
    }
}

void sample_waterfall_fft::process_fft_frame()
{
    if (!audio_loaded || audio.samples.empty()) return;
    if (fft_history.empty() || fft_history[0].empty()) return;

    int32_t sample_index = static_cast<int32_t>(playback_position * audio.sample_rate);
    if (sample_index + params.fft_size > static_cast<int32_t>(audio.samples.size()))
    {
        return;
    }

    // Copy samples and apply window
    for (int32_t i = 0; i < params.fft_size; ++i)
    {
        fft_input[i] = audio.samples[sample_index + i] * window_coefficients[i];
    }

    // Perform FFT
    kiss_fftr(fft_cfg, fft_input.data(), fft_output.data());

    // Store in ring buffer (sync path)
    int32_t freq_bins = static_cast<int32_t>(fft_history[0].size());
    int32_t fft_bins = params.fft_size / 2;

    for (int32_t i = 0; i < freq_bins; ++i)
    {
        // Map visualization bins to FFT bins (linear mapping)
        int32_t fft_idx = (i * fft_bins) / freq_bins;
        fft_idx = clamp<int32_t>(fft_idx, 0, fft_bins - 1);

        float magnitude;
        if (params.scale == scale_type::logarithmic)
        {
            magnitude = compute_magnitude_db(fft_output[fft_idx].r, fft_output[fft_idx].i, params.dynamic_range_db);
        }
        else
        {
            float mag = std::sqrt(fft_output[fft_idx].r * fft_output[fft_idx].r + fft_output[fft_idx].i * fft_output[fft_idx].i);
            magnitude = clamp<float>(mag / (params.fft_size * 0.5f), 0.0f, 1.0f);
        }

        spectrum_raw[i] = magnitude;
    }

    // Time smoothing (EMA)
    if (viz_params.enable_time_smoothing)
    {
        float alpha = clamp<float>(viz_params.time_smoothing_alpha, 0.0f, 1.0f);
        for (int32_t i = 0; i < freq_bins; ++i)
        {
            spectrum_time[i] = spectrum_time[i] + alpha * (spectrum_raw[i] - spectrum_time[i]);
        }
    }
    else
    {
        spectrum_time = spectrum_raw;
    }

    // Frequency smoothing (3-tap)
    if (viz_params.enable_freq_smoothing)
    {
        for (int32_t i = 0; i < freq_bins; ++i)
        {
            int32_t i0 = (i == 0) ? 0 : i - 1;
            int32_t i1 = i;
            int32_t i2 = (i == freq_bins - 1) ? freq_bins - 1 : i + 1;
            spectrum_freq[i] = (spectrum_time[i0] + spectrum_time[i1] + spectrum_time[i2]) / 3.0f;
        }
    }

    float freq_strength = clamp<float>(viz_params.freq_smoothing_strength, 0.0f, 1.0f);
    for (int32_t i = 0; i < freq_bins; ++i)
    {
        float final_mag = viz_params.enable_freq_smoothing
            ? (spectrum_time[i] + (spectrum_freq[i] - spectrum_time[i]) * freq_strength)
            : spectrum_time[i];
        fft_history[current_history_row][i] = final_mag;
    }

    current_history_row = (current_history_row + 1) % history_rows;

    // Rebuild mesh with new data
    rebuild_mesh_vertices();
}

std::vector<float> sample_waterfall_fft::compute_fft_spectrum(int32_t sample_index, int32_t fft_size, const std::vector<float> & window, int32_t freq_bins, scale_type scale_mode, float dynamic_range_db)
{
    std::vector<float> output(freq_bins, 0.0f);
    if (!audio_loaded || audio.samples.empty()) return output;
    if (!fft_cfg_async) return output;
    if (sample_index + fft_size > static_cast<int32_t>(audio.samples.size())) return output;

    std::vector<float> local_input(fft_size);
    std::vector<kiss_fft_cpx> local_output(fft_size / 2 + 1);

    for (int32_t i = 0; i < fft_size; ++i)
    {
        local_input[i] = audio.samples[sample_index + i] * window[i];
    }

    kiss_fftr(fft_cfg_async, local_input.data(), local_output.data());

    int32_t fft_bins = fft_size / 2;
    for (int32_t i = 0; i < freq_bins; ++i)
    {
        int32_t fft_idx = (i * fft_bins) / freq_bins;
        fft_idx = clamp<int32_t>(fft_idx, 0, fft_bins - 1);

        float magnitude;
        if (scale_mode == scale_type::logarithmic)
        {
            magnitude = compute_magnitude_db(local_output[fft_idx].r, local_output[fft_idx].i, dynamic_range_db);
        }
        else
        {
            float mag = std::sqrt(local_output[fft_idx].r * local_output[fft_idx].r + local_output[fft_idx].i * local_output[fft_idx].i);
            magnitude = clamp<float>(mag / (fft_size * 0.5f), 0.0f, 1.0f);
        }
        output[i] = magnitude;
    }
    return output;
}

void sample_waterfall_fft::try_consume_fft_task()
{
    if (!fft_task_active || !fft_future.valid()) return;
    if (fft_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;

    spectrum_raw = fft_future.get();
    fft_task_active = false;

    int32_t freq_bins = static_cast<int32_t>(fft_history[0].size());

    // Time smoothing (EMA)
    if (viz_params.enable_time_smoothing)
    {
        float alpha = clamp<float>(viz_params.time_smoothing_alpha, 0.0f, 1.0f);
        for (int32_t i = 0; i < freq_bins; ++i)
        {
            spectrum_time[i] = spectrum_time[i] + alpha * (spectrum_raw[i] - spectrum_time[i]);
        }
    }
    else
    {
        spectrum_time = spectrum_raw;
    }

    // Frequency smoothing (3-tap)
    if (viz_params.enable_freq_smoothing)
    {
        for (int32_t i = 0; i < freq_bins; ++i)
        {
            int32_t i0 = (i == 0) ? 0 : i - 1;
            int32_t i1 = i;
            int32_t i2 = (i == freq_bins - 1) ? freq_bins - 1 : i + 1;
            spectrum_freq[i] = (spectrum_time[i0] + spectrum_time[i1] + spectrum_time[i2]) / 3.0f;
        }
    }

    float freq_strength = clamp<float>(viz_params.freq_smoothing_strength, 0.0f, 1.0f);
    for (int32_t i = 0; i < freq_bins; ++i)
    {
        float final_mag = viz_params.enable_freq_smoothing
            ? (spectrum_time[i] + (spectrum_freq[i] - spectrum_time[i]) * freq_strength)
            : spectrum_time[i];
        fft_history[current_history_row][i] = final_mag;
    }

    current_history_row = (current_history_row + 1) % history_rows;
    rebuild_mesh_vertices();
}

void sample_waterfall_fft::on_window_resize(int2 size)
{
    if (size.x > 0 && size.y > 0 && (size.x != current_width || size.y != current_height))
    {
        current_width = size.x;
        current_height = size.y;

        // Recreate framebuffers - we need new texture objects
        hdr_color_texture = gl_texture_2d();
        hdr_depth_texture = gl_texture_2d();
        hdr_framebuffer = gl_framebuffer();
        for (int32_t i = 0; i < 5; ++i)
        {
            bloom_tex_h[i] = gl_texture_2d();
            bloom_tex_v[i] = gl_texture_2d();
            bloom_fb_h[i] = gl_framebuffer();
            bloom_fb_v[i] = gl_framebuffer();
        }

        setup_post_processing(size.x, size.y);

        // Recreate TAA buffers on resize
        if (taa_config.enabled)
        {
            velocity_texture = gl_texture_2d();
            velocity_fb = gl_framebuffer();
            for (int32_t i = 0; i < 2; ++i)
            {
                taa_history_tex[i] = gl_texture_2d();
                taa_history_fb[i] = gl_framebuffer();
            }
            setup_taa_buffers(size.x, size.y);
            taa.first_frame = true;
            taa.jitter_index = 0;
        }
    }
}

void sample_waterfall_fft::on_input(const app_input_event & event)
{
    imgui->update_input(event);

    if (event.type == app_input_event::KEY)
    {
        // De-select all objects
        if (event.value[0] == GLFW_KEY_TAB && event.action == GLFW_RELEASE)
        {
            show_imgui = !show_imgui;
            return;
        }
    }

    // Only handle camera input if ImGui doesn't want it
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        cam.handle_input(event);
    }

}

void sample_waterfall_fft::on_drop(std::vector<std::string> names)
{
    for (const std::string & path : names)
    {
        if (path.find(".wav") != std::string::npos || path.find(".WAV") != std::string::npos)
        {
            load_audio(path);
            break;
        }
    }
}

void sample_waterfall_fft::on_update(const app_update_event & e)
{
    cam.update(e.timestep_ms);

    try_consume_fft_task();

    if (is_playing && audio_loaded)
    {
        float hop_size = params.fft_size * (1.0f - params.overlap_percent / 100.0f);
        float hop_duration = hop_size / audio.sample_rate;

        // Process one FFT frame per update at appropriate rate (async)
        if (!fft_task_active)
        {
            int32_t sample_index = static_cast<int32_t>(playback_position * audio.sample_rate);
            int32_t freq_bins = static_cast<int32_t>(fft_history[0].size());
            int32_t fft_size = params.fft_size;
            std::vector<float> window = window_coefficients;
            scale_type scale_mode = params.scale;
            float dynamic_range_db = params.dynamic_range_db;

            fft_task_active = true;
            fft_future = std::async(std::launch::async, [this, sample_index, fft_size, window, freq_bins, scale_mode, dynamic_range_db]()
            {
                return compute_fft_spectrum(sample_index, fft_size, window, freq_bins, scale_mode, dynamic_range_db);
            });
        }

        playback_position += hop_duration;

        float duration = audio.samples.size() / static_cast<float>(audio.sample_rate);
        float buffer_time = params.fft_size / static_cast<float>(audio.sample_rate);

        if (playback_position >= duration - buffer_time)
        {
            if (loop_enabled)
            {
                playback_position = 0.0f;  // Loop back to start
            }
            else
            {
                is_playing = false;
                playback_position = 0.0f;
            }
        }
    }
}

void sample_waterfall_fft::render_bloom_pass(int32_t width, int32_t height)
{
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(fullscreen_vao);

    int32_t mip_w = width / 2;
    int32_t mip_h = height / 2;

    // Use TAA output if enabled, otherwise use HDR directly
    GLuint bloom_source = taa_config.enabled ? taa_history_tex[taa.history_index] : hdr_color_texture;

    // Step 1: Brightness extraction -> bloom_v[0]
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_v[0]);
    glViewport(0, 0, mip_w, mip_h);
    glClear(GL_COLOR_BUFFER_BIT);

    brightness_shader.bind();
    brightness_shader.texture("s_hdr_color", 0, bloom_source, GL_TEXTURE_2D);
    brightness_shader.uniform("u_threshold", post_params.bloom_threshold);
    brightness_shader.uniform("u_knee", post_params.bloom_knee);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    brightness_shader.unbind();

    // Step 2: Multi-mip blur chain
    int32_t cur_w = mip_w;
    int32_t cur_h = mip_h;

    for (int32_t i = 0; i < 5; ++i)
    {
        if (i > 0)
        {
            cur_w = std::max(1, cur_w / 2);
            cur_h = std::max(1, cur_h / 2);
        }

        int32_t kernel_radius = 3 + i * 2;
        std::vector<float> weights = compute_gaussian_weights(kernel_radius);

        GLuint input_tex = (i == 0) ? bloom_tex_v[0] : bloom_tex_v[i - 1];

        // Horizontal blur: input -> bloom_h[i]
        glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_h[i]);
        glViewport(0, 0, cur_w, cur_h);
        glClear(GL_COLOR_BUFFER_BIT);

        blur_shader.bind();
        blur_shader.texture("s_source", 0, input_tex, GL_TEXTURE_2D);
        blur_shader.uniform("u_direction", float2(1.0f / cur_w, 0.0f));
        blur_shader.uniform("u_kernel_radius", kernel_radius);
        blur_shader.uniform("u_weights", kernel_radius + 1, weights);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        blur_shader.unbind();

        // Vertical blur: bloom_h[i] -> bloom_v[i]
        glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb_v[i]);
        glViewport(0, 0, cur_w, cur_h);
        glClear(GL_COLOR_BUFFER_BIT);

        blur_shader.bind();
        blur_shader.texture("s_source", 0, bloom_tex_h[i], GL_TEXTURE_2D);
        blur_shader.uniform("u_direction", float2(0.0f, 1.0f / cur_h));
        blur_shader.uniform("u_kernel_radius", kernel_radius);
        blur_shader.uniform("u_weights", kernel_radius + 1, weights);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        blur_shader.unbind();
    }
}

void sample_waterfall_fft::update_taa_jitter(int32_t width, int32_t height)
{
    if (!taa.first_frame)
    {
        // Preserve last frame's unjittered view-projection before overwriting current matrices.
        taa.previous_viewproj = taa.current_proj_unjittered * taa.current_view_matrix;
    }

    taa.previous_jitter = taa.current_jitter;
    taa.previous_view_matrix = taa.current_view_matrix;

    taa.jitter_index = (taa.jitter_index + 1) % taa_config.jitter_sequence_length;
    taa.current_jitter = halton_2_3(taa.jitter_index);

    taa.current_view_matrix = cam.get_view_matrix();

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    taa.current_proj_unjittered = cam.get_projection_matrix(aspect);
    taa.current_proj_jittered = apply_jitter_to_projection(
        taa.current_proj_unjittered,
        taa.current_jitter,
        float2(static_cast<float>(width), static_cast<float>(height))
    );

    // previous_viewproj already captured above from last frame's matrices
}

void sample_waterfall_fft::render_velocity_pass(int32_t width, int32_t height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, velocity_fb);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    float4x4 current_mvp = taa.current_proj_unjittered * taa.current_view_matrix;
    float4x4 previous_mvp = taa.first_frame ? current_mvp : taa.previous_viewproj;
    float4x4 raster_mvp = taa.current_proj_jittered * taa.current_view_matrix;

    taa_velocity_shader.bind();
    taa_velocity_shader.uniform("u_current_mvp", current_mvp);
    taa_velocity_shader.uniform("u_previous_mvp", previous_mvp);
    taa_velocity_shader.uniform("u_raster_mvp", raster_mvp);
    velocity_mesh.draw_elements();
    taa_velocity_shader.unbind();

    glDepthFunc(GL_LESS);
}

void sample_waterfall_fft::render_taa_resolve_pass(int32_t width, int32_t height)
{
    int32_t read_idx = taa.history_index;
    int32_t write_idx = (taa.history_index + 1) % 2;

    if (taa.first_frame)
    {
        glBlitNamedFramebuffer(
            hdr_framebuffer, taa_history_fb[write_idx],
            0, 0, width, height,
            0, 0, width, height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );
        taa.first_frame = false;
        taa.history_index = write_idx;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, taa_history_fb[write_idx]);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(fullscreen_vao);

    float4 jitter_uv(
        taa.current_jitter.x / static_cast<float>(width),
        taa.current_jitter.y / static_cast<float>(height),
        taa.previous_jitter.x / static_cast<float>(width),
        taa.previous_jitter.y / static_cast<float>(height)
    );

    taa_resolve_shader.bind();
    taa_resolve_shader.texture("s_current", 0, hdr_color_texture, GL_TEXTURE_2D);
    taa_resolve_shader.texture("s_history", 1, taa_history_tex[read_idx], GL_TEXTURE_2D);
    taa_resolve_shader.texture("s_velocity", 2, velocity_texture, GL_TEXTURE_2D);
    taa_resolve_shader.texture("s_depth", 3, hdr_depth_texture, GL_TEXTURE_2D);
    taa_resolve_shader.uniform("u_jitter_uv", jitter_uv);
    taa_resolve_shader.uniform("u_texel_size", float2(1.0f / width, 1.0f / height));
    taa_resolve_shader.uniform("u_feedback_min", taa_config.feedback_min);
    taa_resolve_shader.uniform("u_feedback_max", taa_config.feedback_max);
    taa_resolve_shader.uniform("u_debug_mode", taa_config.debug_mode);
    taa_resolve_shader.uniform("u_depth_threshold", taa_config.depth_threshold);
    taa_resolve_shader.uniform("u_velocity_feedback_scale", taa_config.velocity_feedback_scale);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    taa_resolve_shader.unbind();

    taa.history_index = write_idx;
}

void sample_waterfall_fft::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Check for resize
    if (width != current_width || height != current_height)
    {
        on_window_resize({width, height});
    }

    // Update TAA jitter
    if (taa_config.enabled)
    {
        update_taa_jitter(width, height);
    }

    // Step 1: Render scene to HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, hdr_framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    // Draw 3D mesh
    {
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        float4x4 mvp;
        if (taa_config.enabled)
        {
            mvp = taa.current_proj_jittered * taa.current_view_matrix;
        }
        else
        {
            mvp = cam.get_viewproj_matrix(aspect);
        }

        if (current_render_mode == render_mode::solid)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            waterfall_shader.bind();
            waterfall_shader.uniform("u_mvp", mvp);
            waterfall_shader.uniform("u_edge_fade_intensity", viz_params.edge_fade_intensity);
            waterfall_shader.uniform("u_edge_fade_distance", viz_params.edge_fade_distance);
            waterfall_shader.uniform("u_mesh_depth", viz_params.mesh_depth);
            waterfall_mesh.draw_elements();
            waterfall_shader.unbind();
        }
        else
        {
            // Wireframe mode with selectable blending for glow
            switch (viz_params.wireframe_blend_mode)
            {
                case 1: // Alpha
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case 2: // Premultiplied
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                default: // Additive
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    break;
            }
            glDisable(GL_CULL_FACE);

            int32_t freq_bins = fft_history.empty() ? 1 : static_cast<int32_t>(fft_history[0].size());

            waterfall_wireframe_shader.bind();
            waterfall_wireframe_shader.uniform("u_mvp", mvp);
            waterfall_wireframe_shader.uniform("u_edge_fade_intensity", viz_params.edge_fade_intensity);
            waterfall_wireframe_shader.uniform("u_edge_fade_distance", viz_params.edge_fade_distance);
            waterfall_wireframe_shader.uniform("u_mesh_depth", viz_params.mesh_depth);
            waterfall_wireframe_shader.uniform("u_line_width", wireframe_line_width);
            waterfall_wireframe_shader.uniform("u_glow_intensity", wireframe_glow_intensity);
            waterfall_wireframe_shader.uniform("u_near", cam.near_clip);
            waterfall_wireframe_shader.uniform("u_far", cam.far_clip);
            waterfall_wireframe_shader.uniform("u_distance_fade_start", viz_params.wireframe_distance_fade_start);
            waterfall_wireframe_shader.uniform("u_distance_fade_end", viz_params.wireframe_distance_fade_end);
            waterfall_wireframe_shader.uniform("u_line_width_boost", viz_params.wireframe_width_boost);
            waterfall_wireframe_shader.uniform("u_grid_cols", static_cast<float>(freq_bins - 1));
            waterfall_wireframe_shader.uniform("u_grid_rows", static_cast<float>(history_rows - 1));
            waterfall_wireframe_shader.uniform("u_grid_density", viz_params.grid_density);
            waterfall_mesh.draw_elements();
            waterfall_wireframe_shader.unbind();
        }
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // Step 2: TAA passes (if enabled)
    if (taa_config.enabled)
    {
        update_velocity_mesh();
        render_velocity_pass(width, height);
        render_taa_resolve_pass(width, height);
    }

    // Step 3: Bloom pass (if enabled)
    if (post_params.bloom_enabled)
    {
        render_bloom_pass(width, height);
    }

    // Step 4: Final composite to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(fullscreen_vao);

    // Use TAA output if enabled, otherwise use HDR directly
    GLuint source_texture = taa_config.enabled ? taa_history_tex[taa.history_index] : hdr_color_texture;

    composite_shader.bind();
    composite_shader.texture("s_hdr_color", 0, source_texture, GL_TEXTURE_2D);
    composite_shader.texture("s_bloom_0", 1, bloom_tex_v[0], GL_TEXTURE_2D);
    composite_shader.texture("s_bloom_1", 2, bloom_tex_v[1], GL_TEXTURE_2D);
    composite_shader.texture("s_bloom_2", 3, bloom_tex_v[2], GL_TEXTURE_2D);
    composite_shader.texture("s_bloom_3", 4, bloom_tex_v[3], GL_TEXTURE_2D);
    composite_shader.texture("s_bloom_4", 5, bloom_tex_v[4], GL_TEXTURE_2D);
    composite_shader.uniform("u_bloom_strength", post_params.bloom_enabled ? post_params.bloom_strength : 0.0f);
    composite_shader.uniform("u_bloom_radius", post_params.bloom_radius);
    composite_shader.uniform("u_exposure", post_params.exposure);
    composite_shader.uniform("u_gamma", post_params.gamma);
    composite_shader.uniform("u_tonemap_mode", post_params.tonemap_mode);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    composite_shader.unbind();

    if (show_imgui)
    {
    
        // ImGui
        imgui->begin_frame();

        gui::imgui_fixed_window_begin("Waterfall FFT", {{0, 0}, {340, height}});

        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Separator();

        ImGui::Text("Drop a .wav file onto the window to load");
        ImGui::Separator();

        if (audio_loaded)
        {
        float duration = audio.samples.size() / static_cast<float>(audio.sample_rate);
        ImGui::Text("Duration: %.2f seconds", duration);
        ImGui::Text("Sample Rate: %d Hz", audio.sample_rate);
        ImGui::Text("Position: %.2f s", playback_position);
        ImGui::Separator();

        if (ImGui::Button(is_playing ? "Pause" : "Play"))
        {
        is_playing = !is_playing;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            playback_position = 0.0f;
            is_playing = false;
            wait_for_fft_task();
            for (std::vector<float> & row : fft_history)
            {
                std::fill(row.begin(), row.end(), 0.0f);
            }
            current_history_row = 0;
            std::fill(spectrum_raw.begin(), spectrum_raw.end(), 0.0f);
            std::fill(spectrum_time.begin(), spectrum_time.end(), 0.0f);
            std::fill(spectrum_freq.begin(), spectrum_freq.end(), 0.0f);
            rebuild_mesh_vertices();
        }
        ImGui::Checkbox("Loop Playback", &loop_enabled);
        }
        else
        {
        ImGui::TextDisabled("No audio loaded");
        }

        ImGui::Separator();
        ImGui::Text("Render Mode");

        const char * render_modes[] = {"Solid", "Wireframe (Holographic)"};
        if (ImGui::Combo("Mode", &selected_render_mode_index, render_modes, IM_ARRAYSIZE(render_modes)))
        {
        current_render_mode = static_cast<render_mode>(selected_render_mode_index);
        }

        if (current_render_mode == render_mode::wireframe)
        {
        ImGui::SliderFloat("Line Width", &wireframe_line_width, 0.5f, 5.0f);
        ImGui::SliderFloat("Glow Intensity", &wireframe_glow_intensity, 0.5f, 5.0f);
        ImGui::SliderFloat("Grid Density", &viz_params.grid_density, 0.1f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower = sparser grid (0.5 = every other line)");
        ImGui::SliderFloat("Wire Fade Start", &viz_params.wireframe_distance_fade_start, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wire Fade End", &viz_params.wireframe_distance_fade_end, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wire Width Boost", &viz_params.wireframe_width_boost, 1.0f, 4.0f, "%.2f");
        const char * wire_blend_modes[] = {"Additive", "Alpha", "Premultiplied"};
        ImGui::Combo("Wire Blend", &viz_params.wireframe_blend_mode, wire_blend_modes, IM_ARRAYSIZE(wire_blend_modes));
        }

        ImGui::Separator();
        ImGui::Text("Edge Fade");
        ImGui::SliderFloat("Fade Intensity", &viz_params.edge_fade_intensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Fade Distance", &viz_params.edge_fade_distance, 0.05f, 0.4f);

        ImGui::Separator();
        ImGui::Text("FFT Settings");

        const char * fft_sizes[] = {"256", "512", "1024", "2048", "4096", "8192", "16384"};
        int32_t fft_size_values[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
        if (ImGui::Combo("FFT Size", &selected_fft_size_index, fft_sizes, IM_ARRAYSIZE(fft_sizes)))
        {
        wait_for_fft_task();
        params.fft_size = fft_size_values[selected_fft_size_index];
        setup_fft();
        setup_waterfall_mesh();
        }

        if (ImGui::SliderFloat("Overlap %", &params.overlap_percent, 0.0f, 95.0f, "%.0f%%"))
        {
        wait_for_fft_task();
        setup_waterfall_mesh();
        }

        const char * window_names[] = {"Rectangular", "Hann", "Hamming", "Blackman"};
        if (ImGui::Combo("Window", &selected_window_index, window_names, IM_ARRAYSIZE(window_names)))
        {
        wait_for_fft_task();
        params.window = static_cast<window_type>(selected_window_index);
        compute_window_coefficients(window_coefficients, params.fft_size, params.window);
        }

        const char * scale_names[] = {"Linear", "Logarithmic (dB)"};
        if (ImGui::Combo("Scale", &selected_scale_index, scale_names, IM_ARRAYSIZE(scale_names)))
        {
        wait_for_fft_task();
        params.scale = static_cast<scale_type>(selected_scale_index);
        }

        if (params.scale == scale_type::logarithmic)
        {
        if (ImGui::SliderFloat("Dynamic Range", &params.dynamic_range_db, 20.0f, 120.0f, "%.0f dB"))
        {
            wait_for_fft_task();
        }
        }

        ImGui::Checkbox("Time Smoothing", &viz_params.enable_time_smoothing);
        if (viz_params.enable_time_smoothing)
        {
        ImGui::SliderFloat("Time Smooth Alpha", &viz_params.time_smoothing_alpha, 0.0f, 1.0f, "%.2f");
        }
        ImGui::Checkbox("Freq Smoothing", &viz_params.enable_freq_smoothing);
        if (viz_params.enable_freq_smoothing)
        {
        ImGui::SliderFloat("Freq Smooth Strength", &viz_params.freq_smoothing_strength, 0.0f, 1.0f, "%.2f");
        }

        ImGui::Separator();
        ImGui::Text("Colormap");

        const char * colormap_names[] = {
        "Jet", "Hot", "Gray", "Parula", "Magma", "Inferno", "Plasma", "Viridis",
        "Cividis", "Turbo", "Twilight", "Spectral", "Cubehelix", "CMR Map",
        "Speed", "Ice", "Haline", "Deep", "Balance", "Azzurro", "Ampl"
        };
        if (ImGui::Combo("Colormap", &selected_colormap_index, colormap_names, IM_ARRAYSIZE(colormap_names)))
        {
        params.colormap = static_cast<colormap::colormap_t>(selected_colormap_index);
        rebuild_mesh_vertices();
        }

        ImGui::Separator();
        ImGui::Text("3D Visualization");

        if (ImGui::SliderFloat("Time Window", &viz_params.time_window_seconds, 1.0f, 30.0f, "%.1f s"))
        {
        wait_for_fft_task();
        setup_waterfall_mesh();
        }

        ImGui::SliderFloat("Height Scale", &viz_params.height_scale, 0.1f, 10.0f, "%.1f");

        if (ImGui::SliderFloat("Mesh Width", &viz_params.mesh_width, 5.0f, 50.0f, "%.1f"))
        {
        rebuild_mesh_vertices();
        }

        if (ImGui::SliderFloat("Mesh Depth", &viz_params.mesh_depth, 5.0f, 50.0f, "%.1f"))
        {
        rebuild_mesh_vertices();
        }

        if (ImGui::SliderInt("Freq Resolution", &viz_params.frequency_resolution, 32, 512))
        {
        wait_for_fft_task();
        setup_waterfall_mesh();
        }

        ImGui::Separator();
        ImGui::Text("Post Processing");

        ImGui::Checkbox("Enable Bloom", &post_params.bloom_enabled);
        if (post_params.bloom_enabled)
        {
        ImGui::SliderFloat("Threshold", &post_params.bloom_threshold, 0.0f, 2.0f);
        ImGui::SliderFloat("Knee", &post_params.bloom_knee, 0.0f, 1.0f);
        ImGui::SliderFloat("Strength", &post_params.bloom_strength, 0.0f, 3.0f);
        ImGui::SliderFloat("Radius", &post_params.bloom_radius, 0.0f, 1.0f);
        }
        ImGui::SliderFloat("Exposure", &post_params.exposure, 0.1f, 5.0f);
        ImGui::SliderFloat("Gamma", &post_params.gamma, 1.0f, 3.0f);

        const char * tonemap_names[] = {"None", "Reinhard", "ACES Filmic"};
        ImGui::Combo("Tonemapping", &post_params.tonemap_mode, tonemap_names, IM_ARRAYSIZE(tonemap_names));

        ImGui::Separator();
        ImGui::Text("Temporal Anti-Aliasing");

        if (ImGui::Checkbox("Enable TAA", &taa_config.enabled))
        {
        if (taa_config.enabled)
        {
            setup_taa_buffers(current_width, current_height);
            taa.first_frame = true;
        }
        }

        if (taa_config.enabled)
        {
        ImGui::SliderFloat("Feedback Min", &taa_config.feedback_min, 0.5f, 0.99f);
        ImGui::SliderFloat("Feedback Max", &taa_config.feedback_max, 0.5f, 0.99f);

        const char * jitter_lengths[] = {"8", "16", "32"};
        int32_t jitter_idx = (taa_config.jitter_sequence_length == 8) ? 0 :
                                (taa_config.jitter_sequence_length == 16) ? 1 : 2;
        if (ImGui::Combo("Jitter Samples", &jitter_idx, jitter_lengths, 3))
        {
            taa_config.jitter_sequence_length = (jitter_idx == 0) ? 8 : (jitter_idx == 1) ? 16 : 32;
        }

        const char * taa_debug_modes[] = {"Off", "Velocity", "Current"};
        ImGui::Combo("TAA Debug", &taa_config.debug_mode, taa_debug_modes, IM_ARRAYSIZE(taa_debug_modes));

        ImGui::SliderFloat("Depth Reject", &taa_config.depth_threshold, 0.0001f, 0.02f, "%.5f");
        ImGui::SliderFloat("Velocity Scale", &taa_config.velocity_feedback_scale, 10.0f, 400.0f, "%.0f");
        }

        gui::imgui_fixed_window_end();
        imgui->end_frame();
    } // show_imgui

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_waterfall_fft app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        POLYMER_ERROR("[Fatal] Caught exception: \n" << e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

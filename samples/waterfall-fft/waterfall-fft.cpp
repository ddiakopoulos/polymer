// This sample demonstrates real-time audio FFT visualization using a 3D waterfall
// display with orbit camera, displaced mesh geometry, and colormap-based vertex coloring.
// It uses kissfft for FFT processing and supports multiple window functions, colormaps,
// and visualization parameters.

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
    colormap::colormap_t colormap = colormap::colormap_t::mpl_viridis;
};

struct visualization_params
{
    float time_window_seconds = 5.0f;
    float height_scale = 2.0f;
    float mesh_width = 10.0f;
    float mesh_depth = 10.0f;
    int32_t frequency_resolution = 256;
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

struct sample_waterfall_fft final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    wav_data audio;
    float playback_position = 0.0f;
    bool is_playing = false;
    bool audio_loaded = false;

    kiss_fftr_cfg fft_cfg = nullptr;
    spectrogram_params params;
    visualization_params viz_params;
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

    // Camera
    camera_controller_orbit cam;

    gl_shader waterfall_shader;

    int32_t selected_fft_size_index = 3;   // Default 2048
    int32_t selected_window_index = 1;     // Default Hann
    int32_t selected_scale_index = 1;      // Default Logarithmic
    int32_t selected_colormap_index = 7;   // Default Viridis

    sample_waterfall_fft();
    ~sample_waterfall_fft();

    void on_window_resize(int2 size) override;
    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_drop(std::vector<std::string> names) override;

    void setup_fft();
    void setup_waterfall_mesh();
    void rebuild_mesh_vertices();
    void load_audio(const std::string & path);
    void process_fft_frame();
};

sample_waterfall_fft::sample_waterfall_fft() : polymer_app(1920, 1200, "waterfall-fft")
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
    cam.yfov = 1.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 100.0f;

    std::string asset_base = global_asset_dir::get()->get_asset_dir();

    waterfall_shader = gl_shader(
        read_file_text(asset_base + "/shaders/waterfall_vert.glsl"),
        read_file_text(asset_base + "/shaders/waterfall_frag.glsl"));

    setup_fft();
    setup_waterfall_mesh();
}

sample_waterfall_fft::~sample_waterfall_fft()
{
    if (fft_cfg) kiss_fftr_free(fft_cfg);
}

void sample_waterfall_fft::setup_fft()
{
    if (fft_cfg)
    {
        kiss_fftr_free(fft_cfg);
        fft_cfg = nullptr;
    }

    fft_cfg = kiss_fftr_alloc(params.fft_size, 0, nullptr, nullptr);
    fft_input.resize(params.fft_size);
    fft_output.resize(params.fft_size / 2 + 1);
    compute_window_coefficients(window_coefficients, params.fft_size, params.window);
}

void sample_waterfall_fft::setup_waterfall_mesh()
{
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
    vertex_buffer.resize(vertex_count * 6); // 3 pos + 3 color

    rebuild_mesh_vertices();
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

            vertex_buffer[vi++] = px;
            vertex_buffer[vi++] = py;
            vertex_buffer[vi++] = pz;
            vertex_buffer[vi++] = static_cast<float>(color.x);
            vertex_buffer[vi++] = static_cast<float>(color.y);
            vertex_buffer[vi++] = static_cast<float>(color.z);
        }
    }

    // Upload to GPU
    waterfall_mesh.set_vertex_data(vertex_buffer.size() * sizeof(float), vertex_buffer.data(), GL_DYNAMIC_DRAW);
    waterfall_mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(0));
    waterfall_mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));

    if (!index_buffer.empty())
    {
        waterfall_mesh.set_elements(index_buffer, GL_STATIC_DRAW);
    }
}

void sample_waterfall_fft::load_audio(const std::string & path)
{
    try
    {
        audio = load_wav_file(path);
        playback_position = 0.0f;
        is_playing = false;
        audio_loaded = true;

        // Reset FFT history and rebuild mesh
        setup_waterfall_mesh();

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

    // Store in ring buffer
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

        fft_history[current_history_row][i] = magnitude;
    }

    current_history_row = (current_history_row + 1) % history_rows;

    // Rebuild mesh with new data
    rebuild_mesh_vertices();
}

void sample_waterfall_fft::on_window_resize(int2 size) {}

void sample_waterfall_fft::on_input(const app_input_event & event)
{
    imgui->update_input(event);

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

    if (is_playing && audio_loaded)
    {
        float hop_size = params.fft_size * (1.0f - params.overlap_percent / 100.0f);
        float hop_duration = hop_size / audio.sample_rate;

        // Process one FFT frame per update at appropriate rate
        process_fft_frame();

        playback_position += hop_duration;

        float duration = audio.samples.size() / static_cast<float>(audio.sample_rate);
        if (playback_position >= duration - (params.fft_size / static_cast<float>(audio.sample_rate)))
        {
            is_playing = false;
            playback_position = 0.0f;
        }
    }
}

void sample_waterfall_fft::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Draw 3D mesh
    {
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        float4x4 mvp = cam.get_viewproj_matrix(aspect);

        waterfall_shader.bind();
        waterfall_shader.uniform("u_mvp", mvp);
        waterfall_mesh.draw_elements();
        waterfall_shader.unbind();
    }

    // ImGui
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    imgui->begin_frame();

    gui::imgui_fixed_window_begin("Waterfall FFT", {{0, 0}, {320, height}});

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
            for (std::vector<float> & row : fft_history)
            {
                std::fill(row.begin(), row.end(), 0.0f);
            }
            current_history_row = 0;
            rebuild_mesh_vertices();
        }
    }
    else
    {
        ImGui::TextDisabled("No audio loaded");
    }

    ImGui::Separator();
    ImGui::Text("FFT Settings");

    const char * fft_sizes[] = {"256", "512", "1024", "2048", "4096", "8192", "16384"};
    int32_t fft_size_values[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
    if (ImGui::Combo("FFT Size", &selected_fft_size_index, fft_sizes, IM_ARRAYSIZE(fft_sizes)))
    {
        params.fft_size = fft_size_values[selected_fft_size_index];
        setup_fft();
        setup_waterfall_mesh();
    }

    if (ImGui::SliderFloat("Overlap %", &params.overlap_percent, 0.0f, 95.0f, "%.0f%%"))
    {
        setup_waterfall_mesh();
    }

    const char * window_names[] = {"Rectangular", "Hann", "Hamming", "Blackman"};
    if (ImGui::Combo("Window", &selected_window_index, window_names, IM_ARRAYSIZE(window_names)))
    {
        params.window = static_cast<window_type>(selected_window_index);
        compute_window_coefficients(window_coefficients, params.fft_size, params.window);
    }

    const char * scale_names[] = {"Linear", "Logarithmic (dB)"};
    if (ImGui::Combo("Scale", &selected_scale_index, scale_names, IM_ARRAYSIZE(scale_names)))
    {
        params.scale = static_cast<scale_type>(selected_scale_index);
    }

    if (params.scale == scale_type::logarithmic)
    {
        ImGui::SliderFloat("Dynamic Range", &params.dynamic_range_db, 20.0f, 120.0f, "%.0f dB");
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
        setup_waterfall_mesh();
    }

    ImGui::Separator();
    ImGui::Text("Camera Controls");
    ImGui::TextWrapped("RMB: Orbit, Shift+RMB/MMB: Pan, Scroll: Zoom");

    gui::imgui_fixed_window_end();
    imgui->end_frame();

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

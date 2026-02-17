#include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-gfx-gl/gl-post-processing.hpp"
#include "polymer-gfx-gl/post/gl-unreal-bloom.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include <cmath>
#include <vector>
#include <string>

using namespace polymer;
using namespace gui;

// ============================================================================
// Enums
// ============================================================================

enum class prim_type : uint32_t { circle = 0, box = 1, capsule = 2, segment = 3, lens = 4, ngon = 5 };
enum class material_type : uint32_t { diffuse = 0, mirror = 1, glass = 2, water = 3, diamond = 4 };

// ============================================================================
// GPU SDF Primitive (80 bytes, maps 1:1 to GLSL std430)
// ============================================================================

struct gpu_sdf_primitive
{
    float2 position    = {0.0f, 0.0f};
    float rotation     = 0.0f;
    uint32_t prim      = 0;
    float4 params      = {1.0f, 0.0f, 0.0f, 0.0f};
    uint32_t material  = 0;
    float ior_base     = 1.5f;
    float cauchy_b     = 0.0f;
    float cauchy_c     = 0.0f;
    float3 albedo      = {1.0f, 1.0f, 1.0f};
    float emission     = 0.0f;
    float3 absorption  = {0.0f, 0.0f, 0.0f};
    float _pad         = 0.0f;
};

static_assert(sizeof(gpu_sdf_primitive) == 80, "gpu_sdf_primitive must be 80 bytes to match GLSL std430 layout");

// ============================================================================
// Scene Primitive (C++ side, richer for UI)
// ============================================================================

struct scene_primitive
{
    prim_type type       = prim_type::circle;
    material_type mat    = material_type::diffuse;
    float2 position      = {0.0f, 0.0f};
    float rotation       = 0.0f;
    float4 params        = {1.0f, 0.0f, 0.0f, 0.0f};
    float3 albedo        = {1.0f, 1.0f, 1.0f};
    float emission       = 0.0f;
    float ior_base       = 1.5f;
    float cauchy_b       = 0.0f;
    float cauchy_c       = 0.0f;
    float3 absorption    = {0.0f, 0.0f, 0.0f};
    bool selected        = false;

    gpu_sdf_primitive pack() const
    {
        gpu_sdf_primitive g;
        g.position   = position;
        g.rotation   = rotation;
        g.prim       = static_cast<uint32_t>(type);
        g.params     = params;
        g.material   = static_cast<uint32_t>(mat);
        g.ior_base   = ior_base;
        g.cauchy_b   = cauchy_b;
        g.cauchy_c   = cauchy_c;
        g.albedo     = albedo;
        g.emission   = emission;
        g.absorption = absorption;
        g._pad       = 0.0f;
        return g;
    }
};

// ============================================================================
// Path Tracer Config
// ============================================================================

struct path_tracer_config
{
    int32_t max_bounces         = 8;
    int32_t samples_per_frame   = 1;
    float environment_intensity = 0.05f;
    float firefly_clamp         = 100.0f;
    float camera_zoom           = 1.0f;
    float2 camera_center        = {0.0f, 0.0f};
    float exposure              = 1.0f;
};

// ============================================================================
// CPU-side SDF evaluation for selection
// ============================================================================

inline float2 rotate_2d(float2 p, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return {c * p.x + s * p.y, -s * p.x + c * p.y};
}

inline float sdf_circle(float2 p, float r) { return length(p) - r; }

inline float sdf_box(float2 p, float2 half_size)
{
    float dx = std::abs(p.x) - half_size.x;
    float dy = std::abs(p.y) - half_size.y;
    float2 clamped = {(dx > 0.0f ? dx : 0.0f), (dy > 0.0f ? dy : 0.0f)};
    float inner = (dx > dy) ? dx : dy;
    return length(clamped) + (inner < 0.0f ? inner : 0.0f);
}

inline float sdf_capsule(float2 p, float r, float half_len)
{
    p.x -= clamp(p.x, -half_len, half_len);
    return length(p) - r;
}

inline float sdf_segment(float2 p, float half_len, float thickness)
{
    p.x -= clamp(p.x, -half_len, half_len);
    return length(p) - thickness;
}

inline float sdf_lens(float2 p, float r1, float r2, float d)
{
    float half_d = d * 0.5f;
    float d1 = length(p - float2{-half_d, 0.0f}) - r1;
    float d2 = length(p - float2{half_d, 0.0f}) - r2;
    return std::max(d1, d2);
}

inline float sdf_ngon(float2 p, float r, float sides)
{
    float n = std::max(sides, 3.0f);
    float an = 3.14159265f / n;
    float he = r * std::cos(an);
    float angle = std::atan2(p.y, p.x);
    float sector = std::fmod(angle + an + 100.0f * 2.0f * an, 2.0f * an) - an;
    float2 q = {length(p) * std::cos(sector), length(p) * std::abs(std::sin(sector))};
    return q.x - he;
}

inline float eval_primitive_cpu(float2 world_pos, const scene_primitive & sp)
{
    float2 local_p = rotate_2d(world_pos - sp.position, -sp.rotation);
    switch (sp.type)
    {
        case prim_type::circle:  return sdf_circle(local_p, sp.params.x);
        case prim_type::box:     return sdf_box(local_p, {sp.params.x, sp.params.y});
        case prim_type::capsule: return sdf_capsule(local_p, sp.params.x, sp.params.y);
        case prim_type::segment: return sdf_segment(local_p, sp.params.x, sp.params.y);
        case prim_type::lens:    return sdf_lens(local_p, sp.params.x, sp.params.y, sp.params.z);
        case prim_type::ngon:    return sdf_ngon(local_p, sp.params.x, sp.params.y);
        default:                 return 1e10f;
    }
}

// ============================================================================
// Utility
// ============================================================================

inline void uniform3f(const gl_shader_compute & shader, const std::string & name, const float3 & v)
{
    glProgramUniform3fv(shader.handle(), shader.get_uniform_location(name), 1, &v.x);
}

// ============================================================================
// Application
// ============================================================================

struct sample_2d_pathtracer final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    path_tracer_config config;
    std::vector<scene_primitive> scene;

    // Shaders
    gl_shader_compute trace_compute;
    gl_shader display_shader;

    // Accumulation (RGBA32F compute image)
    gl_texture_2d accumulation_texture;

    // Primitives SSBO
    gl_buffer primitives_ssbo;

    // HDR FBO for display pass
    gl_framebuffer hdr_framebuffer;
    gl_texture_2d hdr_color_texture;

    // Post-processing
    gl_effect_composer composer;
    std::shared_ptr<gl_unreal_bloom> bloom_pass;

    // Empty VAO for fullscreen triangle
    gl_vertex_array_object empty_vao;

    // State
    int32_t current_width = 0;
    int32_t current_height = 0;
    int32_t frame_index = 0;
    bool scene_dirty = true;

    // Selection and interaction
    int32_t selected_index = -1;
    bool left_mouse_down = false;
    bool right_mouse_down = false;
    bool dragging = false;
    float2 last_cursor = {0.0f, 0.0f};
    float2 drag_offset = {0.0f, 0.0f};

    // Pending add primitive type (-1 = none)
    int32_t pending_add_type = -1;

    sample_2d_pathtracer();
    ~sample_2d_pathtracer() = default;

    float2 cursor_to_world(float2 cursor_px) const;
    int32_t pick_primitive(float2 world_pos) const;

    void build_default_scene();
    void upload_scene();
    void setup_accumulation(int32_t width, int32_t height);
    void setup_hdr_framebuffer(int32_t width, int32_t height);
    void clear_accumulation();
    void add_primitive(prim_type type, float2 world_pos);

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_window_resize(int2 size) override;
};

float2 sample_2d_pathtracer::cursor_to_world(float2 cursor_px) const
{
    float ndc_x = (cursor_px.x / static_cast<float>(current_width)) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (cursor_px.y / static_cast<float>(current_height)) * 2.0f;
    float aspect = static_cast<float>(current_width) / static_cast<float>(current_height);
    return float2{ndc_x * aspect, ndc_y} / config.camera_zoom + config.camera_center;
}

int32_t sample_2d_pathtracer::pick_primitive(float2 world_pos) const
{
    float best_dist = 1e10f;
    int32_t best_id = -1;
    for (int32_t i = 0; i < static_cast<int32_t>(scene.size()); ++i)
    {
        float d = eval_primitive_cpu(world_pos, scene[i]);
        if (d < best_dist)
        {
            best_dist = d;
            best_id = i;
        }
    }
    if (best_dist > 0.5f) return -1;
    return best_id;
}

void sample_2d_pathtracer::add_primitive(prim_type type, float2 world_pos)
{
    scene_primitive sp;
    sp.type = type;
    sp.position = world_pos;
    sp.mat = material_type::diffuse;

    switch (type)
    {
        case prim_type::circle:  sp.params = {0.5f, 0.0f, 0.0f, 0.0f}; break;
        case prim_type::box:     sp.params = {0.5f, 0.5f, 0.0f, 0.0f}; break;
        case prim_type::capsule: sp.params = {0.2f, 0.5f, 0.0f, 0.0f}; break;
        case prim_type::segment: sp.params = {0.5f, 0.05f, 0.0f, 0.0f}; break;
        case prim_type::lens:    sp.params = {0.8f, 0.8f, 0.6f, 0.0f}; break;
        case prim_type::ngon:    sp.params = {0.5f, 6.0f, 0.0f, 0.0f}; break;
    }

    sp.albedo = {0.8f, 0.8f, 0.8f};
    scene.push_back(sp);
    selected_index = static_cast<int32_t>(scene.size()) - 1;
    scene_dirty = true;
}

sample_2d_pathtracer::sample_2d_pathtracer() : polymer_app(1920, 1080, "2d-pathtracer", 4)
{
    glfwMakeContextCurrent(window);

    imgui.reset(new imgui_instance(window, true));
    gui::make_light_theme();

    const std::string asset_base = global_asset_dir::get()->get_asset_dir();
    const std::string shader_base = asset_base + "/shaders/2d-pathtracer/";

    std::string common_src = read_file_text(shader_base + "pt_common.glsl");
    std::string trace_src = read_file_text(shader_base + "pt_trace_comp.glsl");
    trace_compute = gl_shader_compute(common_src + "\n" + trace_src);

    std::string fullscreen_vert = read_file_text(asset_base + "/shaders/waterfall_fullscreen_vert.glsl");
    std::string display_frag = read_file_text(shader_base + "pt_display_frag.glsl");
    display_shader = gl_shader(fullscreen_vert, display_frag);

    bloom_pass = std::make_shared<gl_unreal_bloom>(asset_base);
    bloom_pass->config.threshold = 0.2f;
    bloom_pass->config.strength = 0.8f;
    bloom_pass->config.radius = 0.5f;
    bloom_pass->config.exposure = 1.0f;
    bloom_pass->config.tonemap_mode = 3;
    composer.add_pass(bloom_pass);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    current_width = width;
    current_height = height;

    setup_accumulation(width, height);
    setup_hdr_framebuffer(width, height);
    composer.resize(width, height);

    build_default_scene();

    gl_check_error(__FILE__, __LINE__);
}

void sample_2d_pathtracer::build_default_scene()
{
    scene.clear();

    // Emissive circle (light source)
    {
        scene_primitive light;
        light.type = prim_type::circle;
        light.mat = material_type::diffuse;
        light.position = {0.0f, 2.5f};
        light.params = {0.4f, 0.0f, 0.0f, 0.0f};
        light.albedo = {1.0f, 0.95f, 0.9f};
        light.emission = 15.0f;
        scene.push_back(light);
    }

    // Floor
    {
        scene_primitive floor;
        floor.type = prim_type::box;
        floor.mat = material_type::diffuse;
        floor.position = {0.0f, -2.0f};
        floor.params = {4.0f, 0.3f, 0.0f, 0.0f};
        floor.albedo = {0.8f, 0.8f, 0.8f};
        scene.push_back(floor);
    }

    // Left wall (red)
    {
        scene_primitive wall;
        wall.type = prim_type::box;
        wall.mat = material_type::diffuse;
        wall.position = {-3.5f, 0.0f};
        wall.params = {0.3f, 3.0f, 0.0f, 0.0f};
        wall.albedo = {0.8f, 0.2f, 0.2f};
        scene.push_back(wall);
    }

    // Right wall (green)
    {
        scene_primitive wall;
        wall.type = prim_type::box;
        wall.mat = material_type::diffuse;
        wall.position = {3.5f, 0.0f};
        wall.params = {0.3f, 3.0f, 0.0f, 0.0f};
        wall.albedo = {0.2f, 0.8f, 0.2f};
        scene.push_back(wall);
    }

    // Ceiling
    {
        scene_primitive ceiling;
        ceiling.type = prim_type::box;
        ceiling.mat = material_type::diffuse;
        ceiling.position = {0.0f, 3.0f};
        ceiling.params = {4.0f, 0.3f, 0.0f, 0.0f};
        ceiling.albedo = {0.8f, 0.8f, 0.8f};
        scene.push_back(ceiling);
    }

    // Glass sphere
    {
        scene_primitive sphere;
        sphere.type = prim_type::circle;
        sphere.mat = material_type::glass;
        sphere.position = {0.0f, -1.0f};
        sphere.params = {0.7f, 0.0f, 0.0f, 0.0f};
        sphere.albedo = {1.0f, 1.0f, 1.0f};
        sphere.ior_base = 1.5f;
        sphere.cauchy_b = 0.004f;
        sphere.cauchy_c = 0.0f;
        scene.push_back(sphere);
    }

    selected_index = -1;
    scene_dirty = true;
}

void sample_2d_pathtracer::upload_scene()
{
    std::vector<gpu_sdf_primitive> gpu_prims;
    gpu_prims.reserve(scene.size());
    for (const scene_primitive & sp : scene)
    {
        gpu_prims.push_back(sp.pack());
    }

    GLsizeiptr byte_size = static_cast<GLsizeiptr>(gpu_prims.size()) * sizeof(gpu_sdf_primitive);
    if (byte_size > 0)
    {
        primitives_ssbo.set_buffer_data(byte_size, gpu_prims.data(), GL_DYNAMIC_DRAW);
    }
    else
    {
        gpu_sdf_primitive dummy;
        primitives_ssbo.set_buffer_data(sizeof(gpu_sdf_primitive), &dummy, GL_DYNAMIC_DRAW);
    }
}

void sample_2d_pathtracer::setup_accumulation(int32_t width, int32_t height)
{
    accumulation_texture = gl_texture_2d();
    accumulation_texture.setup(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void sample_2d_pathtracer::setup_hdr_framebuffer(int32_t width, int32_t height)
{
    hdr_color_texture.setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glNamedFramebufferTexture(hdr_framebuffer, GL_COLOR_ATTACHMENT0, hdr_color_texture, 0);
    hdr_framebuffer.check_complete();

    gl_check_error(__FILE__, __LINE__);
}

void sample_2d_pathtracer::clear_accumulation()
{
    float clear_val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(accumulation_texture, 0, GL_RGBA, GL_FLOAT, clear_val);
    frame_index = 0;
}

void sample_2d_pathtracer::on_window_resize(int2 size)
{
    if (size.x == current_width && size.y == current_height) return;
    current_width = size.x;
    current_height = size.y;

    accumulation_texture = gl_texture_2d();
    hdr_color_texture = gl_texture_2d();
    hdr_framebuffer = gl_framebuffer();

    setup_accumulation(size.x, size.y);
    setup_hdr_framebuffer(size.x, size.y);
    composer.resize(size.x, size.y);

    clear_accumulation();
}

void sample_2d_pathtracer::on_input(const app_input_event & event)
{
    imgui->update_input(event);

    if (ImGui::GetIO().WantCaptureMouse) return;

    // Left click: select or place primitive
    if (event.type == app_input_event::MOUSE && event.value.x == GLFW_MOUSE_BUTTON_LEFT)
    {
        left_mouse_down = event.is_down();
        float2 world = cursor_to_world(last_cursor);

        if (event.is_down())
        {
            if (pending_add_type >= 0)
            {
                add_primitive(static_cast<prim_type>(pending_add_type), world);
                pending_add_type = -1;
            }
            else
            {
                int32_t picked = pick_primitive(world);
                selected_index = picked;
                dragging = (picked >= 0);
                if (dragging) drag_offset = scene[picked].position - world;
            }
        }
        else
        {
            dragging = false;
        }
    }

    // Right click drag: pan camera
    if (event.type == app_input_event::MOUSE && event.value.x == GLFW_MOUSE_BUTTON_RIGHT)
    {
        right_mouse_down = event.is_down();
    }

    if (event.type == app_input_event::CURSOR)
    {
        float2 cursor = {static_cast<float>(event.cursor.x), static_cast<float>(event.cursor.y)};

        if (dragging && left_mouse_down && selected_index >= 0)
        {
            float2 world = cursor_to_world(cursor);
            scene[selected_index].position = world + drag_offset;
            scene_dirty = true;
        }
        else if (right_mouse_down)
        {
            float2 delta = cursor - last_cursor;
            float scale = 2.0f / (config.camera_zoom * static_cast<float>(current_height));
            config.camera_center.x -= delta.x * scale;
            config.camera_center.y += delta.y * scale;
            scene_dirty = true;
        }

        last_cursor = cursor;
    }

    // Scroll to zoom
    if (event.type == app_input_event::SCROLL)
    {
        float zoom_factor = 1.1f;
        if (event.value.y > 0) config.camera_zoom *= zoom_factor;
        else if (event.value.y < 0) config.camera_zoom /= zoom_factor;
        config.camera_zoom = clamp(config.camera_zoom, 0.1f, 50.0f);
        scene_dirty = true;
    }

    // Delete key
    if (event.type == app_input_event::KEY && event.value.x == GLFW_KEY_DELETE && event.is_down())
    {
        if (selected_index >= 0 && selected_index < static_cast<int32_t>(scene.size()))
        {
            scene.erase(scene.begin() + selected_index);
            selected_index = -1;
            scene_dirty = true;
        }
    }
}

void sample_2d_pathtracer::on_update(const app_update_event & e) {}

void sample_2d_pathtracer::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (width != current_width || height != current_height)
    {
        on_window_resize({width, height});
    }

    if (scene_dirty)
    {
        upload_scene();
        clear_accumulation();
        scene_dirty = false;
    }

    // ========================================================================
    // Compute pass: path trace + accumulate
    // ========================================================================
    {
        trace_compute.bind();
        trace_compute.bind_ssbo(0, primitives_ssbo);
        trace_compute.bind_image(1, accumulation_texture, GL_READ_WRITE, GL_RGBA32F);

        trace_compute.uniform("u_num_prims", static_cast<int>(scene.size()));
        trace_compute.uniform("u_frame_index", frame_index);
        trace_compute.uniform("u_max_bounces", config.max_bounces);
        trace_compute.uniform("u_samples_per_frame", config.samples_per_frame);
        trace_compute.uniform("u_environment_intensity", config.environment_intensity);
        trace_compute.uniform("u_firefly_clamp", config.firefly_clamp);
        trace_compute.uniform("u_camera_zoom", config.camera_zoom);
        trace_compute.uniform("u_camera_center", config.camera_center);
        trace_compute.uniform("u_resolution", float2(static_cast<float>(width), static_cast<float>(height)));

        uint32_t groups_x = (width + 15) / 16;
        uint32_t groups_y = (height + 15) / 16;
        trace_compute.dispatch_and_barrier(groups_x, groups_y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        trace_compute.unbind();

        frame_index++;
    }

    // ========================================================================
    // Display pass
    // ========================================================================
    {
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_framebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        display_shader.bind();
        display_shader.texture("u_accumulation_tex", 0, accumulation_texture, GL_TEXTURE_2D);
        display_shader.uniform("u_exposure", config.exposure);

        glBindVertexArray(empty_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        display_shader.unbind();
    }

    // ========================================================================
    // Post-processing -> backbuffer
    // ========================================================================
    composer.render(hdr_color_texture, width, height);

    // ========================================================================
    // ImGui
    // ========================================================================
    imgui->begin_frame();
    gui::imgui_fixed_window_begin("2D Path Tracer", ui_rect{{0, 0}, {320, height}});

    int32_t total_samples = frame_index * config.samples_per_frame;
    ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text("Samples: %d", total_samples);
    ImGui::Separator();

    // ------ Scene Controls ------
    if (ImGui::CollapsingHeader("Scene Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::SliderInt("Max Bounces", &config.max_bounces, 1, 32)) scene_dirty = true;
        if (ImGui::SliderInt("Samples/Frame", &config.samples_per_frame, 1, 16)) scene_dirty = true;
        if (ImGui::SliderFloat("Environment", &config.environment_intensity, 0.0f, 1.0f)) scene_dirty = true;
        if (ImGui::SliderFloat("Firefly Clamp", &config.firefly_clamp, 1.0f, 1000.0f, "%.0f")) scene_dirty = true;
        ImGui::SliderFloat("Exposure", &config.exposure, 0.1f, 10.0f);

        if (ImGui::Button("Reset Accumulation")) clear_accumulation();
        ImGui::SameLine();
        if (ImGui::Button("Reset Scene")) build_default_scene();
    }

    // ------ Camera ------
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::SliderFloat("Zoom", &config.camera_zoom, 0.1f, 10.0f)) scene_dirty = true;
        if (ImGui::SliderFloat2("Center", &config.camera_center.x, -10.0f, 10.0f)) scene_dirty = true;
    }

    // ------ Add Primitive ------
    if (ImGui::CollapsingHeader("Add Primitive", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char * labels[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon"};
        for (int32_t i = 0; i < 6; ++i)
        {
            if (i > 0) ImGui::SameLine();
            bool is_pending = (pending_add_type == i);
            if (is_pending) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
            if (ImGui::Button(labels[i]))
            {
                pending_add_type = is_pending ? -1 : i;
            }
            if (is_pending) ImGui::PopStyleColor();
        }
        if (pending_add_type >= 0) ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Click canvas to place");
    }

    // ------ Primitive List ------
    if (ImGui::CollapsingHeader("Primitives", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char * type_names[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon"};
        const char * mat_names[] = {"Diffuse", "Mirror", "Glass", "Water", "Diamond"};

        for (int32_t i = 0; i < static_cast<int32_t>(scene.size()); ++i)
        {
            scene_primitive & sp = scene[i];
            ImGui::PushID(i);

            bool is_selected = (i == selected_index);

            char label[128];
            snprintf(label, sizeof(label), "%s %d (%s)%s",
                type_names[static_cast<int>(sp.type)], i,
                mat_names[static_cast<int>(sp.mat)],
                sp.emission > 0.0f ? " [E]" : "");

            if (ImGui::Selectable(label, is_selected))
            {
                selected_index = is_selected ? -1 : i;
            }

            ImGui::PopID();
        }

        if (selected_index >= 0 && ImGui::Button("Delete Selected"))
        {
            scene.erase(scene.begin() + selected_index);
            selected_index = -1;
            scene_dirty = true;
        }
    }

    // ------ Selected Primitive Properties ------
    if (selected_index >= 0 && selected_index < static_cast<int32_t>(scene.size()))
    {
        if (ImGui::CollapsingHeader("Selected Primitive", ImGuiTreeNodeFlags_DefaultOpen))
        {
            scene_primitive & sp = scene[selected_index];
            const char * type_names[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon"};
            const char * mat_names[] = {"Diffuse", "Mirror", "Glass", "Water", "Diamond"};

            bool changed = false;
            changed |= ImGui::DragFloat2("Position", &sp.position.x, 0.05f);
            changed |= ImGui::SliderFloat("Rotation", &sp.rotation, -3.14159f, 3.14159f);

            int type_idx = static_cast<int>(sp.type);
            if (ImGui::Combo("Shape", &type_idx, type_names, IM_ARRAYSIZE(type_names)))
            {
                sp.type = static_cast<prim_type>(type_idx);
                changed = true;
            }

            int mat_idx = static_cast<int>(sp.mat);
            if (ImGui::Combo("Material", &mat_idx, mat_names, IM_ARRAYSIZE(mat_names)))
            {
                sp.mat = static_cast<material_type>(mat_idx);
                changed = true;

                // Auto-fill IOR/Cauchy/absorption for refractive materials
                switch (sp.mat)
                {
                    case material_type::glass:
                        sp.ior_base = 1.5f; sp.cauchy_b = 0.004f; sp.cauchy_c = 0.0f;
                        sp.absorption = {0.0f, 0.0f, 0.0f};
                        break;
                    case material_type::water:
                        sp.ior_base = 1.333f; sp.cauchy_b = 0.003f; sp.cauchy_c = 0.0f;
                        sp.absorption = {0.2f, 0.05f, 0.01f};
                        break;
                    case material_type::diamond:
                        sp.ior_base = 2.42f; sp.cauchy_b = 0.044f; sp.cauchy_c = 0.001f;
                        sp.absorption = {0.0f, 0.0f, 0.0f};
                        break;
                    default: break;
                }
            }

            // Shape-specific params
            switch (sp.type)
            {
                case prim_type::circle:
                    changed |= ImGui::DragFloat("Radius", &sp.params.x, 0.01f, 0.01f, 10.0f);
                    break;
                case prim_type::box:
                    changed |= ImGui::DragFloat("Half Width", &sp.params.x, 0.01f, 0.01f, 10.0f);
                    changed |= ImGui::DragFloat("Half Height", &sp.params.y, 0.01f, 0.01f, 10.0f);
                    break;
                case prim_type::capsule:
                    changed |= ImGui::DragFloat("Radius##cap", &sp.params.x, 0.01f, 0.01f, 5.0f);
                    changed |= ImGui::DragFloat("Half Length", &sp.params.y, 0.01f, 0.01f, 10.0f);
                    break;
                case prim_type::segment:
                    changed |= ImGui::DragFloat("Half Length##seg", &sp.params.x, 0.01f, 0.01f, 10.0f);
                    changed |= ImGui::DragFloat("Thickness", &sp.params.y, 0.005f, 0.005f, 1.0f);
                    break;
                case prim_type::lens:
                    changed |= ImGui::DragFloat("Radius 1", &sp.params.x, 0.01f, 0.1f, 5.0f);
                    changed |= ImGui::DragFloat("Radius 2", &sp.params.y, 0.01f, 0.1f, 5.0f);
                    changed |= ImGui::DragFloat("Distance", &sp.params.z, 0.01f, 0.0f, 5.0f);
                    break;
                case prim_type::ngon:
                    changed |= ImGui::DragFloat("Radius##ngon", &sp.params.x, 0.01f, 0.01f, 5.0f);
                    changed |= ImGui::DragFloat("Sides", &sp.params.y, 0.1f, 3.0f, 12.0f);
                    break;
            }

            changed |= ImGui::ColorEdit3("Albedo", &sp.albedo.x);
            changed |= ImGui::DragFloat("Emission", &sp.emission, 0.1f, 0.0f, 100.0f);

            if (static_cast<uint32_t>(sp.mat) >= 2)
            {
                ImGui::Separator();
                changed |= ImGui::SliderFloat("IOR Base", &sp.ior_base, 1.0f, 3.0f);
                changed |= ImGui::SliderFloat("Cauchy B", &sp.cauchy_b, 0.0f, 0.05f, "%.4f");
                changed |= ImGui::SliderFloat("Cauchy C", &sp.cauchy_c, 0.0f, 0.01f, "%.5f");
                changed |= ImGui::ColorEdit3("Absorption", &sp.absorption.x);
            }

            if (changed) scene_dirty = true;
        }
    }

    // ------ Bloom ------
    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &bloom_pass->config.bloom_enabled);
        ImGui::SliderFloat("Threshold", &bloom_pass->config.threshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Knee", &bloom_pass->config.knee, 0.0f, 1.0f);
        ImGui::SliderFloat("Strength", &bloom_pass->config.strength, 0.0f, 3.0f);
        ImGui::SliderFloat("Radius", &bloom_pass->config.radius, 0.0f, 1.0f);
        ImGui::SliderFloat("Bloom Exposure", &bloom_pass->config.exposure, 0.1f, 5.0f);
        ImGui::SliderFloat("Gamma", &bloom_pass->config.gamma, 1.0f, 3.0f);
        const char * tonemap_modes[] = {"None", "Filmic", "Hejl", "ACES 2.0", "ACES 1.0"};
        ImGui::Combo("Tonemap", &bloom_pass->config.tonemap_mode, tonemap_modes, IM_ARRAYSIZE(tonemap_modes));
    }

    // ------ Presets ------
    if (ImGui::CollapsingHeader("Presets"))
    {
        if (ImGui::Button("Prism"))
        {
            scene.clear();
            selected_index = -1;

            scene_primitive light;
            light.type = prim_type::box;
            light.mat = material_type::diffuse;
            light.position = {-3.0f, 0.0f};
            light.params = {0.1f, 1.5f, 0.0f, 0.0f};
            light.albedo = {1.0f, 1.0f, 1.0f};
            light.emission = 20.0f;
            scene.push_back(light);

            scene_primitive prism;
            prism.type = prim_type::ngon;
            prism.mat = material_type::glass;
            prism.position = {0.0f, 0.0f};
            prism.params = {1.0f, 3.0f, 0.0f, 0.0f};
            prism.albedo = {1.0f, 1.0f, 1.0f};
            prism.ior_base = 1.5f;
            prism.cauchy_b = 0.01f;
            scene.push_back(prism);

            scene_primitive screen;
            screen.type = prim_type::box;
            screen.mat = material_type::diffuse;
            screen.position = {4.0f, 0.0f};
            screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
            screen.albedo = {0.9f, 0.9f, 0.9f};
            scene.push_back(screen);

            scene_dirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Converging Lens"))
        {
            scene.clear();
            selected_index = -1;

            scene_primitive light;
            light.type = prim_type::box;
            light.mat = material_type::diffuse;
            light.position = {-4.0f, 0.0f};
            light.params = {0.1f, 2.0f, 0.0f, 0.0f};
            light.albedo = {1.0f, 1.0f, 1.0f};
            light.emission = 20.0f;
            scene.push_back(light);

            scene_primitive lens;
            lens.type = prim_type::lens;
            lens.mat = material_type::glass;
            lens.position = {0.0f, 0.0f};
            lens.params = {2.0f, 2.0f, 1.5f, 0.0f};
            lens.albedo = {1.0f, 1.0f, 1.0f};
            lens.ior_base = 1.5f;
            lens.cauchy_b = 0.004f;
            scene.push_back(lens);

            scene_primitive screen;
            screen.type = prim_type::box;
            screen.mat = material_type::diffuse;
            screen.position = {4.0f, 0.0f};
            screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
            screen.albedo = {0.9f, 0.9f, 0.9f};
            scene.push_back(screen);

            scene_dirty = true;
        }

        if (ImGui::Button("Diamond"))
        {
            scene.clear();
            selected_index = -1;

            scene_primitive light;
            light.type = prim_type::circle;
            light.mat = material_type::diffuse;
            light.position = {0.0f, 3.0f};
            light.params = {0.5f, 0.0f, 0.0f, 0.0f};
            light.albedo = {1.0f, 1.0f, 1.0f};
            light.emission = 25.0f;
            scene.push_back(light);

            scene_primitive diamond;
            diamond.type = prim_type::ngon;
            diamond.mat = material_type::diamond;
            diamond.position = {0.0f, 0.0f};
            diamond.params = {1.0f, 8.0f, 0.0f, 0.0f};
            diamond.albedo = {1.0f, 1.0f, 1.0f};
            diamond.ior_base = 2.42f;
            diamond.cauchy_b = 0.044f;
            diamond.cauchy_c = 0.001f;
            scene.push_back(diamond);

            scene_primitive floor;
            floor.type = prim_type::box;
            floor.mat = material_type::diffuse;
            floor.position = {0.0f, -2.0f};
            floor.params = {5.0f, 0.3f, 0.0f, 0.0f};
            floor.albedo = {0.9f, 0.9f, 0.9f};
            scene.push_back(floor);

            scene_dirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cornell Box 2D"))
        {
            build_default_scene();
        }
    }

    gui::imgui_fixed_window_end();
    imgui->end_frame();

    glfwSwapBuffers(window);
    gl_check_error(__FILE__, __LINE__);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char * argv[])
{
    try
    {
        sample_2d_pathtracer app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

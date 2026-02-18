#include "scenes.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

using namespace gui;

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

inline float sdf_lens(float2 p, float r1, float r2, float d, float aperture_half_height)
{
    float half_d = d * 0.5f;
    float ar1 = std::max(std::abs(r1), 1e-4f);
    float ar2 = std::max(std::abs(r2), 1e-4f);

    // Vertex positions are fixed at x = +/- half_d. The sign of r controls
    // curvature direction: r > 0 is convex, r < 0 is concave.
    float2 c1 = {-half_d + r1, 0.0f};
    float2 c2 = {half_d - r2, 0.0f};

    float side1 = length(p - c1) - ar1;
    float side2 = length(p - c2) - ar2;

    if (r1 < 0.0f) side1 = -side1;
    if (r2 < 0.0f) side2 = -side2;

    float aperture = (aperture_half_height > 0.0f) ? aperture_half_height : (std::min(ar1, ar2) * 0.98f);
    float cap = std::abs(p.y) - aperture;

    return std::max(std::max(side1, side2), cap);
}

inline float sdf_ngon(float2 p, float r, float sides)
{
    float n = std::max(sides, 3.0f);
    float an = POLYMER_PI / n;
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
        case prim_type::lens:    return sdf_lens(local_p, sp.params.x, sp.params.y, sp.params.z, sp.params.w);
        case prim_type::ngon:    return sdf_ngon(local_p, sp.params.x, sp.params.y);
        default:                 return 1e10f;
    }
}

inline void uniform3f(const gl_shader_compute & shader, const std::string & name, const float3 & v)
{
    glProgramUniform3fv(shader.handle(), shader.get_uniform_location(name), 1, &v.x);
}

inline std::string find_asset_directory(const std::vector<std::string> & search_paths)
{
    for (const std::string & search_path : search_paths)
    {
        std::error_code ec;
        if (!std::filesystem::exists(search_path, ec) || ec) continue;
        if (!std::filesystem::is_directory(search_path, ec) || ec) continue;

        try
        {
            for (auto it = std::filesystem::recursive_directory_iterator(search_path, std::filesystem::directory_options::skip_permission_denied, ec);
                 it != std::filesystem::recursive_directory_iterator() && !ec;
                 it.increment(ec))
            {
                const std::filesystem::path & entry_path = it->path();
                if (std::filesystem::is_directory(entry_path, ec) && !ec && entry_path.filename() == "assets")
                {
                    return entry_path.string();
                }
            }
        }
        catch (const std::filesystem::filesystem_error &) { continue; }
    }

    return {};
}

struct camera_controller_2d
{
    float2 center = {0.0f, 0.0f};
    float zoom = 0.30f;
    bool panning = false;
    float2 last_cursor = {0.0f, 0.0f};

    float2 cursor_to_world(float2 cursor_px, int32_t viewport_w, int32_t viewport_h) const
    {
        float ndc_x = (cursor_px.x / static_cast<float>(viewport_w)) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (cursor_px.y / static_cast<float>(viewport_h)) * 2.0f;
        float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h);
        return float2{ndc_x * aspect, ndc_y} / zoom + center;
    }

    bool handle_scroll(float scroll_y)
    {
        float zoom_factor = 1.1f;
        if (scroll_y > 0) zoom *= zoom_factor;
        else if (scroll_y < 0) zoom /= zoom_factor;
        zoom = clamp(zoom, 0.1f, 50.0f);
        return true;
    }

    bool handle_pan(float2 cursor, int32_t viewport_h)
    {
        float2 delta = cursor - last_cursor;
        float scale = 2.0f / (zoom * static_cast<float>(viewport_h));
        center.x -= delta.x * scale;
        center.y += delta.y * scale;
        return true;
    }

    void update_cursor(float2 cursor) { last_cursor = cursor; }
};

enum class env_interp_mode : int32_t
{
    rgb_linear = 0,
    hsv_shortest = 1,
    hsv_longest = 2
};

inline void to_json(json & archive, const env_interp_mode & p) { archive = static_cast<int32_t>(p); }
inline void from_json(const json & archive, env_interp_mode & p)
{
    const int32_t mode = archive.get<int32_t>();
    p = static_cast<env_interp_mode>(std::clamp(mode, 0, 2));
}

struct env_gradient_stop
{
    float u = 0.0f; // [0, 1)
    float3 color = {0.0f, 0.0f, 0.0f};
};

struct env_lobe
{
    float u = 0.0f; // [0, 1)
    float width = 0.08f; // angular half-width in [0, 0.5]
    float intensity = 2.0f;
    float falloff = 0.7f; // maps to exponent
    float3 color = {1.0f, 1.0f, 1.0f};
};

struct env_composer
{
    bool enabled = false;
    int32_t resolution = 1024;
    env_interp_mode interpolation = env_interp_mode::rgb_linear;
    float gain = 1.0f;
    std::vector<env_gradient_stop> stops;
    std::vector<env_lobe> lobes;
};

template<class F> inline void visit_fields(camera_controller_2d & o, F f)
{
    f("center", o.center);
    f("zoom", o.zoom);
}

inline void to_json(json & j, const camera_controller_2d & p)
{
    j = json::object();
    visit_fields(const_cast<camera_controller_2d &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, camera_controller_2d & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

template<class F> inline void visit_fields(env_gradient_stop & o, F f)
{
    f("u", o.u);
    f("color", o.color);
}

inline void to_json(json & j, const env_gradient_stop & p)
{
    j = json::object();
    visit_fields(const_cast<env_gradient_stop &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, env_gradient_stop & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

template<class F> inline void visit_fields(env_lobe & o, F f)
{
    f("u", o.u);
    f("width", o.width);
    f("intensity", o.intensity);
    f("falloff", o.falloff);
    f("color", o.color);
}

inline void to_json(json & j, const env_lobe & p)
{
    j = json::object();
    visit_fields(const_cast<env_lobe &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, env_lobe & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

template<class F> inline void visit_fields(env_composer & o, F f)
{
    f("enabled", o.enabled);
    f("resolution", o.resolution);
    f("interpolation", o.interpolation);
    f("gain", o.gain);
    f("stops", o.stops);
    f("lobes", o.lobes);
}

inline void to_json(json & j, const env_composer & p)
{
    j = json::object();
    visit_fields(const_cast<env_composer &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, env_composer & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

struct pathtracer_scene_archive
{
    int32_t version = 1;
    path_tracer_config config;
    camera_controller_2d camera;
    std::vector<scene_primitive> primitives;
    env_composer environment;
};

template<class F> inline void visit_fields(pathtracer_scene_archive & o, F f)
{
    f("version", o.version);
    f("config", o.config);
    f("camera", o.camera);
    f("primitives", o.primitives);
    f("environment", o.environment);
}

inline void to_json(json & j, const pathtracer_scene_archive & p)
{
    j = json::object();
    visit_fields(const_cast<pathtracer_scene_archive &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, pathtracer_scene_archive & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

inline float wrap01(float x)
{
    float y = x - std::floor(x);
    if (y < 0.0f) y += 1.0f;
    return y;
}

inline float circular_distance01(float a, float b)
{
    float d = std::abs(a - b);
    return std::min(d, 1.0f - d);
}

inline float3 ui_hsv_to_rgb(float3 hsv)
{
    float rgb[3];
    ImGui::ColorConvertHSVtoRGB(hsv.x, hsv.y, hsv.z, rgb[0], rgb[1], rgb[2]);
    return {rgb[0], rgb[1], rgb[2]};
}

inline float3 ui_rgb_to_hsv(float3 rgb)
{
    float hsv[3];
    ImGui::ColorConvertRGBtoHSV(rgb.x, rgb.y, rgb.z, hsv[0], hsv[1], hsv[2]);
    return {hsv[0], hsv[1], hsv[2]};
}

struct sample_2d_pathtracer final : public polymer_app
{
    struct discovered_scene
    {
        std::string name;
        std::string path;
    };

    std::unique_ptr<imgui_instance> imgui;

    path_tracer_config config;
    std::vector<scene_primitive> scene;

    gl_shader_compute trace_compute;
    gl_shader display_shader;
    gl_texture_2d accumulation_texture;
    gl_buffer primitives_ssbo;
    gl_vertex_array_object empty_vao;
    GLuint environment_texture_1d = 0;

    int32_t current_width = 0;
    int32_t current_height = 0;
    int32_t frame_index = 0;
    bool scene_dirty = true;
    bool show_env_composer_modal = false;
    bool env_dirty = true;

    camera_controller_2d camera;
    env_composer env;
    std::vector<float3> env_baked;
    int32_t env_selected_stop = -1;
    int32_t env_selected_lobe = -1;
    bool env_dragging_stop = false;
    bool env_dragging_lobe = false;

    int32_t selected_index = -1;
    bool left_mouse_down = false;
    bool dragging = false;
    float2 drag_offset = {0.0f, 0.0f};

    int32_t pending_add_type = -1;
    std::string scene_io_status;
    bool scene_io_error = false;
    std::vector<discovered_scene> discovered_scenes;
    int32_t selected_scene_file_index = -1;
    std::string scenes_directory;
    std::string scene_file_path;
    bool open_export_scene_modal = false;
    char export_scene_filename[128] = "scene.json";

    sample_2d_pathtracer();
    ~sample_2d_pathtracer();

    int32_t pick_primitive(float2 world_pos) const;

    void build_default_scene();
    void upload_scene();
    void setup_accumulation(int32_t width, int32_t height);
    void clear_accumulation();
    void add_primitive(prim_type type, float2 world_pos);
    void export_exr();
    bool save_scene_to_file(const std::string & path);
    bool load_scene_from_file(const std::string & path);
    void load_scenes();
    void draw_export_scene_modal();
    void setup_environment_texture();
    void bake_environment_texture();
    float3 eval_environment(float u) const;
    void draw_environment_composer_modal();
    void apply_environment_preset(int preset_id);

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_window_resize(int2 size) override;
};

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
        case prim_type::lens:    sp.params = {0.8f, 0.8f, 0.6f, 0.0f}; 
                                 sp.mat = material_type::glass; 
                                 sp.ior_base = 1.5f; 
                                 sp.cauchy_b = 0.004f; 
                                 break;
        case prim_type::ngon:    sp.params = {0.5f, 6.0f, 0.0f, 0.0f}; break;
    }

    sp.albedo = {0.8f, 0.8f, 0.8f};
    scene.push_back(sp);
    selected_index = static_cast<int32_t>(scene.size()) - 1;
    scene_dirty = true;
}

sample_2d_pathtracer::sample_2d_pathtracer() : polymer_app(1920, 1080, "pathtracer_2D", 1)
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
    display_shader = gl_shader(fullscreen_vert, common_src + "\n" + display_frag);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    current_width = width;
    current_height = height;

    env.enabled = false;
    env.interpolation = env_interp_mode::hsv_shortest;
    env.gain = 1.0f;
    env.resolution = 1024;
    apply_environment_preset(0);
    setup_environment_texture();
    bake_environment_texture();

    setup_accumulation(width, height);

    build_default_scene();
    load_scenes();

    gl_check_error(__FILE__, __LINE__);
}

sample_2d_pathtracer::~sample_2d_pathtracer()
{
    if (environment_texture_1d != 0)
    {
        glDeleteTextures(1, &environment_texture_1d);
        environment_texture_1d = 0;
    }
}

void sample_2d_pathtracer::build_default_scene()
{
    scene = scene_cornell_box();
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

void sample_2d_pathtracer::clear_accumulation()
{
    float clear_val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(accumulation_texture, 0, GL_RGBA, GL_FLOAT, clear_val);
    frame_index = 0;
}

void sample_2d_pathtracer::setup_environment_texture()
{
    int32_t resolution = std::max(env.resolution, 64);
    if (env.resolution != resolution) env.resolution = resolution;

    if (environment_texture_1d != 0) glDeleteTextures(1, &environment_texture_1d);
    glCreateTextures(GL_TEXTURE_1D, 1, &environment_texture_1d);
    glTextureStorage1D(environment_texture_1d, 1, GL_RGB32F, resolution);
    glTextureParameteri(environment_texture_1d, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(environment_texture_1d, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(environment_texture_1d, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

float3 sample_hsv_interp(const float3 & c0, const float3 & c1, float t, env_interp_mode mode)
{
    if (mode == env_interp_mode::rgb_linear) return c0 * (1.0f - t) + c1 * t;

    float3 h0 = ui_rgb_to_hsv(c0);
    float3 h1 = ui_rgb_to_hsv(c1);
    float dh = h1.x - h0.x;
    if (dh > 0.5f) dh -= 1.0f;
    if (dh < -0.5f) dh += 1.0f;

    if (mode == env_interp_mode::hsv_longest)
    {
        if (std::abs(dh) < 0.5f)
        {
            dh = (dh >= 0.0f) ? (dh - 1.0f) : (dh + 1.0f);
        }
    }

    float3 h = {
        wrap01(h0.x + dh * t),
        h0.y * (1.0f - t) + h1.y * t,
        h0.z * (1.0f - t) + h1.z * t
    };
    return ui_hsv_to_rgb(h);
}

float3 sample_gradient_ring(const std::vector<env_gradient_stop> & stops, float u, env_interp_mode mode)
{
    if (stops.empty()) return {0.0f, 0.0f, 0.0f};
    if (stops.size() == 1) return stops[0].color;

    std::vector<size_t> sorted(stops.size());
    for (size_t i = 0; i < stops.size(); ++i) sorted[i] = i;
    std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) { return stops[a].u < stops[b].u; });

    for (size_t i = 0; i < sorted.size(); ++i)
    {
        const env_gradient_stop & a = stops[sorted[i]];
        const env_gradient_stop & b = stops[sorted[(i + 1) % sorted.size()]];
        float ua = a.u;
        float ub = b.u;
        if (i + 1 == sorted.size()) ub += 1.0f;

        float x = u;
        if (x < ua) x += 1.0f;
        if (x >= ua && x <= ub)
        {
            float t = (ub > ua) ? (x - ua) / (ub - ua) : 0.0f;
            return sample_hsv_interp(a.color, b.color, t, mode);
        }
    }

    return stops[sorted[0]].color;
}

float3 sample_lobes(const std::vector<env_lobe> & lobes, float u)
{
    float3 sum = {0.0f, 0.0f, 0.0f};
    for (const env_lobe & l : lobes)
    {
        float w = std::max(l.width, 1e-4f);
        float du = circular_distance01(u, l.u);
        if (du > w) continue;

        float x = 1.0f - du / w;
        float exponent = 1.0f + l.falloff * 15.0f;
        float shape = std::pow(std::max(x, 0.0f), exponent);
        sum += l.color * (l.intensity * shape);
    }
    return sum;
}

float3 sample_2d_pathtracer::eval_environment(float u) const
{
    float3 grad = sample_gradient_ring(env.stops, wrap01(u), env.interpolation);
    float3 lobe = sample_lobes(env.lobes, wrap01(u));
    return grad + lobe;
}

void sample_2d_pathtracer::bake_environment_texture()
{
    env.resolution = std::max(env.resolution, 64);
    if (environment_texture_1d == 0) setup_environment_texture();

    env_baked.resize(static_cast<size_t>(env.resolution));
    std::vector<float> upload(static_cast<size_t>(env.resolution) * 3);

    for (int32_t i = 0; i < env.resolution; ++i)
    {
        float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(env.resolution);
        float3 c = eval_environment(u) * env.gain;
        c = {
            std::max(0.0f, static_cast<float>(c.x)),
            std::max(0.0f, static_cast<float>(c.y)),
            std::max(0.0f, static_cast<float>(c.z))
        };
        env_baked[static_cast<size_t>(i)] = c;
        upload[3 * i + 0] = c.x;
        upload[3 * i + 1] = c.y;
        upload[3 * i + 2] = c.z;
    }

    glTextureSubImage1D(environment_texture_1d, 0, 0, env.resolution, GL_RGB, GL_FLOAT, upload.data());
    env_dirty = false;
}

void sample_2d_pathtracer::apply_environment_preset(int preset_id)
{
    env.stops.clear();
    env.lobes.clear();

    switch (preset_id)
    {
        case 0: // Single hard sun
            env.stops.push_back({0.00f, {0.01f, 0.01f, 0.02f}});
            env.stops.push_back({0.50f, {0.02f, 0.02f, 0.03f}});
            env.lobes.push_back({0.12f, 0.05f, 24.0f, 0.9f, {1.0f, 0.96f, 0.85f}});
            break;
        case 1: // Dual key
            env.stops.push_back({0.00f, {0.00f, 0.00f, 0.00f}});
            env.stops.push_back({0.50f, {0.015f, 0.02f, 0.03f}});
            env.lobes.push_back({0.16f, 0.08f, 14.0f, 0.7f, {1.0f, 0.85f, 0.65f}});
            env.lobes.push_back({0.63f, 0.07f, 10.0f, 0.8f, {0.55f, 0.75f, 1.0f}});
            break;
        case 2: // Gradient sky
            env.stops.push_back({0.00f, {0.06f, 0.08f, 0.15f}});
            env.stops.push_back({0.25f, {0.22f, 0.30f, 0.55f}});
            env.stops.push_back({0.50f, {0.10f, 0.12f, 0.20f}});
            env.stops.push_back({0.75f, {0.02f, 0.02f, 0.04f}});
            break;
        case 3: // Neon arc
            env.stops.push_back({0.00f, {0.01f, 0.01f, 0.01f}});
            env.stops.push_back({0.50f, {0.00f, 0.00f, 0.00f}});
            env.lobes.push_back({0.31f, 0.18f, 11.0f, 0.25f, {0.2f, 1.0f, 0.8f}});
            env.lobes.push_back({0.33f, 0.06f, 20.0f, 0.85f, {0.05f, 0.85f, 0.65f}});
            break;
        case 4: // Striped angular lights
            env.stops.push_back({0.00f, {0.005f, 0.005f, 0.005f}});
            env.stops.push_back({0.50f, {0.0f, 0.0f, 0.0f}});
            for (int i = 0; i < 12; ++i)
            {
                float u = (static_cast<float>(i) + 0.25f) / 12.0f;
                float hue = wrap01(0.1f + i * 0.083f);
                float3 rgb = ui_hsv_to_rgb({hue, 0.7f, 1.0f});
                env.lobes.push_back({u, 0.03f, 8.0f, 0.75f, rgb});
            }
            break;
        default:
            env.stops.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
            break;
    }

    env_selected_stop = env.stops.empty() ? -1 : 0;
    env_selected_lobe = env.lobes.empty() ? -1 : 0;
    env_dirty = true;
}

void sample_2d_pathtracer::export_exr()
{
    std::vector<float> rgba(current_width * current_height * 4);
    glGetTextureImage(accumulation_texture, 0, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(rgba.size() * sizeof(float)), rgba.data());

    std::vector<float> rgb(current_width * current_height * 3);
    for (int32_t y = 0; y < current_height; ++y)
    {
        int32_t flipped_y = current_height - 1 - y;
        for (int32_t x = 0; x < current_width; ++x)
        {
            int32_t src = (flipped_y * current_width + x) * 4;
            int32_t dst = (y * current_width + x) * 3;
            float sample_count = rgba[src + 3];
            if (sample_count > 0.0f)
            {
                float inv = 1.0f / sample_count;
                rgb[dst + 0] = rgba[src + 0] * inv;
                rgb[dst + 1] = rgba[src + 1] * inv;
                rgb[dst + 2] = rgba[src + 2] * inv;
            }
            else
            {
                rgb[dst + 0] = 0.0f;
                rgb[dst + 1] = 0.0f;
                rgb[dst + 2] = 0.0f;
            }
        }
    }

    std::string filename = "pathtracer_" + make_timestamp() + ".exr";
    export_exr_image(filename, current_width, current_height, 3, rgb);
}

bool sample_2d_pathtracer::save_scene_to_file(const std::string & path)
{
    try
    {
        const std::filesystem::path output_path(path);
        const std::filesystem::path parent = output_path.parent_path();
        if (!parent.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }

        pathtracer_scene_archive archive;
        archive.config = config;
        archive.camera = camera;
        archive.primitives = scene;
        archive.environment = env;

        json j = archive;
        write_file_text(output_path.string(), j.dump(4));
        scene_file_path = output_path.string();
        scene_io_status = "Saved scene to " + path;
        scene_io_error = false;
        return true;
    }
    catch (const std::exception & e)
    {
        scene_io_status = std::string("Save failed: ") + e.what();
        scene_io_error = true;
        return false;
    }
}

bool sample_2d_pathtracer::load_scene_from_file(const std::string & path)
{
    try
    {
        const std::string content = read_file_text(path);
        const json j = json::parse(content);
        const pathtracer_scene_archive archive = j.get<pathtracer_scene_archive>();

        config = archive.config;
        camera = archive.camera;
        scene = archive.primitives;
        env = archive.environment;
        env.resolution = std::max(env.resolution, 64);
        setup_environment_texture();
        env_selected_stop = env.stops.empty() ? -1 : 0;
        env_selected_lobe = env.lobes.empty() ? -1 : 0;
        env_dragging_stop = false;
        env_dragging_lobe = false;
        selected_index = -1;
        pending_add_type = -1;
        scene_dirty = true;
        env_dirty = true;
        clear_accumulation();

        scene_file_path = path;
        scene_io_status = "Loaded scene from " + path;
        scene_io_error = false;
        return true;
    }
    catch (const std::exception & e)
    {
        scene_io_status = std::string("Load failed: ") + e.what();
        scene_io_error = true;
        return false;
    }
}

void sample_2d_pathtracer::load_scenes()
{
    discovered_scenes.clear();

    std::vector<std::string> search_paths =
    {
        std::filesystem::current_path().string(),
        std::filesystem::current_path().parent_path().string(),
        std::filesystem::current_path().parent_path().parent_path().string(),
        std::filesystem::current_path().parent_path().parent_path().parent_path().string()
    };

    const std::string asset_dir = find_asset_directory(search_paths);
    if (asset_dir.empty())
    {
        scene_io_status = "Scene discovery failed: assets directory not found";
        scene_io_error = true;
        selected_scene_file_index = -1;
        return;
    }

    const std::filesystem::path scene_dir = (std::filesystem::path(asset_dir) / ".." / "samples" / "2d-pathtracer" / "scenes").lexically_normal();
    scenes_directory = scene_dir.string();

    if (!std::filesystem::exists(scene_dir))
    {
        selected_scene_file_index = -1;
        scene_io_status = "Scene directory not found: " + scenes_directory;
        scene_io_error = true;
        return;
    }

    for (const auto & entry : std::filesystem::directory_iterator(scene_dir))
    {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".json") continue;

        discovered_scene scene_entry;
        scene_entry.name = entry.path().stem().string();
        scene_entry.path = entry.path().string();
        discovered_scenes.push_back(std::move(scene_entry));
    }

    std::sort(discovered_scenes.begin(), discovered_scenes.end(), [](const discovered_scene & a, const discovered_scene & b) { return a.name < b.name; });

    if (discovered_scenes.empty())
    {
        selected_scene_file_index = -1;
        scene_io_status = "No JSON scenes found in " + scenes_directory;
        scene_io_error = false;
        return;
    }

    int32_t matched_index = -1;
    if (!scene_file_path.empty())
    {
        for (int32_t i = 0; i < static_cast<int32_t>(discovered_scenes.size()); ++i)
        {
            if (discovered_scenes[i].path == scene_file_path)
            {
                matched_index = i;
                break;
            }
        }
    }

    if (matched_index >= 0)
    {
        selected_scene_file_index = matched_index;
    }
    else
    {
        int32_t previous_index = selected_scene_file_index;
        selected_scene_file_index = std::clamp(previous_index, 0, static_cast<int32_t>(discovered_scenes.size()) - 1);
    }
    scene_io_status = "Found " + std::to_string(discovered_scenes.size()) + " scene files";
    scene_io_error = false;
}

void sample_2d_pathtracer::draw_export_scene_modal()
{
    if (open_export_scene_modal)
    {
        ImGui::OpenPopup("Export Scene");
        open_export_scene_modal = false;
    }

    bool open = true;
    if (!ImGui::BeginPopupModal("Export Scene", &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::Text("Directory:");
    ImGui::TextWrapped("%s", scenes_directory.empty() ? "<unresolved>" : scenes_directory.c_str());
    ImGui::InputText("Filename", export_scene_filename, IM_ARRAYSIZE(export_scene_filename));

    if (ImGui::Button("Save"))
    {
        std::string filename = export_scene_filename;
        if (filename.empty())
        {
            scene_io_status = "Export failed: filename is empty";
            scene_io_error = true;
        }
        else if (scenes_directory.empty())
        {
            scene_io_status = "Export failed: scenes directory unresolved";
            scene_io_error = true;
        }
        else
        {
            std::filesystem::path output = std::filesystem::path(scenes_directory) / filename;
            if (output.extension().string().empty()) output.replace_extension(".json");
            scene_file_path = output.string();
            save_scene_to_file(scene_file_path);
            load_scenes();
        }

        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void sample_2d_pathtracer::on_window_resize(int2 size)
{
    if (size.x == current_width && size.y == current_height) return;
    current_width = size.x;
    current_height = size.y;

    accumulation_texture = gl_texture_2d();
    setup_accumulation(size.x, size.y);
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
        float2 world = camera.cursor_to_world(camera.last_cursor, current_width, current_height);

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
        camera.panning = event.is_down();
    }

    if (event.type == app_input_event::CURSOR)
    {
        float2 cursor = {static_cast<float>(event.cursor.x), static_cast<float>(event.cursor.y)};

        if (dragging && left_mouse_down && selected_index >= 0)
        {
            float2 world = camera.cursor_to_world(cursor, current_width, current_height);
            scene[selected_index].position = world + drag_offset;
            scene_dirty = true;
        }
        else if (camera.panning)
        {
            camera.handle_pan(cursor, current_height);
            scene_dirty = true;
        }

        camera.update_cursor(cursor);
    }

    // Scroll to zoom
    if (event.type == app_input_event::SCROLL)
    {
        camera.handle_scroll(event.value.y);
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

void sample_2d_pathtracer::draw_environment_composer_modal()
{
    if (show_env_composer_modal)
    {
        ImGui::OpenPopup("Environment Composer");
        show_env_composer_modal = false;
    }

    bool open = true;
    if (!ImGui::BeginPopupModal("Environment Composer", &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    bool changed = false;

    changed |= ImGui::Checkbox("Enable Environment Map", &env.enabled);
    changed |= ImGui::DragFloat("Composer Gain", &env.gain, 0.01f, 0.0f, 50.0f, "%.3f");

    int32_t new_resolution = env.resolution;
    if (ImGui::SliderInt("Resolution", &new_resolution, 128, 4096))
    {
        if (new_resolution != env.resolution)
        {
            env.resolution = new_resolution;
            setup_environment_texture();
            changed = true;
        }
    }

    const char * interp_items[] = {"RGB linear", "HSV shortest", "HSV longest"};
    int interp = static_cast<int>(env.interpolation);
    if (ImGui::Combo("Interpolation", &interp, interp_items, IM_ARRAYSIZE(interp_items)))
    {
        env.interpolation = static_cast<env_interp_mode>(interp);
        changed = true;
    }

    ImGui::SeparatorText("Presets");
    if (ImGui::Button("Single Hard Sun")) { apply_environment_preset(0); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Dual Key")) { apply_environment_preset(1); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Gradient Sky")) { apply_environment_preset(2); changed = true; }
    if (ImGui::Button("Neon Arc")) { apply_environment_preset(3); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Striped Angular")) { apply_environment_preset(4); changed = true; }

    if (env_dirty) bake_environment_texture();

    ImGui::SeparatorText("Preview");
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();
    float preview_w = 700.0f;
    float preview_h = 36.0f;
    ImGui::InvisibleButton("##env_preview", ImVec2(preview_w, preview_h));
    ImDrawList * dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(preview_pos, ImVec2(preview_pos.x + preview_w, preview_pos.y + preview_h), IM_COL32(20, 20, 20, 255));
    if (!env_baked.empty())
    {
        for (int x = 0; x < static_cast<int>(preview_w); ++x)
        {
            float u = (x + 0.5f) / preview_w;
            size_t idx = static_cast<size_t>(std::clamp(static_cast<int>(u * env.resolution), 0, env.resolution - 1));
            const float3 c = env_baked[idx];
            ImU32 col = IM_COL32(
                static_cast<int>(std::clamp(static_cast<float>(c.x), 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(static_cast<float>(c.y), 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(static_cast<float>(c.z), 0.0f, 1.0f) * 255.0f),
                255);
            dl->AddLine(ImVec2(preview_pos.x + static_cast<float>(x), preview_pos.y),
                        ImVec2(preview_pos.x + static_cast<float>(x), preview_pos.y + preview_h), col);
        }
    }

    auto marker_to_u = [](float x, float x0, float w) { return std::clamp((x - x0) / w, 0.0f, 1.0f); };

    ImGui::SeparatorText("Gradient Stops");
    ImVec2 stop_pos = ImGui::GetCursorScreenPos();
    float stop_h = 40.0f;
    ImGui::InvisibleButton("##stop_lane", ImVec2(preview_w, stop_h));
    bool stop_hovered = ImGui::IsItemHovered();
    dl->AddRectFilled(stop_pos, ImVec2(stop_pos.x + preview_w, stop_pos.y + stop_h), IM_COL32(25, 25, 25, 255));
    dl->AddRect(stop_pos, ImVec2(stop_pos.x + preview_w, stop_pos.y + stop_h), IM_COL32(70, 70, 70, 255));

    for (int i = 0; i < static_cast<int>(env.stops.size()); ++i)
    {
        float x = stop_pos.x + env.stops[i].u * preview_w;
        ImU32 col = IM_COL32(
            static_cast<int>(std::clamp(static_cast<float>(env.stops[i].color.x), 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(static_cast<float>(env.stops[i].color.y), 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(static_cast<float>(env.stops[i].color.z), 0.0f, 1.0f) * 255.0f),
            255);
        dl->AddTriangleFilled(ImVec2(x, stop_pos.y + 3.0f), ImVec2(x - 6.0f, stop_pos.y + 14.0f), ImVec2(x + 6.0f, stop_pos.y + 14.0f), col);
        if (i == env_selected_stop) dl->AddCircle(ImVec2(x, stop_pos.y + 21.0f), 6.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
    }

    if (stop_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        float u = marker_to_u(ImGui::GetIO().MousePos.x, stop_pos.x, preview_w);
        env.stops.push_back({u, eval_environment(u)});
        env_selected_stop = static_cast<int>(env.stops.size()) - 1;
        changed = true;
    }
    if (stop_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        env_dragging_stop = false;
        float best_px = 10.0f;
        int best = -1;
        for (int i = 0; i < static_cast<int>(env.stops.size()); ++i)
        {
            float x = stop_pos.x + env.stops[i].u * preview_w;
            float d = std::abs(ImGui::GetIO().MousePos.x - x);
            if (d < best_px) { best_px = d; best = i; }
        }
        env_selected_stop = best;
        env_dragging_stop = (best >= 0);
    }
    if (env_dragging_stop)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && env_selected_stop >= 0)
        {
            env.stops[env_selected_stop].u = marker_to_u(ImGui::GetIO().MousePos.x, stop_pos.x, preview_w);
            changed = true;
        }
        else
        {
            env_dragging_stop = false;
        }
    }
    if (ImGui::Button("Add Stop"))
    {
        env.stops.push_back({0.5f, {1.0f, 1.0f, 1.0f}});
        env_selected_stop = static_cast<int>(env.stops.size()) - 1;
        changed = true;
    }

    ImGui::SeparatorText("Lobes");
    ImVec2 lobe_pos = ImGui::GetCursorScreenPos();
    float lobe_h = 46.0f;
    ImGui::InvisibleButton("##lobe_lane", ImVec2(preview_w, lobe_h));
    bool lobe_hovered = ImGui::IsItemHovered();
    dl->AddRectFilled(lobe_pos, ImVec2(lobe_pos.x + preview_w, lobe_pos.y + lobe_h), IM_COL32(25, 25, 25, 255));
    dl->AddRect(lobe_pos, ImVec2(lobe_pos.x + preview_w, lobe_pos.y + lobe_h), IM_COL32(70, 70, 70, 255));

    for (int i = 0; i < static_cast<int>(env.lobes.size()); ++i)
    {
        const env_lobe & l = env.lobes[i];
        float x = lobe_pos.x + l.u * preview_w;
        float hw = std::max(l.width * preview_w, 2.0f);
        ImU32 col = IM_COL32(
            static_cast<int>(std::clamp(static_cast<float>(l.color.x), 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(static_cast<float>(l.color.y), 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(static_cast<float>(l.color.z), 0.0f, 1.0f) * 255.0f),
            255);

        dl->AddLine(ImVec2(x - hw, lobe_pos.y + 22.0f), ImVec2(x + hw, lobe_pos.y + 22.0f), col, 2.0f);
        dl->AddCircleFilled(ImVec2(x, lobe_pos.y + 22.0f), 5.0f, col, 16);
        if (i == env_selected_lobe) dl->AddCircle(ImVec2(x, lobe_pos.y + 22.0f), 8.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
    }

    if (lobe_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        float u = marker_to_u(ImGui::GetIO().MousePos.x, lobe_pos.x, preview_w);
        env.lobes.push_back({u, 0.08f, 8.0f, 0.75f, {1.0f, 1.0f, 1.0f}});
        env_selected_lobe = static_cast<int>(env.lobes.size()) - 1;
        changed = true;
    }
    if (lobe_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        env_dragging_lobe = false;
        float best_px = 10.0f;
        int best = -1;
        for (int i = 0; i < static_cast<int>(env.lobes.size()); ++i)
        {
            float x = lobe_pos.x + env.lobes[i].u * preview_w;
            float d = std::abs(ImGui::GetIO().MousePos.x - x);
            if (d < best_px) { best_px = d; best = i; }
        }
        env_selected_lobe = best;
        env_dragging_lobe = (best >= 0);
    }
    if (env_dragging_lobe)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && env_selected_lobe >= 0)
        {
            env.lobes[env_selected_lobe].u = marker_to_u(ImGui::GetIO().MousePos.x, lobe_pos.x, preview_w);
            changed = true;
        }
        else
        {
            env_dragging_lobe = false;
        }
    }
    if (ImGui::Button("Add Lobe"))
    {
        env.lobes.push_back({0.5f, 0.08f, 6.0f, 0.75f, {1.0f, 1.0f, 1.0f}});
        env_selected_lobe = static_cast<int>(env.lobes.size()) - 1;
        changed = true;
    }

    ImGui::Separator();
    ImGuiColorEditFlags color_flags = ImGuiColorEditFlags_Float;
    if (env.interpolation != env_interp_mode::rgb_linear)
    {
        color_flags |= ImGuiColorEditFlags_InputHSV | ImGuiColorEditFlags_DisplayHSV;
    }

    if (env_selected_stop >= 0 && env_selected_stop < static_cast<int>(env.stops.size()))
    {
        env_gradient_stop & s = env.stops[env_selected_stop];
        changed |= ImGui::SliderFloat("Stop U", &s.u, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit3("Stop Color", &s.color.x, color_flags);
        if (ImGui::Button("Delete Stop"))
        {
            env.stops.erase(env.stops.begin() + env_selected_stop);
            env_selected_stop = std::min(env_selected_stop, static_cast<int>(env.stops.size()) - 1);
            changed = true;
        }
    }

    if (env_selected_lobe >= 0 && env_selected_lobe < static_cast<int>(env.lobes.size()))
    {
        env_lobe & l = env.lobes[env_selected_lobe];
        changed |= ImGui::SliderFloat("Lobe U", &l.u, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Width", &l.width, 0.001f, 0.5f, "%.4f");
        changed |= ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 80.0f, "%.2f");
        changed |= ImGui::SliderFloat("Falloff", &l.falloff, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::ColorEdit3("Lobe Color", &l.color.x, color_flags);
        if (ImGui::Button("Delete Lobe"))
        {
            env.lobes.erase(env.lobes.begin() + env_selected_lobe);
            env_selected_lobe = std::min(env_selected_lobe, static_cast<int>(env.lobes.size()) - 1);
            changed = true;
        }
    }

    if (changed)
    {
        env_dirty = true;
        clear_accumulation();
    }

    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void sample_2d_pathtracer::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);  // disable vsync

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

    if (env_dirty)
    {
        bake_environment_texture();
        clear_accumulation();
    }

    // path trace + accumulate
    {
        trace_compute.bind();
        trace_compute.bind_ssbo(0, primitives_ssbo);
        trace_compute.bind_image(1, accumulation_texture, GL_READ_WRITE, GL_RGBA32F);

        trace_compute.uniform("u_num_prims", static_cast<int>(scene.size()));
        trace_compute.uniform("u_frame_index", frame_index);
        trace_compute.uniform("u_max_bounces", config.max_bounces);
        trace_compute.uniform("u_samples_per_frame", config.samples_per_frame);
        trace_compute.uniform("u_environment_intensity", config.environment_intensity);
        trace_compute.uniform("u_use_environment_map", env.enabled ? 1 : 0);
        trace_compute.uniform("u_environment_map", 2);
        trace_compute.uniform("u_firefly_clamp", config.firefly_clamp);
        trace_compute.uniform("u_camera_zoom", camera.zoom);
        trace_compute.uniform("u_camera_center", camera.center);
        trace_compute.uniform("u_resolution", float2(static_cast<float>(width), static_cast<float>(height)));
        glBindTextureUnit(2, environment_texture_1d);

        uint32_t groups_x = (width + 15) / 16;
        uint32_t groups_y = (height + 15) / 16;
        trace_compute.dispatch_and_barrier(groups_x, groups_y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        trace_compute.unbind();

        frame_index++;
    }

    // output pass 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        display_shader.bind();
        display_shader.texture("u_accumulation_tex", 0, accumulation_texture, GL_TEXTURE_2D);
        display_shader.uniform("u_exposure", config.exposure);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, primitives_ssbo);
        display_shader.uniform("u_camera_zoom", camera.zoom);
        display_shader.uniform("u_camera_center", camera.center);
        display_shader.uniform("u_resolution", float2(static_cast<float>(width), static_cast<float>(height)));
        display_shader.uniform("u_num_prims", static_cast<int>(scene.size()));
        display_shader.uniform("u_selected_prim", selected_index);
        display_shader.uniform("u_debug_overlay", config.debug_overlay ? 1 : 0);

        glBindVertexArray(empty_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        display_shader.unbind();
    }


    imgui->begin_frame();
    gui::imgui_fixed_window_begin("PT Settings", ui_rect{{0, 0}, {320, height}});

    int32_t total_samples = frame_index * config.samples_per_frame;
    ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text("Samples: %d", total_samples);
    ImGui::Separator();

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
        ImGui::SameLine();
        if (ImGui::Button("Export EXR")) export_exr();
        ImGui::Checkbox("Debug Overlay", &config.debug_overlay);
    }

    if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Use 1D Environment Map", &env.enabled)) clear_accumulation();
        ImGui::Text("Current profile: %d stops, %d lobes", static_cast<int>(env.stops.size()), static_cast<int>(env.lobes.size()));
        if (ImGui::Button("Open Composer")) show_env_composer_modal = true;
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::SliderFloat("Zoom", &camera.zoom, 0.1f, 10.0f)) scene_dirty = true;
        if (ImGui::SliderFloat2("Center", &camera.center.x, -10.0f, 10.0f)) scene_dirty = true;
    }

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

    if (selected_index >= 0 && selected_index < static_cast<int32_t>(scene.size()))
    {
        if (ImGui::CollapsingHeader("Selected Primitive", ImGuiTreeNodeFlags_DefaultOpen))
        {
            scene_primitive & sp = scene[selected_index];
            const char * type_names[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon"};
            const char * mat_names[] = {"Diffuse", "Mirror", "Glass", "Water", "Diamond"};

            bool changed = false;
            changed |= ImGui::DragFloat2("Position", &sp.position.x, 0.05f);
            changed |= ImGui::SliderFloat("Rotation", &sp.rotation, -POLYMER_PI, POLYMER_PI);

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
                    changed |= ImGui::DragFloat("Radius 1", &sp.params.x, 0.01f, -5.0f, 5.0f);
                    changed |= ImGui::DragFloat("Radius 2", &sp.params.y, 0.01f, -5.0f, 5.0f);
                    changed |= ImGui::DragFloat("Distance", &sp.params.z, 0.01f, 0.0f, 5.0f);
                    changed |= ImGui::DragFloat("Aperture (0=auto)", &sp.params.w, 0.01f, 0.0f, 5.0f);
                    break;
                case prim_type::ngon:
                    changed |= ImGui::DragFloat("Radius##ngon", &sp.params.x, 0.01f, 0.01f, 5.0f);
                    changed |= ImGui::DragFloat("Sides", &sp.params.y, 0.1f, 3.0f, 12.0f);
                    break;
            }

            changed |= ImGui::ColorEdit3("Albedo", &sp.albedo.x);
            changed |= ImGui::DragFloat("Emission", &sp.emission, 0.1f, 0.0f, 100.0f);
            if (sp.emission > 0.0f) changed |= ImGui::SliderFloat("Emission Angle", &sp.emission_half_angle, 0.05f, POLYMER_PI);

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

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const std::vector<scene_preset> & presets = get_scene_presets();
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (ImGui::Button(presets[i].name))
            {
                scene = presets[i].build();
                selected_index = -1;
                scene_dirty = true;
            }
        }
    }

    if (ImGui::CollapsingHeader("Scene Export", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("Scene Directory: %s", scenes_directory.empty() ? "<unresolved>" : scenes_directory.c_str());
        if (ImGui::Button("Refresh Scene List"))
        {
            load_scenes();
        }

        const char * preview = (selected_scene_file_index >= 0 && selected_scene_file_index < static_cast<int32_t>(discovered_scenes.size()))
            ? discovered_scenes[selected_scene_file_index].name.c_str()
            : "<none>";

        if (ImGui::BeginCombo("Available Scenes", preview))
        {
            for (int32_t i = 0; i < static_cast<int32_t>(discovered_scenes.size()); ++i)
            {
                const bool is_selected = (i == selected_scene_file_index);
                if (ImGui::Selectable(discovered_scenes[i].name.c_str(), is_selected)) selected_scene_file_index = i;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Load Selected"))
        {
            if (selected_scene_file_index >= 0 && selected_scene_file_index < static_cast<int32_t>(discovered_scenes.size()))
            {
                load_scene_from_file(discovered_scenes[selected_scene_file_index].path);
            }
            else
            {
                scene_io_status = "Load failed: no scene selected";
                scene_io_error = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Export Scene"))
        {
            open_export_scene_modal = true;
        }

        if (!scene_io_status.empty())
        {
            const ImVec4 color = scene_io_error ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) : ImVec4(0.35f, 0.9f, 0.35f, 1.0f);
            ImGui::TextColored(color, "%s", scene_io_status.c_str());
        }
    }

    draw_export_scene_modal();
    gui::imgui_fixed_window_end();
    draw_environment_composer_modal();
    imgui->end_frame();

    glfwSwapBuffers(window);

    gl_check_error(__FILE__, __LINE__);
}

int main(int argc, char * argv[])
{
    try
    {
        sample_2d_pathtracer app; app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl; return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

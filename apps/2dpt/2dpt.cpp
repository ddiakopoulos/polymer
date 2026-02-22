#include "2dpt-utils.hpp"
#include "serialization.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"

#include <cctype>
#include <cfloat>
#include <cstring>

using namespace gui;

inline float sdf_circle(float2 p, float r) 
{ 
    return length(p) - r; 
}

inline float sdf_box(float2 p, float2 half_size, float radius)
{
    float dx = std::abs(p.x) - half_size.x + radius;
    float dy = std::abs(p.y) - half_size.y + radius;
    float2 clamped = {(dx > 0.0f ? dx : 0.0f), (dy > 0.0f ? dy : 0.0f)};
    float inner = (dx > dy) ? dx : dy;
    return length(clamped) + (inner < 0.0f ? inner : 0.0f) - radius;
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
        case prim_type::box:     return sdf_box(local_p, {sp.params.x, sp.params.y}, sp.params.z);
        case prim_type::capsule: return sdf_capsule(local_p, sp.params.x, sp.params.y);
        case prim_type::segment: return sdf_segment(local_p, sp.params.x, sp.params.y);
        case prim_type::lens:    return sdf_lens(local_p, sp.params.x, sp.params.y, sp.params.z, sp.params.w);
        case prim_type::ngon:    return sdf_ngon(local_p, sp.params.x, sp.params.y);
        case prim_type::image_sdf: return 1e10f;
        default:                 return 1e10f;
    }
}

inline void uniform3f(const gl_shader_compute & shader, const std::string & name, const float3 & v)
{
    glProgramUniform3fv(shader.handle(), shader.get_uniform_location(name), 1, &v.x);
}

struct pathtracer_2d final : public polymer_app
{
    struct discovered_scene
    {
        std::string name;
        std::string path;
    };

    struct discovered_sdf
    {
        std::string name;
        std::string path;
        int32_t width = 0;
        int32_t height = 0;
        int32_t channels = 0;
        std::vector<uint8_t> pixels;
    };

    std::unique_ptr<imgui_instance> imgui;

    path_tracer_config config;
    std::vector<scene_primitive> scene;

    gl_shader_compute trace_compute;
    gl_shader display_shader;
    gl_texture_2d accumulation_texture;
    gl_texture_3d sdf_texture_array;
    gl_buffer primitives_ssbo;
    gl_vertex_array_object empty_vao;
    GLuint environment_texture_1d = 0;

    int32_t current_width = 0;
    int32_t current_height = 0;
    int32_t frame_index = 0;
    bool scene_dirty = true;
    bool env_dirty = true;

    camera_controller_2d camera;
    env_composer env;
    env_composer_ui_state env_ui;
    std::vector<float3> env_baked;

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
    char export_scene_filename[128] = "new-scene.json";

    std::vector<discovered_sdf> discovered_sdfs;
    int32_t selected_sdf_file_index = -1;
    std::string sdfs_directory;
    std::string sdf_io_status;
    bool sdf_io_error = false;

    pathtracer_2d();
    ~pathtracer_2d();

    int32_t pick_primitive(float2 world_pos, int32_t current_selection) const;
    float eval_primitive_distance_cpu(float2 world_pos, const scene_primitive & sp) const;
    void fit_image_sdf_aspect(scene_primitive & sp, int32_t sdf_index) const;

    void build_default_scene();
    void upload_scene();
    void setup_accumulation(int32_t width, int32_t height);
    void clear_accumulation();
    void add_primitive(prim_type type, float2 world_pos);
    void export_exr();
    bool save_scene_to_file(const std::string & path);
    bool load_scene_from_file(const std::string & path);
    void load_scenes();
    void load_sdfs();
    void rebuild_sdf_texture_array();
    void draw_export_scene_modal();

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_window_resize(int2 size) override;
};

int32_t pathtracer_2d::pick_primitive(float2 world_pos, int32_t current_selection) const
{
    const float pick_threshold = 0.5f;

    std::vector<std::pair<float, int32_t>> candidates;
    for (int32_t i = 0; i < static_cast<int32_t>(scene.size()); ++i)
    {
        float d = eval_primitive_distance_cpu(world_pos, scene[i]);
        if (d < pick_threshold) candidates.push_back({d, i});
    }

    if (candidates.empty()) return -1;

    std::sort(candidates.begin(), candidates.end());

    if (current_selection < 0) return candidates[0].second;

    for (int32_t c = 0; c < static_cast<int32_t>(candidates.size()); ++c)
    {
        if (candidates[c].second == current_selection)
        {
            int32_t next = (c + 1) % static_cast<int32_t>(candidates.size());
            return candidates[next].second;
        }
    }

    return candidates[0].second;
}

float pathtracer_2d::eval_primitive_distance_cpu(float2 world_pos, const scene_primitive & sp) const
{
    if (sp.type != prim_type::image_sdf) return eval_primitive_cpu(world_pos, sp);

    if (discovered_sdfs.empty()) return 1e10f;

    const int32_t sdf_index = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
    const discovered_sdf & sdf = discovered_sdfs[sdf_index];
    if (sdf.width <= 0 || sdf.height <= 0 || sdf.channels <= 0 || sdf.pixels.empty()) return 1e10f;

    float2 local_p = rotate_2d(world_pos - sp.position, -sp.rotation);
    const float half_x = std::max(static_cast<float>(sp.params.x), 1e-4f);
    const float half_y = std::max(static_cast<float>(sp.params.y), 1e-4f);
    float2 half_extents = {half_x, half_y};
    float2 uv = local_p / (2.0f * half_extents) + float2{0.5f, 0.5f};
    float2 uv_clamped = {clamp(uv.x, 0.0f, 1.0f), clamp(uv.y, 0.0f, 1.0f)};

    const float x = uv_clamped.x * static_cast<float>(sdf.width - 1);
    const float y = uv_clamped.y * static_cast<float>(sdf.height - 1);
    const int32_t x0 = std::clamp(static_cast<int32_t>(std::floor(x)), 0, sdf.width - 1);
    const int32_t y0 = std::clamp(static_cast<int32_t>(std::floor(y)), 0, sdf.height - 1);
    const int32_t x1 = std::min(x0 + 1, sdf.width - 1);
    const int32_t y1 = std::min(y0 + 1, sdf.height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    auto sample_r = [&sdf](int32_t sx, int32_t sy)
    {
        const size_t idx = static_cast<size_t>(sy * sdf.width + sx) * static_cast<size_t>(sdf.channels);
        const float value = static_cast<float>(sdf.pixels[idx]);

        if (sdf.channels == 2)
        {
            const float alpha = static_cast<float>(sdf.pixels[idx + 1]) / 255.0f;
            return (value * alpha + 255.0f * (1.0f - alpha)) / 255.0f;
        }

        if (sdf.channels >= 4)
        {
            const float alpha = static_cast<float>(sdf.pixels[idx + 3]) / 255.0f;
            return (value * alpha + 255.0f * (1.0f - alpha)) / 255.0f;
        }

        return value / 255.0f;
    };

    const float s00 = sample_r(x0, y0);
    const float s10 = sample_r(x1, y0);
    const float s01 = sample_r(x0, y1);
    const float s11 = sample_r(x1, y1);
    const float sx0 = s00 + (s10 - s00) * tx;
    const float sx1 = s01 + (s11 - s01) * tx;
    float encoded = sx0 + (sx1 - sx0) * ty;
    if (sp.invert_image) encoded = 1.0f - encoded;

    const float range_scale = (std::abs(sp.params.w) > 1e-6f) ? sp.params.w : 1.0f;
    const float signed_dist = (encoded * 2.0f - 1.0f) * range_scale;

    float2 q = {std::abs(local_p.x) - half_extents.x, std::abs(local_p.y) - half_extents.y};
    const float qx = std::max(static_cast<float>(q.x), 0.0f);
    const float qy = std::max(static_cast<float>(q.y), 0.0f);
    float2 q_pos = {qx, qy};
    const float outside = length(q_pos);

    return signed_dist + outside;
}

void pathtracer_2d::fit_image_sdf_aspect(scene_primitive & sp, int32_t sdf_index) const
{
    if (sdf_index < 0 || sdf_index >= static_cast<int32_t>(discovered_sdfs.size())) return;

    const discovered_sdf & sdf = discovered_sdfs[sdf_index];
    if (sdf.width <= 0 || sdf.height <= 0) return;

    const float w = static_cast<float>(sdf.width);
    const float h = static_cast<float>(sdf.height);
    if (w >= h)
    {
        sp.params.x = 1.0f;
        sp.params.y = std::max(h / w, 1e-4f);
    }
    else
    {
        sp.params.x = std::max(w / h, 1e-4f);
        sp.params.y = 1.0f;
    }
}

void pathtracer_2d::add_primitive(prim_type type, float2 world_pos)
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
        case prim_type::image_sdf:
        {
            int32_t sdf_idx = 0;
            if (!discovered_sdfs.empty())
            {
                sdf_idx = std::clamp(selected_sdf_file_index, 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
            }
            sp.params = {1.0f, 1.0f, static_cast<float>(sdf_idx), 1.0f};
            fit_image_sdf_aspect(sp, sdf_idx);
            break;
        }
    }

    sp.albedo = {0.8f, 0.8f, 0.8f};
    scene.push_back(sp);
    selected_index = static_cast<int32_t>(scene.size()) - 1;
    scene_dirty = true;
}

pathtracer_2d::pathtracer_2d() : polymer_app(1920, 1080, "2dpt", 1)
{
    glfwMakeContextCurrent(window);

    imgui.reset(new imgui_instance(window, true));
    gui::make_light_theme();

    const std::string asset_base = global_asset_dir::get()->get_asset_dir();
    const std::string shader_base = asset_base + "/shaders/2d-pathtracer/";

    std::string common_src = read_file_text(shader_base + "pt_common.glsl");
    std::string trace_src = read_file_text(shader_base + "pt_trace_comp.glsl");
    trace_compute = gl_shader_compute(common_src + "\n" + trace_src);

    std::string fullscreen_vert = read_file_text(asset_base + "/shaders/fullscreen_vert.glsl");
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
    apply_environment_preset(env, env_ui, 0);
    setup_environment_texture(env, environment_texture_1d);
    bake_environment_texture(env, environment_texture_1d, env_baked, env_dirty);

    setup_accumulation(width, height);

    load_sdfs();
    build_default_scene();
    load_scenes();

    gl_check_error(__FILE__, __LINE__);
}

pathtracer_2d::~pathtracer_2d()
{
    if (environment_texture_1d != 0)
    {
        glDeleteTextures(1, &environment_texture_1d);
        environment_texture_1d = 0;
    }
}

void pathtracer_2d::build_default_scene()
{
    selected_index = -1;
    scene_dirty = true;
}

void pathtracer_2d::upload_scene()
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

void pathtracer_2d::setup_accumulation(int32_t width, int32_t height)
{
    accumulation_texture = gl_texture_2d();
    accumulation_texture.setup(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(accumulation_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void pathtracer_2d::clear_accumulation()
{
    float clear_val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(accumulation_texture, 0, GL_RGBA, GL_FLOAT, clear_val);
    frame_index = 0;
}


void pathtracer_2d::export_exr()
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

    std::string timestamp = make_timestamp();

    std::string filename = "pathtracer_" + timestamp + ".exr";
    export_exr_image(filename, current_width, current_height, 3, rgb);

    // Export object mask AOV: white where any SDF is present, black for background
    std::vector<float> mask(current_width * current_height * 3);
    float aspect = static_cast<float>(current_width) / static_cast<float>(current_height);
    for (int32_t y = 0; y < current_height; ++y)
    {
        for (int32_t x = 0; x < current_width; ++x)
        {
            float ndc_x = ((x + 0.5f) / static_cast<float>(current_width)) * 2.0f - 1.0f;
            float ndc_y = 1.0f - ((y + 0.5f) / static_cast<float>(current_height)) * 2.0f;
            float2 world_pos = float2{ndc_x * aspect, ndc_y} / camera.zoom + camera.center;

            float min_dist = std::numeric_limits<float>::max();
            for (const scene_primitive & sp : scene) min_dist = std::min(min_dist, eval_primitive_distance_cpu(world_pos, sp));

            int32_t dst = (y * current_width + x) * 3;
            float val = (min_dist <= 0.0f) ? 1.0f : 0.0f;
            mask[dst + 0] = val;
            mask[dst + 1] = val;
            mask[dst + 2] = val;
        }
    }

    std::string mask_filename = "pathtracer_mask_" + timestamp + ".exr";
    export_exr_image(mask_filename, current_width, current_height, 3, mask);
}

bool pathtracer_2d::save_scene_to_file(const std::string & path)
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

bool pathtracer_2d::load_scene_from_file(const std::string & path)
{
    try
    {
        const std::string content = read_file_text(path);
        const json j = json::parse(content);
        const pathtracer_scene_archive archive = j.get<pathtracer_scene_archive>();

        config = archive.config;
        camera = archive.camera;
        scene = archive.primitives;
        for (scene_primitive & sp : scene)
        {
            if (sp.type == prim_type::image_sdf)
            {
                sp.params.x = std::max(static_cast<float>(sp.params.x), 1e-4f);
                sp.params.y = std::max(static_cast<float>(sp.params.y), 1e-4f);
                sp.params.w = (std::abs(sp.params.w) > 1e-6f) ? sp.params.w : 1.0f;
                if (!discovered_sdfs.empty())
                {
                    const int32_t safe_idx = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
                    sp.params.z = static_cast<float>(safe_idx);
                }
                else
                {
                    sp.params.z = 0.0f;
                }
            }
        }
        env = archive.environment;
        env.resolution = std::max(env.resolution, 2048);
        setup_environment_texture(env, environment_texture_1d);
        env_ui.selected_stop = env.stops.empty() ? -1 : 0;
        env_ui.selected_lobe = env.lobes.empty() ? -1 : 0;
        env_ui.dragging_stop = false;
        env_ui.dragging_lobe = false;
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

void pathtracer_2d::load_scenes()
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

    const std::filesystem::path scene_dir = (std::filesystem::path(asset_dir) / ".." / "apps" / "2dpt" / "scenes").lexically_normal();
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

void pathtracer_2d::rebuild_sdf_texture_array()
{
    if (discovered_sdfs.empty())
    {
        sdf_texture_array = gl_texture_3d();
        return;
    }

    int32_t max_width = 0;
    int32_t max_height = 0;
    for (const discovered_sdf & sdf : discovered_sdfs)
    {
        max_width = std::max(max_width, sdf.width);
        max_height = std::max(max_height, sdf.height);
    }

    max_width = std::max(max_width, 1);
    max_height = std::max(max_height, 1);

    const size_t layer_count = discovered_sdfs.size();
    std::vector<uint8_t> atlas(static_cast<size_t>(max_width) * static_cast<size_t>(max_height) * layer_count, 0);

    for (size_t layer = 0; layer < discovered_sdfs.size(); ++layer)
    {
        const discovered_sdf & sdf = discovered_sdfs[layer];
        if (sdf.width <= 0 || sdf.height <= 0 || sdf.channels <= 0 || sdf.pixels.empty()) continue;

        for (int32_t y = 0; y < max_height; ++y)
        {
            for (int32_t x = 0; x < max_width; ++x)
            {
                const int32_t src_x = std::clamp(static_cast<int32_t>((static_cast<float>(x) + 0.5f) * static_cast<float>(sdf.width) / static_cast<float>(max_width)), 0, sdf.width - 1);
                const int32_t src_y = std::clamp(static_cast<int32_t>((static_cast<float>(y) + 0.5f) * static_cast<float>(sdf.height) / static_cast<float>(max_height)), 0, sdf.height - 1);

                const size_t src_idx = static_cast<size_t>(src_y * sdf.width + src_x) * static_cast<size_t>(sdf.channels);
                const size_t dst_idx =
                    (layer * static_cast<size_t>(max_width) * static_cast<size_t>(max_height)) +
                    static_cast<size_t>(y * max_width + x);
                const float value = static_cast<float>(sdf.pixels[src_idx]);
                float encoded = value;

                if (sdf.channels == 2)
                {
                    const float alpha = static_cast<float>(sdf.pixels[src_idx + 1]) / 255.0f;
                    encoded = value * alpha + 255.0f * (1.0f - alpha);
                }
                else if (sdf.channels >= 4)
                {
                    const float alpha = static_cast<float>(sdf.pixels[src_idx + 3]) / 255.0f;
                    encoded = value * alpha + 255.0f * (1.0f - alpha);
                }

                atlas[dst_idx] = static_cast<uint8_t>(std::clamp(encoded, 0.0f, 255.0f));
            }
        }
    }

    sdf_texture_array.setup(GL_TEXTURE_2D_ARRAY, max_width, max_height, static_cast<int32_t>(layer_count), GL_R8, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTextureParameteri(sdf_texture_array, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(sdf_texture_array, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(sdf_texture_array, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(sdf_texture_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(sdf_texture_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void pathtracer_2d::load_sdfs()
{
    discovered_sdfs.clear();

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
        sdf_io_status = "SDF discovery failed: assets directory not found";
        sdf_io_error = true;
        selected_sdf_file_index = -1;
        rebuild_sdf_texture_array();
        return;
    }

    const std::filesystem::path sdf_dir = (std::filesystem::path(asset_dir) / ".." / "apps" / "2dpt" / "sdfs").lexically_normal();
    sdfs_directory = sdf_dir.string();

    if (!std::filesystem::exists(sdf_dir))
    {
        selected_sdf_file_index = -1;
        sdf_io_status = "SDF directory not found: " + sdfs_directory;
        sdf_io_error = true;
        rebuild_sdf_texture_array();
        return;
    }

    for (const auto & entry : std::filesystem::directory_iterator(sdf_dir))
    {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".png") continue;

        discovered_sdf sdf_entry;
        sdf_entry.name = entry.path().stem().string();
        sdf_entry.path = entry.path().string();
        try
        {
            sdf_entry.pixels = load_image_data(sdf_entry.path, &sdf_entry.width, &sdf_entry.height, &sdf_entry.channels, true);
            if (sdf_entry.width > 0 && sdf_entry.height > 0 && sdf_entry.channels > 0 && !sdf_entry.pixels.empty())
            {
                discovered_sdfs.push_back(std::move(sdf_entry));
            }
        }
        catch (const std::exception &)
        {
            continue;
        }
    }

    std::sort(discovered_sdfs.begin(), discovered_sdfs.end(), [](const discovered_sdf & a, const discovered_sdf & b) { return a.name < b.name; });

    if (discovered_sdfs.empty())
    {
        selected_sdf_file_index = -1;
        sdf_io_status = "No PNG SDFs found in " + sdfs_directory;
        sdf_io_error = false;
        rebuild_sdf_texture_array();
        return;
    }

    selected_sdf_file_index = std::clamp(selected_sdf_file_index, 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
    rebuild_sdf_texture_array();

    for (scene_primitive & sp : scene)
    {
        if (sp.type == prim_type::image_sdf)
        {
            const int32_t safe_idx = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
            sp.params.z = static_cast<float>(safe_idx);
        }
    }

    sdf_io_status = "Found " + std::to_string(discovered_sdfs.size()) + " PNG SDF files";
    sdf_io_error = false;
    scene_dirty = true;
}

void pathtracer_2d::draw_export_scene_modal()
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

void pathtracer_2d::on_window_resize(int2 size)
{
    if (size.x == current_width && size.y == current_height) return;

    current_width = size.x;
    current_height = size.y;

    accumulation_texture = gl_texture_2d();
    setup_accumulation(size.x, size.y);
    clear_accumulation();
}

void pathtracer_2d::on_input(const app_input_event & event)
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
                int32_t picked = pick_primitive(world, selected_index);
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

void pathtracer_2d::on_update(const app_update_event & e) 
{
    // ... 
}

void pathtracer_2d::on_draw()
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
        bake_environment_texture(env, environment_texture_1d, env_baked, env_dirty);
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
        trace_compute.uniform("u_strict_layer_masking", config.strict_layer_masking ? 1 : 0);
        trace_compute.uniform("u_camera_zoom", camera.zoom);
        trace_compute.uniform("u_camera_center", camera.center);
        trace_compute.uniform("u_resolution", float2(static_cast<float>(width), static_cast<float>(height)));
        trace_compute.uniform("u_sdf_texture_array", 3);
        trace_compute.uniform("u_num_sdf_textures", static_cast<int>(discovered_sdfs.size()));
        glBindTextureUnit(2, environment_texture_1d);
        glBindTextureUnit(3, static_cast<GLuint>(sdf_texture_array));

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
        display_shader.uniform("u_sdf_texture_array", 3);
        display_shader.uniform("u_num_sdf_textures", static_cast<int>(discovered_sdfs.size()));
        display_shader.texture("u_sdf_texture_array", 3, static_cast<GLuint>(sdf_texture_array), GL_TEXTURE_2D_ARRAY);

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
        if (ImGui::Checkbox("Strict Layer Masking", &config.strict_layer_masking)) scene_dirty = true;

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
        if (ImGui::Button("Open Composer")) env_ui.show_modal = true;
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::SliderFloat("Zoom", &camera.zoom, 0.1f, 10.0f)) scene_dirty = true;
        if (ImGui::SliderFloat2("Center", &camera.center.x, -10.0f, 10.0f)) scene_dirty = true;
    }

    if (ImGui::CollapsingHeader("Add Primitive", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char * labels[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon", "Image SDF"};
        for (int32_t i = 0; i < IM_ARRAYSIZE(labels); ++i)
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
        const char * type_names[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon", "Image SDF"};
        const char * mat_names[] = {"Diffuse", "Mirror", "Glass", "Water", "Diamond"};

        for (int32_t i = 0; i < static_cast<int32_t>(scene.size()); ++i)
        {
            scene_primitive & sp = scene[i];
            ImGui::PushID(i);

            bool is_selected = (i == selected_index);

            char label[128];
            const char * vis_tag = "";
            if (sp.visibility == visibility_mode::primary_holdout) vis_tag = " [H]";
            else if (sp.visibility == visibility_mode::primary_no_direct) vis_tag = " [ND]";
            snprintf(label, sizeof(label), "%s %d (%s)%s",
                type_names[static_cast<int>(sp.type)], i,
                mat_names[static_cast<int>(sp.mat)],
                sp.emission > 0.0f ? " [E]" : "");
            if (vis_tag[0] != '\0') strncat(label, vis_tag, sizeof(label) - strlen(label) - 1);

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

        if (selected_index >= 0 && selected_index < static_cast<int32_t>(scene.size()))
        {
            ImGui::SameLine();
            if (ImGui::Button("Layer -") && selected_index > 0)
            {
                std::swap(scene[selected_index], scene[selected_index - 1]);
                selected_index -= 1;
                scene_dirty = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Layer +") && selected_index < static_cast<int32_t>(scene.size()) - 1)
            {
                std::swap(scene[selected_index], scene[selected_index + 1]);
                selected_index += 1;
                scene_dirty = true;
            }
        }
    }

    if (selected_index >= 0 && selected_index < static_cast<int32_t>(scene.size()))
    {
        if (ImGui::CollapsingHeader("Selected Primitive", ImGuiTreeNodeFlags_DefaultOpen))
        {
            scene_primitive & sp = scene[selected_index];
            const char * type_names[] = {"Circle", "Box", "Capsule", "Segment", "Lens", "N-gon", "Image SDF"};
            const char * mat_names[] = {"Diffuse", "Mirror", "Glass", "Water", "Diamond"};
            const char * vis_names[] = {"Normal", "Primary Holdout", "Primary No-Direct"};

            bool changed = false;
            changed |= ImGui::DragFloat2("Position", &sp.position.x, 0.05f);
            changed |= ImGui::SliderFloat("Rotation", &sp.rotation, -POLYMER_PI, POLYMER_PI);

            int type_idx = static_cast<int>(sp.type);
            if (ImGui::Combo("Shape", &type_idx, type_names, IM_ARRAYSIZE(type_names)))
            {
                sp.type = static_cast<prim_type>(type_idx);
                if (sp.type == prim_type::image_sdf)
                {
                    sp.params.w = (std::abs(sp.params.w) > 1e-6f) ? sp.params.w : 1.0f;
                    if (!discovered_sdfs.empty())
                    {
                        const int32_t safe_idx = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
                        sp.params.z = static_cast<float>(safe_idx);
                        fit_image_sdf_aspect(sp, safe_idx);
                    }
                    else
                    {
                        sp.params.z = 0.0f;
                        sp.params.x = 1.0f;
                        sp.params.y = 1.0f;
                    }
                }
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
                    changed |= ImGui::DragFloat("Corner Radius", &sp.params.z, 0.005f, 0.0f, min(sp.params.x, sp.params.y));
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
                case prim_type::image_sdf:
                {
                    float base_half_x = 1.0f;
                    float base_half_y = 1.0f;

                    if (!discovered_sdfs.empty())
                    {
                        const int32_t sdf_idx = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
                        const discovered_sdf & sdf = discovered_sdfs[sdf_idx];
                        if (sdf.width > 0 && sdf.height > 0)
                        {
                            const float w = static_cast<float>(sdf.width);
                            const float h = static_cast<float>(sdf.height);
                            if (w >= h)
                            {
                                base_half_x = 1.0f;
                                base_half_y = std::max(h / w, 1e-4f);
                            }
                            else
                            {
                                base_half_x = std::max(w / h, 1e-4f);
                                base_half_y = 1.0f;
                            }
                        }
                    }

                    float scale = std::max(static_cast<float>(sp.params.x) / base_half_x, static_cast<float>(sp.params.y) / base_half_y);
                    scale = std::max(scale, 0.01f);
                    if (ImGui::DragFloat("Scale##img", &scale, 0.01f, 0.01f, 100.0f))
                    {
                        sp.params.x = base_half_x * scale;
                        sp.params.y = base_half_y * scale;
                        changed = true;
                    }
                    changed |= ImGui::DragFloat("Distance Range##img", &sp.params.w, 0.005f, -0.1f, +0.1f);
                    changed |= ImGui::Checkbox("Invert Image##img", &sp.invert_image);

                    if (!discovered_sdfs.empty())
                    {
                        int32_t sdf_idx = std::clamp(static_cast<int32_t>(std::lround(sp.params.z)), 0, static_cast<int32_t>(discovered_sdfs.size()) - 1);
                        const char * preview = discovered_sdfs[sdf_idx].name.c_str();
                        if (ImGui::BeginCombo("SDF Image", preview))
                        {
                            for (int32_t i = 0; i < static_cast<int32_t>(discovered_sdfs.size()); ++i)
                            {
                                const bool is_selected = (i == sdf_idx);
                                if (ImGui::Selectable(discovered_sdfs[i].name.c_str(), is_selected))
                                {
                                    sp.params.z = static_cast<float>(i);
                                    fit_image_sdf_aspect(sp, i);
                                    changed = true;
                                }
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No SDF PNGs discovered");
                    }
                    break;
                }
            }

            int vis_idx = static_cast<int>(sp.visibility);
            if (ImGui::Combo("Visibility", &vis_idx, vis_names, IM_ARRAYSIZE(vis_names)))
            {
                sp.visibility = static_cast<visibility_mode>(vis_idx);
                changed = true;
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

    if (ImGui::CollapsingHeader("SDF Library", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("SDF Directory: %s", sdfs_directory.empty() ? "<unresolved>" : sdfs_directory.c_str());

        const char * preview = (selected_sdf_file_index >= 0 && selected_sdf_file_index < static_cast<int32_t>(discovered_sdfs.size()))
            ? discovered_sdfs[selected_sdf_file_index].name.c_str()
            : "<none>";

        if (ImGui::BeginCombo("Available SDF PNGs", preview))
        {
            for (int32_t i = 0; i < static_cast<int32_t>(discovered_sdfs.size()); ++i)
            {
                const bool is_selected = (i == selected_sdf_file_index);
                if (ImGui::Selectable(discovered_sdfs[i].name.c_str(), is_selected)) selected_sdf_file_index = i;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Refresh SDFs")) load_sdfs();

        if (!sdf_io_status.empty())
        {
            const ImVec4 color = sdf_io_error ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) : ImVec4(0.35f, 0.9f, 0.35f, 1.0f);
            ImGui::TextColored(color, "%s", sdf_io_status.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Scene Export", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("Scene Directory: %s", scenes_directory.empty() ? "<unresolved>" : scenes_directory.c_str());

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

    if (draw_environment_composer_modal(env, env_ui, env_baked, environment_texture_1d, env_dirty)) clear_accumulation();

    imgui->end_frame();

    glfwSwapBuffers(window);

    gl_check_error(__FILE__, __LINE__);
}

int main(int argc, char * argv[])
{
    try
    {
        pathtracer_2d app; app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl; return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#include "scenes.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/renderer/renderer-util.hpp"

#include <string>

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

struct sample_2d_pathtracer final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    path_tracer_config config;
    std::vector<scene_primitive> scene;

    gl_shader_compute trace_compute;
    gl_shader display_shader;
    gl_texture_2d accumulation_texture;
    gl_buffer primitives_ssbo;
    gl_vertex_array_object empty_vao;

    int32_t current_width = 0;
    int32_t current_height = 0;
    int32_t frame_index = 0;
    bool scene_dirty = true;

    camera_controller_2d camera;

    int32_t selected_index = -1;
    bool left_mouse_down = false;
    bool dragging = false;
    float2 drag_offset = {0.0f, 0.0f};

    int32_t pending_add_type = -1;

    sample_2d_pathtracer();
    ~sample_2d_pathtracer() = default;

    int32_t pick_primitive(float2 world_pos) const;

    void build_default_scene();
    void upload_scene();
    void setup_accumulation(int32_t width, int32_t height);
    void clear_accumulation();
    void add_primitive(prim_type type, float2 world_pos);
    void export_exr();

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

    setup_accumulation(width, height);

    build_default_scene();

    gl_check_error(__FILE__, __LINE__);
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
        trace_compute.uniform("u_firefly_clamp", config.firefly_clamp);
        trace_compute.uniform("u_camera_zoom", camera.zoom);
        trace_compute.uniform("u_camera_center", camera.center);
        trace_compute.uniform("u_resolution", float2(static_cast<float>(width), static_cast<float>(height)));

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

    gui::imgui_fixed_window_end();
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

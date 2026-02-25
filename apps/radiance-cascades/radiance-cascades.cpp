// Based on RadianceCascadesPaper by Alexander Sannikov
// https://github.com/Raikiri/RadianceCascadesPaper
// Licensed under the MIT License

#include "polymer-core/lib-polymer.hpp"
#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include <cmath>
#include <algorithm>

using namespace polymer;
using namespace gui;

struct rc_config
{
    int32_t base_ray_count = 4;
    int32_t base_pixels_between_probes_exp = 0;
    float ray_interval = 1.0f;
    float cascade_interval = 1.0f;
    float interval_overlap = 0.1f;
    float srgb_gamma = 2.2f;
    bool enable_sun = false;
    float sun_angle = 0.0f;
    bool add_noise = true;
    float brush_radius = 6.0f;
    float4 brush_color = {1.0f, 0.96f, 0.83f, 1.0f};
    bool force_full_pass = true;
    bool show_surface = true;
};

struct radiance_cascades_app final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    gl_shader draw_shader;
    gl_shader seed_shader;
    gl_shader jfa_shader;
    gl_shader distance_shader;
    gl_shader_compute cascades_compute;
    gl_shader overlay_shader;

    gl_texture_2d draw_tex[2];
    gl_texture_2d seed_tex;
    gl_texture_2d jfa_tex[2];
    gl_texture_2d distance_tex;
    gl_texture_2d cascade_tex[2];
    gl_texture_2d overlay_tex[2];

    gl_framebuffer fbo;
    gl_vertex_array_object empty_vao;

    rc_config config;

    bool drawing = false;
    float2 prev_mouse = {0.0f, 0.0f};
    float2 curr_mouse = {0.0f, 0.0f};
    float2 smooth_mouse = {0.0f, 0.0f};

    int32_t cascade_count = 0;
    int32_t radiance_width = 0;
    int32_t radiance_height = 0;
    int32_t jfa_passes = 0;

    int32_t frame = 0;
    int32_t canvas_width = 0;
    int32_t canvas_height = 0;

    int32_t draw_idx = 0;
    int32_t jfa_idx = 0;
    int32_t cascade_idx = 0;
    int32_t overlay_idx = 0;

    float friction = 0.2f;

    radiance_cascades_app();
    ~radiance_cascades_app() = default;

    void compute_cascade_params();
    void setup_textures(int32_t w, int32_t h);
    void clear_all();
    void draw_fullscreen_tri();

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_window_resize(int2 size) override;
};

inline void bind_fbo_to_texture(gl_framebuffer & fbo, gl_texture_2d & tex, int32_t w, int32_t h)
{
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

radiance_cascades_app::radiance_cascades_app() : polymer_app(1920, 1080, "Radiance Cascades", 1)
{
    glfwMakeContextCurrent(window);

    imgui.reset(new imgui_instance(window, true));
    gui::make_light_theme();

    const std::string asset_base = global_asset_dir::get()->get_asset_dir();
    const std::string shader_base = asset_base + "/shaders/radiance-cascades/";
    const std::string fullscreen_vert = read_file_text(asset_base + "/shaders/fullscreen_vert.glsl");

    draw_shader = gl_shader(fullscreen_vert, read_file_text(shader_base + "rc_draw_frag.glsl"));
    seed_shader = gl_shader(fullscreen_vert, read_file_text(shader_base + "rc_seed_frag.glsl"));
    jfa_shader = gl_shader(fullscreen_vert, read_file_text(shader_base + "rc_jfa_frag.glsl"));
    distance_shader = gl_shader(fullscreen_vert, read_file_text(shader_base + "rc_distance_frag.glsl"));
    cascades_compute = gl_shader_compute(read_file_text(shader_base + "rc_cascades_comp.glsl"));
    overlay_shader = gl_shader(fullscreen_vert, read_file_text(shader_base + "rc_overlay_frag.glsl"));

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    canvas_width = width;
    canvas_height = height;

    setup_textures(canvas_width, canvas_height);
    compute_cascade_params();
    clear_all();

    gl_check_error(__FILE__, __LINE__);
}

void radiance_cascades_app::compute_cascade_params()
{
    float base_pixels_between_probes = std::pow(2.0f, static_cast<float>(config.base_pixels_between_probes_exp));
    float diagonal = std::sqrt(static_cast<float>(canvas_width * canvas_width + canvas_height * canvas_height));
    cascade_count = static_cast<int32_t>(std::ceil(std::log(diagonal) / std::log(static_cast<float>(config.base_ray_count)))) + 1;
    radiance_width = static_cast<int32_t>(std::floor(static_cast<float>(canvas_width) / base_pixels_between_probes));
    radiance_height = static_cast<int32_t>(std::floor(static_cast<float>(canvas_height) / base_pixels_between_probes));
    jfa_passes = static_cast<int32_t>(std::ceil(std::log2(static_cast<float>(std::max(canvas_width, canvas_height))))) + 1;
}

void radiance_cascades_app::setup_textures(int32_t w, int32_t h)
{
    // Draw textures (RGBA8, NEAREST)
    for (int32_t i = 0; i < 2; ++i)
    {
        draw_tex[i] = gl_texture_2d();
        draw_tex[i].setup(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteri(draw_tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(draw_tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(draw_tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(draw_tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // Seed texture (RG32F, NEAREST)
    seed_tex = gl_texture_2d();
    seed_tex.setup(w, h, GL_RG32F, GL_RG, GL_FLOAT, nullptr);
    glTextureParameteri(seed_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(seed_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(seed_tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(seed_tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // JFA textures (RG32F, NEAREST)
    for (int32_t i = 0; i < 2; ++i)
    {
        jfa_tex[i] = gl_texture_2d();
        jfa_tex[i].setup(w, h, GL_RG32F, GL_RG, GL_FLOAT, nullptr);
        glTextureParameteri(jfa_tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(jfa_tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(jfa_tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(jfa_tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // Distance texture (R16F, NEAREST)
    distance_tex = gl_texture_2d();
    distance_tex.setup(w, h, GL_R16F, GL_RED, GL_HALF_FLOAT, nullptr);
    glTextureParameteri(distance_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(distance_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(distance_tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(distance_tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Cascade textures (R11F_G11F_B10F, LINEAR_MIPMAP_LINEAR, with mipmaps)
    for (int32_t i = 0; i < 2; ++i)
    {
        cascade_tex[i] = gl_texture_2d();
        cascade_tex[i].setup(w, h, GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT, nullptr, true);
        glTextureParameteri(cascade_tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cascade_tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cascade_tex[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(cascade_tex[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Overlay textures (RGBA16F, NEAREST)
    for (int32_t i = 0; i < 2; ++i)
    {
        overlay_tex[i] = gl_texture_2d();
        overlay_tex[i].setup(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTextureParameteri(overlay_tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(overlay_tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(overlay_tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(overlay_tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // Single reusable FBO
    fbo = gl_framebuffer();
}

void radiance_cascades_app::clear_all()
{
    float clear_black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_rg[2] = {0.0f, 0.0f};
    float clear_r[1] = {0.0f};
    float clear_rgb[3] = {0.0f, 0.0f, 0.0f};

    for (int32_t i = 0; i < 2; ++i)
    {
        glClearTexImage(draw_tex[i], 0, GL_RGBA, GL_FLOAT, clear_black);
        glClearTexImage(jfa_tex[i], 0, GL_RG, GL_FLOAT, clear_rg);
        glClearTexImage(cascade_tex[i], 0, GL_RGB, GL_FLOAT, clear_rgb);
        glClearTexImage(overlay_tex[i], 0, GL_RGBA, GL_FLOAT, clear_black);
    }
    glClearTexImage(seed_tex, 0, GL_RG, GL_FLOAT, clear_rg);
    glClearTexImage(distance_tex, 0, GL_RED, GL_FLOAT, clear_r);

    draw_idx = 0;
    jfa_idx = 0;
    cascade_idx = 0;
    overlay_idx = 0;
}

void radiance_cascades_app::draw_fullscreen_tri()
{
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void radiance_cascades_app::on_input(const app_input_event & event)
{
    imgui->update_input(event);

    if (ImGui::GetIO().WantCaptureMouse) return;

    if (event.type == app_input_event::MOUSE && event.value.x == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (event.is_down())
        {
            drawing = true;
            prev_mouse = curr_mouse;
            smooth_mouse = curr_mouse;
        }
        else
        {
            drawing = false;
        }
    }

    if (event.type == app_input_event::CURSOR)
    {
        prev_mouse = curr_mouse;
        curr_mouse = {static_cast<float>(event.cursor.x), static_cast<float>(event.cursor.y)};
        if (!drawing) smooth_mouse = curr_mouse;
    }

    if (event.type == app_input_event::KEY && event.value.x == GLFW_KEY_C && event.is_down())
    {
        clear_all();
    }
}

void radiance_cascades_app::on_update(const app_update_event & e)
{

}

void radiance_cascades_app::on_window_resize(int2 size)
{
    canvas_width = size.x;
    canvas_height = size.y;
    setup_textures(canvas_width, canvas_height);
    compute_cascade_params();
    clear_all();
}

void radiance_cascades_app::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (width != canvas_width || height != canvas_height)
    {
        on_window_resize({width, height});
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    float base_pixels_between_probes = std::pow(2.0f, static_cast<float>(config.base_pixels_between_probes_exp));
    float2 resolution = {static_cast<float>(canvas_width), static_cast<float>(canvas_height)};
    float2 one_over_size = {1.0f / resolution.x, 1.0f / resolution.y};
    bool is_frame_0 = config.force_full_pass || (frame == 0);

    {
        int32_t read = draw_idx;
        int32_t write = 1 - draw_idx;

        bind_fbo_to_texture(fbo, draw_tex[write], canvas_width, canvas_height);

        draw_shader.bind();
        draw_shader.texture("u_input_texture", 0, draw_tex[read], GL_TEXTURE_2D);
        draw_shader.uniform("u_resolution", resolution);
        draw_shader.uniform("u_scale", 1.0f);
        draw_shader.uniform("u_dpr", 1.0f);
        draw_shader.uniform("u_radius_squared", config.brush_radius * config.brush_radius);
        draw_shader.uniform("u_color", config.brush_color);
        draw_shader.uniform("u_drawing", drawing ? 1 : 0);

        if (drawing)
        {
            // Friction-based smoothing
            float2 dir = curr_mouse - smooth_mouse;
            float dist = length(dir);
            if (dist > 0.0001f) dir = dir / dist;
            float len = std::max(dist - std::sqrt(config.brush_radius), 0.0f);
            float ease = 1.0f - std::pow(friction, (1.0f / 60.0f) * 10.0f);
            float2 new_smooth = smooth_mouse + dir * len * ease;

            float2 from_flipped = {smooth_mouse.x, static_cast<float>(canvas_height) - smooth_mouse.y};
            float2 to_flipped = {new_smooth.x, static_cast<float>(canvas_height) - new_smooth.y};

            draw_shader.uniform("u_from", from_flipped);
            draw_shader.uniform("u_to", to_flipped);

            smooth_mouse = new_smooth;
        }
        else
        {
            draw_shader.uniform("u_from", float2{0.0f, 0.0f});
            draw_shader.uniform("u_to", float2{0.0f, 0.0f});
        }

        draw_fullscreen_tri();
        draw_idx = write;
    }

    // Seed pass
    if (is_frame_0)
    {
        bind_fbo_to_texture(fbo, seed_tex, canvas_width, canvas_height);
        seed_shader.bind();
        seed_shader.texture("u_surface_texture", 0, draw_tex[draw_idx], GL_TEXTURE_2D);
        draw_fullscreen_tri();
    }

    // Jump Flooding Algorithm
    if (is_frame_0)
    {
        jfa_idx = 0;
        for (int32_t i = 0; i < jfa_passes; ++i)
        {
            int32_t read = jfa_idx;
            int32_t write = 1 - jfa_idx;

            bind_fbo_to_texture(fbo, jfa_tex[write], canvas_width, canvas_height);
            jfa_shader.bind();

            gl_texture_2d & input_tex = (i == 0) ? seed_tex : jfa_tex[read];
            jfa_shader.texture("u_input_texture", 0, input_tex, GL_TEXTURE_2D);
            jfa_shader.uniform("u_resolution", resolution);
            jfa_shader.uniform("u_one_over_size", one_over_size);
            jfa_shader.uniform("u_offset", std::pow(2.0f, static_cast<float>(jfa_passes - i - 1)));
            jfa_shader.uniform("u_skip", 0);
            draw_fullscreen_tri();

            jfa_idx = write;
        }
    }

    // Distance field
    if (is_frame_0)
    {
        bind_fbo_to_texture(fbo, distance_tex, canvas_width, canvas_height);
        distance_shader.bind();
        distance_shader.texture("u_jfa_texture", 0, jfa_tex[jfa_idx], GL_TEXTURE_2D);
        distance_shader.uniform("u_resolution", resolution);
        draw_fullscreen_tri();
    }

    // Radiance cascades
    {
        compute_cascade_params();

        int32_t first_layer = cascade_count - 1;
        int32_t last_layer = 0;

        float2 cascade_extent = {static_cast<float>(radiance_width), static_cast<float>(radiance_height)};
        GLuint gx = (radiance_width + 15) / 16;
        GLuint gy = (radiance_height + 15) / 16;

        if (config.force_full_pass)
        {
            cascade_idx = 0;
            for (int32_t i = first_layer; i >= last_layer; --i)
            {
                int32_t read = cascade_idx;
                int32_t write = 1 - cascade_idx;

                cascades_compute.bind();
                cascades_compute.bind_image(0, cascade_tex[write], GL_WRITE_ONLY, GL_R11F_G11F_B10F);
                cascades_compute.texture("u_scene_texture", 0, draw_tex[draw_idx], GL_TEXTURE_2D);
                cascades_compute.texture("u_distance_texture", 1, distance_tex, GL_TEXTURE_2D);
                cascades_compute.texture("u_last_texture", 2, cascade_tex[read], GL_TEXTURE_2D);

                cascades_compute.uniform("u_resolution", resolution);
                cascades_compute.uniform("u_cascade_extent", cascade_extent);
                cascades_compute.uniform("u_cascade_count", static_cast<float>(cascade_count));
                cascades_compute.uniform("u_cascade_index", static_cast<float>(i));
                cascades_compute.uniform("u_base_pixels_between_probes", base_pixels_between_probes);
                cascades_compute.uniform("u_cascade_interval", config.cascade_interval);
                cascades_compute.uniform("u_ray_interval", config.ray_interval);
                cascades_compute.uniform("u_interval_overlap", config.interval_overlap);
                cascades_compute.uniform("u_base_ray_count", static_cast<float>(config.base_ray_count));
                cascades_compute.uniform("u_srgb", config.srgb_gamma);
                cascades_compute.uniform("u_enable_sun", config.enable_sun ? 1 : 0);
                cascades_compute.uniform("u_sun_angle", config.sun_angle);
                cascades_compute.uniform("u_add_noise", config.add_noise ? 1 : 0);
                cascades_compute.uniform("u_first_cascade_index", 0.0f);
                cascades_compute.uniform("u_bilinear_fix_enabled", 0);

                cascades_compute.dispatch_and_barrier(gx, gy, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
                glGenerateTextureMipmap(cascade_tex[write]);
                cascade_idx = write;
            }
        }
        else
        {
            // Progressive two-frame rendering
            int32_t halfway = static_cast<int32_t>(std::floor(static_cast<float>(first_layer - last_layer) / 2.0f));

            int32_t start_cascade, end_cascade;
            if (frame == 0)
            {
                start_cascade = first_layer;
                end_cascade = last_layer + halfway + 1;
                cascade_idx = 0;
            }
            else
            {
                start_cascade = last_layer + halfway;
                end_cascade = last_layer;
            }

            for (int32_t i = start_cascade; i >= end_cascade; --i)
            {
                int32_t read = cascade_idx;
                int32_t write = 1 - cascade_idx;

                cascades_compute.bind();
                cascades_compute.bind_image(0, cascade_tex[write], GL_WRITE_ONLY, GL_R11F_G11F_B10F);
                cascades_compute.texture("u_scene_texture", 0, draw_tex[draw_idx], GL_TEXTURE_2D);
                cascades_compute.texture("u_distance_texture", 1, distance_tex, GL_TEXTURE_2D);
                cascades_compute.texture("u_last_texture", 2, cascade_tex[read], GL_TEXTURE_2D);

                cascades_compute.uniform("u_resolution", resolution);
                cascades_compute.uniform("u_cascade_extent", cascade_extent);
                cascades_compute.uniform("u_cascade_count", static_cast<float>(cascade_count));
                cascades_compute.uniform("u_cascade_index", static_cast<float>(i));
                cascades_compute.uniform("u_base_pixels_between_probes", base_pixels_between_probes);
                cascades_compute.uniform("u_cascade_interval", config.cascade_interval);
                cascades_compute.uniform("u_ray_interval", config.ray_interval);
                cascades_compute.uniform("u_interval_overlap", config.interval_overlap);
                cascades_compute.uniform("u_base_ray_count", static_cast<float>(config.base_ray_count));
                cascades_compute.uniform("u_srgb", config.srgb_gamma);
                cascades_compute.uniform("u_enable_sun", config.enable_sun ? 1 : 0);
                cascades_compute.uniform("u_sun_angle", config.sun_angle);
                cascades_compute.uniform("u_add_noise", config.add_noise ? 1 : 0);
                cascades_compute.uniform("u_first_cascade_index", 0.0f);
                cascades_compute.uniform("u_bilinear_fix_enabled", 0);

                cascades_compute.dispatch_and_barrier(gx, gy, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
                glGenerateTextureMipmap(cascade_tex[write]);
                cascade_idx = write;
            }
        }
    }

    // Overlay
    if (config.force_full_pass)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        overlay_shader.bind();
        overlay_shader.texture("u_input_texture", 0, cascade_tex[cascade_idx], GL_TEXTURE_2D);
        overlay_shader.texture("u_draw_texture", 1, draw_tex[draw_idx], GL_TEXTURE_2D);
        overlay_shader.uniform("u_resolution", resolution);
        overlay_shader.uniform("u_show_surface", config.show_surface ? 1 : 0);
        draw_fullscreen_tri();
    }
    else
    {
        int32_t overlay_write = overlay_idx;
        int32_t overlay_read = 1 - overlay_idx;

        bind_fbo_to_texture(fbo, overlay_tex[overlay_write], canvas_width, canvas_height);
        overlay_shader.bind();
        overlay_shader.texture("u_input_texture", 0, cascade_tex[cascade_idx], GL_TEXTURE_2D);
        overlay_shader.texture("u_draw_texture", 1, draw_tex[draw_idx], GL_TEXTURE_2D);
        overlay_shader.uniform("u_resolution", resolution);
        overlay_shader.uniform("u_show_surface", config.show_surface ? 1 : 0);
        draw_fullscreen_tri();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        overlay_shader.bind();
        overlay_shader.texture("u_input_texture", 0, overlay_tex[overlay_read], GL_TEXTURE_2D);
        overlay_shader.texture("u_draw_texture", 1, draw_tex[draw_idx], GL_TEXTURE_2D);
        overlay_shader.uniform("u_resolution", resolution);
        overlay_shader.uniform("u_show_surface", config.show_surface ? 1 : 0);
        draw_fullscreen_tri();

        overlay_idx = 1 - overlay_idx;
    }

    if (!config.force_full_pass) frame = 1 - frame;

    imgui->begin_frame();
    gui::imgui_fixed_window_begin("Radiance Cascades", ui_rect{{0, 0}, {340, height}});

    ImGui::Text("%.1f fps (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char * ray_count_items[] = {"4", "16", "64"};
        int32_t ray_count_idx = 0;
        if (config.base_ray_count == 16) ray_count_idx = 1;
        else if (config.base_ray_count == 64) ray_count_idx = 2;
        if (ImGui::Combo("Base Ray Count", &ray_count_idx, ray_count_items, 3))
        {
            const int32_t values[] = {4, 16, 64};
            config.base_ray_count = values[ray_count_idx];
            compute_cascade_params();
        }

        if (ImGui::SliderInt("Probe Spacing (2^n)", &config.base_pixels_between_probes_exp, 0, 4))
        {
            compute_cascade_params();
        }
        int32_t actual_spacing = static_cast<int32_t>(std::pow(2.0f, static_cast<float>(config.base_pixels_between_probes_exp)));
        ImGui::SameLine();
        ImGui::Text("= %d", actual_spacing);

        ImGui::SliderFloat("Ray Interval", &config.ray_interval, 1.0f, 512.0f);
        ImGui::SliderFloat("Interval Overlap", &config.interval_overlap, -1.0f, 2.0f);
        ImGui::Text("Cascade Count: %d", cascade_count);
        ImGui::Text("Radiance Dims: %d x %d", radiance_width, radiance_height);
        ImGui::Checkbox("Add Noise", &config.add_noise);

        bool srgb_enabled = config.srgb_gamma > 1.5f;
        if (ImGui::Checkbox("sRGB Gamma", &srgb_enabled))
        {
            config.srgb_gamma = srgb_enabled ? 2.2f : 1.0f;
        }
    }

    if (ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable Sun", &config.enable_sun);
        if (!config.enable_sun) ImGui::BeginDisabled();
        ImGui::SliderFloat("Sun Angle", &config.sun_angle, 0.0f, 6.28318f);
        if (!config.enable_sun) ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Brush", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Radius", &config.brush_radius, 1.0f, 100.0f);
        ImGui::ColorEdit3("Color", &config.brush_color.x);
        if (ImGui::Button("Clear Canvas (C)"))
        {
            clear_all();
        }
        ImGui::Checkbox("Show Surface", &config.show_surface);
    }

    if (ImGui::CollapsingHeader("Performance"))
    {
        ImGui::Checkbox("Force Full Pass", &config.force_full_pass);
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
        radiance_cascades_app app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

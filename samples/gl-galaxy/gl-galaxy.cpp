#include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-gfx-gl/gl-post-processing.hpp"
#include "polymer-gfx-gl/post/gl-unreal-bloom.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include <cmath>

using namespace polymer;
using namespace gui;

// gl_shader_compute doesn't have a float3 uniform overload
inline void uniform3f(const gl_shader_compute & shader, const std::string & name, const float3 & v)
{
    glProgramUniform3fv(shader.handle(), shader.get_uniform_location(name), 1, &v.x);
}

inline float3 hex_to_float3(uint32_t hex)
{
    return float3(((hex >> 16) & 0xFF) / 255.0f,((hex >> 8) & 0xFF) / 255.0f,  (hex & 0xFF) / 255.0f);
}

struct galaxy_config
{
    int32_t star_count = 1000000;
    float rotation_speed = 0.1f;
    float spiral_tightness = 1.75f;
    float mouse_force = 7.0f;
    float mouse_radius = 10.0f;
    float galaxy_radius = 13.0f;
    float galaxy_thickness = 3.0f;
    int32_t arm_count = 2;
    float arm_width = 2.25f;
    float randomness = 1.8f;
    float particle_size = 0.06f;
    float star_brightness = 0.3f;
    int32_t cloud_count = 5000;
    float cloud_size = 3.0f;
    float cloud_opacity = 0.02f;
    float3 dense_color = hex_to_float3(0x1885FF);  // blue
    float3 sparse_color = hex_to_float3(0xFFB28A);  // orange
    float3 cloud_tint = hex_to_float3(0xFFDACE);    // light pink
};

struct sample_gl_galaxy final : public polymer_app
{
    camera_controller_orbit cam;
    std::unique_ptr<imgui_instance> imgui;

    galaxy_config config;

    // Post-processing
    gl_effect_composer composer;
    std::shared_ptr<gl_unreal_bloom> bloom_pass;

    // Compute shaders
    gl_shader_compute star_init_compute;
    gl_shader_compute star_update_compute;
    gl_shader_compute cloud_init_compute;
    gl_shader_compute cloud_update_compute;

    // Render shaders
    gl_shader star_shader;
    gl_shader cloud_shader;
    gl_shader starfield_shader;

    // Star SSBOs (bindings 0-3)
    gl_buffer star_positions_buf;
    gl_buffer star_originals_buf;
    gl_buffer star_velocities_buf;
    gl_buffer star_density_buf;

    // Cloud SSBOs (bindings 4-8)
    gl_buffer cloud_positions_buf;
    gl_buffer cloud_originals_buf;
    gl_buffer cloud_colors_buf;
    gl_buffer cloud_sizes_buf;
    gl_buffer cloud_rotations_buf;

    // Cloud texture
    gl_texture_2d cloud_texture;

    // Background starfield mesh (CPU-generated GL_POINTS)
    gl_mesh starfield_mesh;
    int32_t starfield_count = 5000;

    // Empty VAO for SSBO-based particle draws
    gl_vertex_array_object particle_vao;

    // HDR FBO
    gl_framebuffer hdr_framebuffer;
    gl_texture_2d hdr_color_texture;
    gl_texture_2d hdr_depth_texture;

    // State
    bool stars_initialized = false;
    bool clouds_initialized = false;
    bool needs_star_regen = false;
    bool needs_cloud_regen = false;

    // Mouse interaction
    bool mouse_pressed = false;
    float3 mouse_world_pos = {0, 0, 0};

    // Window dimensions
    int32_t current_width = 0;
    int32_t current_height = 0;

    sample_gl_galaxy();
    ~sample_gl_galaxy() = default;

    void allocate_star_ssbos();
    void allocate_cloud_ssbos();
    void setup_hdr_framebuffer(int32_t width, int32_t height);
    void generate_starfield();

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
    void on_window_resize(int2 size) override;
};

sample_gl_galaxy::sample_gl_galaxy() : polymer_app(1920, 1080, "galaxy-sim", 4)
{
    glfwMakeContextCurrent(window);

    // Enable point sprite features
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);

    cam.set_eye_position({0, 12, 17});
    cam.set_target({0, -2, 0});
    cam.yfov = to_radians(50.0f);

    // ImGui
    imgui.reset(new imgui_instance(window, true));
    gui::make_light_theme();

    // Resolve asset path
    const std::string asset_base = global_asset_dir::get()->get_asset_dir();
    const std::string shader_base = asset_base + "/shaders/galaxy/";

    // Load compute shaders
    star_init_compute = gl_shader_compute(read_file_text(shader_base + "galaxy_star_init_comp.glsl"));
    star_update_compute = gl_shader_compute(read_file_text(shader_base + "galaxy_star_update_comp.glsl"));
    cloud_init_compute = gl_shader_compute(read_file_text(shader_base + "galaxy_cloud_init_comp.glsl"));
    cloud_update_compute = gl_shader_compute(read_file_text(shader_base + "galaxy_cloud_update_comp.glsl"));

    // Load render shaders
    star_shader = gl_shader( read_file_text(shader_base + "galaxy_star_vert.glsl"), read_file_text(shader_base + "galaxy_star_frag.glsl"));
    cloud_shader = gl_shader(read_file_text(shader_base + "galaxy_cloud_vert.glsl"), read_file_text(shader_base + "galaxy_cloud_frag.glsl"));
    starfield_shader = gl_shader(read_file_text(shader_base + "galaxy_starfield_vert.glsl"),read_file_text(shader_base + "galaxy_starfield_frag.glsl"));

    // Post-processing (bloom + tonemapping)
    bloom_pass = std::make_shared<gl_unreal_bloom>(asset_base);
    bloom_pass->config.threshold = 0.1f;
    bloom_pass->config.strength = 1.5f;
    bloom_pass->config.tonemap_mode = 3; // ACES 2.0
    composer.add_pass(bloom_pass);

    cloud_texture = load_image(asset_base + "/textures/cloud.png", true);

    // Allocate SSBOs
    allocate_star_ssbos();
    allocate_cloud_ssbos();

    // Setup HDR framebuffer
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    current_width = width;
    current_height = height;
    setup_hdr_framebuffer(width, height);
    composer.resize(width, height);

    // Generate background starfield
    generate_starfield();

    gl_check_error(__FILE__, __LINE__);
}

void sample_gl_galaxy::allocate_star_ssbos()
{
    GLsizeiptr vec4_size = static_cast<GLsizeiptr>(config.star_count) * sizeof(float4);
    GLsizeiptr float_size = static_cast<GLsizeiptr>(config.star_count) * sizeof(float);

    star_positions_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    star_originals_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    star_velocities_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    star_density_buf.set_buffer_data(float_size, nullptr, GL_DYNAMIC_DRAW);

    stars_initialized = false;
}

void sample_gl_galaxy::allocate_cloud_ssbos()
{
    GLsizeiptr vec4_size = static_cast<GLsizeiptr>(config.cloud_count) * sizeof(float4);
    GLsizeiptr float_size = static_cast<GLsizeiptr>(config.cloud_count) * sizeof(float);

    cloud_positions_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    cloud_originals_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    cloud_colors_buf.set_buffer_data(vec4_size, nullptr, GL_DYNAMIC_DRAW);
    cloud_sizes_buf.set_buffer_data(float_size, nullptr, GL_DYNAMIC_DRAW);
    cloud_rotations_buf.set_buffer_data(float_size, nullptr, GL_DYNAMIC_DRAW);

    clouds_initialized = false;
}

void sample_gl_galaxy::setup_hdr_framebuffer(int32_t width, int32_t height)
{
    hdr_color_texture.setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdr_color_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    hdr_depth_texture.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glNamedFramebufferTexture(hdr_framebuffer, GL_COLOR_ATTACHMENT0, hdr_color_texture, 0);
    glNamedFramebufferTexture(hdr_framebuffer, GL_DEPTH_ATTACHMENT, hdr_depth_texture, 0);
    hdr_framebuffer.check_complete();

    gl_check_error(__FILE__, __LINE__);
}

void sample_gl_galaxy::generate_starfield()
{
    uniform_random_gen rng;

    struct starfield_vertex
    {
        float3 position;
        float3 color;
    };

    std::vector<starfield_vertex> vertices(starfield_count);

    for (int32_t i = 0; i < starfield_count; ++i)
    {
        // Uniform spherical distribution
        float theta = rng.random_float() * 6.28318f;
        float phi = std::acos(2.0f * rng.random_float() - 1.0f);
        float radius = 100.0f + rng.random_float() * 100.0f;

        vertices[i].position = float3(
            radius * std::sin(phi) * std::cos(theta),
            radius * std::sin(phi) * std::sin(theta),
            radius * std::cos(phi)
        );

        // Color variation: mostly white, some blue/orange tinted
        float brightness = 0.8f + rng.random_float() * 0.2f;
        float tint = rng.random_float();
        if (tint < 0.1f)
        {
            vertices[i].color = float3(brightness * 0.8f, brightness * 0.9f, brightness);
        }
        else if (tint < 0.2f)
        {
            vertices[i].color = float3(brightness, brightness * 0.8f, brightness * 0.6f);
        }
        else
        {
            vertices[i].color = float3(brightness, brightness, brightness);
        }
    }

    starfield_mesh.set_vertices(vertices, GL_STATIC_DRAW);
    starfield_mesh.set_attribute(0, &starfield_vertex::position);
    starfield_mesh.set_attribute(1, &starfield_vertex::color);
    starfield_mesh.set_non_indexed(GL_POINTS);
}

void sample_gl_galaxy::on_window_resize(int2 size)
{
    if (size.x == current_width && size.y == current_height) return;
    current_width = size.x;
    current_height = size.y;

    hdr_color_texture = gl_texture_2d();
    hdr_depth_texture = gl_texture_2d();
    hdr_framebuffer = gl_framebuffer();

    setup_hdr_framebuffer(size.x, size.y);
    composer.resize(size.x, size.y);
}

void sample_gl_galaxy::on_input(const app_input_event & event)
{
    imgui->update_input(event);
    cam.handle_input(event);

    if (event.type == app_input_event::MOUSE && event.value.x == GLFW_MOUSE_BUTTON_LEFT)
    {
        mouse_pressed = event.is_down();
    }

    if (event.type == app_input_event::CURSOR)
    {
        // Ray-plane intersection to get mouse world position on Y=0 plane
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        float ndc_x = (event.cursor.x / static_cast<float>(width)) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (event.cursor.y / static_cast<float>(height)) * 2.0f;

        float aspect = static_cast<float>(width) / static_cast<float>(height);
        float4x4 inv_vp = inverse(cam.get_viewproj_matrix(aspect));

        float4 near_ndc = float4(ndc_x, ndc_y, -1.0f, 1.0f);
        float4 far_ndc = float4(ndc_x, ndc_y, 1.0f, 1.0f);

        float4 near_world = inv_vp * near_ndc;
        float4 far_world = inv_vp * far_ndc;
        near_world /= near_world.w;
        far_world /= far_world.w;

        float3 ray_origin = near_world.xyz();
        float3 ray_dir = normalize(far_world.xyz() - near_world.xyz());

        // Intersect with Y=0 plane
        if (std::abs(ray_dir.y) > 0.0001f)
        {
            float t = -ray_origin.y / ray_dir.y;
            if (t > 0.0f) mouse_world_pos = ray_origin + ray_dir * t;
        }
    }
}

void sample_gl_galaxy::on_update(const app_update_event & e)
{
    cam.update(e.timestep_ms);
}

void sample_gl_galaxy::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (width != current_width || height != current_height)
    {
        on_window_resize({width, height});
    }

    float delta_time = std::min(1.0f / ImGui::GetIO().Framerate, 0.033f);


    // Star init (once, or on regeneration)
    if (!stars_initialized || needs_star_regen)
    {
        star_init_compute.bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, star_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, star_originals_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, star_velocities_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, star_density_buf);
        star_init_compute.uniform("u_galaxy_radius", config.galaxy_radius);
        star_init_compute.uniform("u_galaxy_thickness", config.galaxy_thickness);
        star_init_compute.uniform("u_spiral_tightness", config.spiral_tightness);
        star_init_compute.uniform("u_arm_count", static_cast<float>(config.arm_count));
        star_init_compute.uniform("u_arm_width", config.arm_width);
        star_init_compute.uniform("u_randomness", config.randomness);
        star_init_compute.uniform("u_count", config.star_count);

        uint32_t groups = (config.star_count + 255) / 256;
        star_init_compute.dispatch(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        star_init_compute.unbind();

        stars_initialized = true;
        needs_star_regen = false;
    }

    // Cloud init (once, or on regeneration)
    if (!clouds_initialized || needs_cloud_regen)
    {
        cloud_init_compute.bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cloud_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cloud_originals_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cloud_colors_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cloud_sizes_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, cloud_rotations_buf);
        cloud_init_compute.uniform("u_galaxy_radius", config.galaxy_radius);
        cloud_init_compute.uniform("u_galaxy_thickness", config.galaxy_thickness);
        cloud_init_compute.uniform("u_spiral_tightness", config.spiral_tightness);
        cloud_init_compute.uniform("u_arm_count", static_cast<float>(config.arm_count));
        cloud_init_compute.uniform("u_arm_width", config.arm_width);
        cloud_init_compute.uniform("u_randomness", config.randomness);
        cloud_init_compute.uniform("u_cloud_tint", float4(config.cloud_tint, 1.0f));
        cloud_init_compute.uniform("u_count", config.cloud_count);

        uint32_t groups = (config.cloud_count + 255) / 256;
        cloud_init_compute.dispatch(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        cloud_init_compute.unbind();

        clouds_initialized = true;
        needs_cloud_regen = false;
    }

    // Star update (every frame)
    {
        star_update_compute.bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, star_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, star_originals_buf);
        star_update_compute.uniform("u_rotation_speed", config.rotation_speed);
        star_update_compute.uniform("u_delta_time", delta_time);
        star_update_compute.uniform("u_mouse_pos", float4(mouse_world_pos, 0.0f));
        star_update_compute.uniform("u_mouse_active", mouse_pressed ? 1.0f : 0.0f);
        star_update_compute.uniform("u_mouse_force", config.mouse_force);
        star_update_compute.uniform("u_mouse_radius", config.mouse_radius);
        star_update_compute.uniform("u_count", config.star_count);

        uint32_t groups = (config.star_count + 255) / 256;
        star_update_compute.dispatch(groups, 1, 1);
        star_update_compute.unbind();
    }

    // Cloud update (every frame)
    {
        cloud_update_compute.bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cloud_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cloud_originals_buf);
        cloud_update_compute.uniform("u_rotation_speed", config.rotation_speed);
        cloud_update_compute.uniform("u_delta_time", delta_time);
        cloud_update_compute.uniform("u_mouse_pos", float4(mouse_world_pos, 0.0f));
        cloud_update_compute.uniform("u_mouse_active", mouse_pressed ? 1.0f : 0.0f);
        cloud_update_compute.uniform("u_mouse_force", config.mouse_force);
        cloud_update_compute.uniform("u_mouse_radius", config.mouse_radius);
        cloud_update_compute.uniform("u_count", config.cloud_count);

        uint32_t groups = (config.cloud_count + 255) / 256;
        cloud_update_compute.dispatch(groups, 1, 1);
        cloud_update_compute.unbind();
    }

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    // Render to HDR FBO

    glBindFramebuffer(GL_FRAMEBUFFER, hdr_framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float4x4 viewproj = cam.get_viewproj_matrix(aspect);
    float2 viewport = float2(static_cast<float>(width), static_cast<float>(height));

    // Draw background starfield (alpha blend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        starfield_shader.bind();
        starfield_shader.uniform("u_viewproj", viewproj);
        starfield_shader.uniform("u_viewport", viewport);
        starfield_mesh.draw_elements();
        starfield_shader.unbind();
    }

    // Draw clouds (additive blend, rendered before stars per original)
    if (config.cloud_count > 0)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        glBindVertexArray(particle_vao);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cloud_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cloud_colors_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cloud_sizes_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, cloud_rotations_buf);

        cloud_shader.bind();
        cloud_shader.uniform("u_viewproj", viewproj);
        cloud_shader.uniform("u_cloud_size", config.cloud_size);
        cloud_shader.uniform("u_viewport", viewport);
        cloud_shader.uniform("u_cloud_opacity", config.cloud_opacity);
        cloud_shader.texture("s_cloud_texture", 0, cloud_texture, GL_TEXTURE_2D);
        glDrawArrays(GL_POINTS, 0, config.cloud_count);
        cloud_shader.unbind();
    }

    // Draw stars
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        glBindVertexArray(particle_vao);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, star_positions_buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, star_density_buf);

        star_shader.bind();
        star_shader.uniform("u_viewproj", viewproj);
        star_shader.uniform("u_particle_size", config.particle_size);
        star_shader.uniform("u_viewport", viewport);
        star_shader.uniform("u_dense_color", float4(config.dense_color, 1.0f));
        star_shader.uniform("u_sparse_color", float4(config.sparse_color, 1.0f));
        float count_normalized_brightness = config.star_brightness * (750000.0f / static_cast<float>(config.star_count));
        star_shader.uniform("u_brightness", count_normalized_brightness);
        glDrawArrays(GL_POINTS, 0, config.star_count);
        star_shader.unbind();
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    composer.render(hdr_color_texture, width, height);

    imgui->begin_frame();

    gui::imgui_fixed_window_begin("Galaxy Simulation", ui_rect{{0, 0}, {300, height}});

    ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int32_t star_count_k = config.star_count / 1000;
        if (ImGui::SliderInt("Stars (K)", &star_count_k, 1, 1000))
        {
            config.star_count = star_count_k * 1000;
            allocate_star_ssbos();
        }
        ImGui::Text("Particles: %d stars + %d clouds", config.star_count, config.cloud_count);
    }

    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Star Size", &config.particle_size, 0.01f, 0.5f);
        ImGui::SliderFloat("Star Brightness", &config.star_brightness, 0.0f, 2.0f);
        ImGui::ColorEdit3("Dense Color", &config.dense_color.x);
        ImGui::ColorEdit3("Sparse Color", &config.sparse_color.x);
    }

    if (ImGui::CollapsingHeader("Clouds"))
    {
        int32_t cloud_count_k = config.cloud_count / 1000;
        if (ImGui::SliderInt("Cloud Count (K)", &cloud_count_k, 0, 100))
        {
            config.cloud_count = cloud_count_k * 1000;
            allocate_cloud_ssbos();
        }
        ImGui::SliderFloat("Cloud Size", &config.cloud_size, 0.5f, 10.0f);
        ImGui::SliderFloat("Cloud Opacity", &config.cloud_opacity, 0.0f, 1.0f);
        if (ImGui::ColorEdit3("Cloud Tint", &config.cloud_tint.x)) needs_cloud_regen = true;
    }

    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &bloom_pass->config.bloom_enabled);
        ImGui::SliderFloat("Threshold", &bloom_pass->config.threshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Knee", &bloom_pass->config.knee, 0.0f, 1.0f);
        ImGui::SliderFloat("Strength", &bloom_pass->config.strength, 0.0f, 3.0f);
        ImGui::SliderFloat("Radius", &bloom_pass->config.radius, 0.0f, 1.0f);
        ImGui::SliderFloat("Exposure", &bloom_pass->config.exposure, 0.1f, 5.0f);
        ImGui::SliderFloat("Gamma", &bloom_pass->config.gamma, 1.0f, 3.0f);
        const char * tonemap_modes[] = {"None", "Filmic", "Hejl", "ACES 2.0", "ACES 1.0"};
        ImGui::Combo("Tonemap", &bloom_pass->config.tonemap_mode, tonemap_modes, IM_ARRAYSIZE(tonemap_modes));
    }

    if (ImGui::CollapsingHeader("Galaxy Structure"))
    {
        ImGui::SliderFloat("Rotation Speed", &config.rotation_speed, 0.0f, 2.0f);
        bool regen = false;
        regen |= ImGui::SliderFloat("Spiral Tightness", &config.spiral_tightness, 0.0f, 10.0f);
        int arm_count = config.arm_count;
        if (ImGui::SliderInt("Arm Count", &arm_count, 1, 4))
        {
            config.arm_count = arm_count;
            regen = true;
        }
        regen |= ImGui::SliderFloat("Arm Width", &config.arm_width, 1.0f, 5.0f);
        regen |= ImGui::SliderFloat("Randomness", &config.randomness, 0.0f, 5.0f);
        regen |= ImGui::SliderFloat("Galaxy Radius", &config.galaxy_radius, 5.0f, 20.0f);
        regen |= ImGui::SliderFloat("Thickness", &config.galaxy_thickness, 0.1f, 10.0f);

        if (regen)
        {
            needs_star_regen = true;
            needs_cloud_regen = true;
        }
    }

    if (ImGui::CollapsingHeader("Interaction"))
    {
        ImGui::SliderFloat("Force", &config.mouse_force, 0.0f, 10.0f);
        ImGui::SliderFloat("Radius##mouse", &config.mouse_radius, 1.0f, 15.0f);
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
        sample_gl_galaxy app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

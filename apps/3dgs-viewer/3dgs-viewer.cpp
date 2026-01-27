#include "polymer-core/lib-polymer.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/camera-controllers.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-gfx-gl/gl-texture-view.hpp"
#include "polymer-model-io/gaussian-splat-io.hpp"

#include <algorithm>
#include <chrono>

using namespace polymer;

/////////////////////////////////
//   Gaussian Splat Renderer   //
/////////////////////////////////

class gaussian_splat_renderer
{

public:

    gaussian_splat_renderer() = default;
    ~gaussian_splat_renderer() = default;

    void initialize(uint32_t width, uint32_t height)
    {
        width_ = width;
        height_ = height;

        load_shaders();
        create_output_texture();
    }

    void resize(uint32_t width, uint32_t height)
    {
        if (width_ != width || height_ != height)
        {
            width_ = width;
            height_ = height;
            create_output_texture();
            recreate_tile_buffers();
        }
    }

    void set_scene(const gaussian_splat_scene & scene)
    {
        scene_ = scene;
        num_gaussians_ = static_cast<uint32_t>(scene_.vertices.size());
        if (num_gaussians_ == 0) return;

        const size_t vertex_size = sizeof(gaussian_vertex);
        const size_t attr_size= 64;
        vertex_buffer_.set_buffer_data(num_gaussians_ * vertex_size, scene_.vertices.data(), GL_STATIC_DRAW);
        cov3d_buffer_.set_buffer_data(num_gaussians_ * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        vertex_attr_buffer_.set_buffer_data(num_gaussians_ * attr_size, nullptr, GL_DYNAMIC_DRAW);
        tile_overlap_buffer_.set_buffer_data(num_gaussians_ * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        prefix_sum_buffer_.set_buffer_data(num_gaussians_ * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

        recreate_tile_buffers();
        recreate_sort_buffers();
    }

    void render(const perspective_camera & cam, uint32_t sh_degree, float scale_modifier)
    {
        if (num_gaussians_ == 0) return;

        auto start_time = std::chrono::high_resolution_clock::now();

        precompute_cov3d(scale_modifier);
        preprocess(cam, sh_degree); // project, cull, compute 2D cov, SH
        compute_prefix_sum();

        uint32_t total_instances = 0;
        glGetNamedBufferSubData(prefix_sum_buffer_, (num_gaussians_ - 1) * sizeof(uint32_t), sizeof(uint32_t), &total_instances);

        if (total_instances == 0)
        {
            glClearTexImage(output_texture_, 0, GL_RGBA, GL_FLOAT, nullptr); // Clear output
            return;
        }

        // clamp to reasonable maximum (10M instances)
        const uint32_t MAX_INSTANCES = 10000000;
        if (total_instances > MAX_INSTANCES)
        {
            std::cerr << "Warning: total_instances (" << total_instances << ") exceeds maximum, clamping." << std::endl;
            total_instances = MAX_INSTANCES;
        }

        visible_gaussians_ = total_instances;

        // Ensure sort buffers are large enough
        if (total_instances > max_sort_instances_)
        {
            max_sort_instances_ = total_instances * 2;
            recreate_sort_buffers();
        }

        generate_sort_keys(total_instances);
        sort_instances_cpu(total_instances); // workaround until gpu radix support is implemented
        compute_tile_boundaries(total_instances);
        render_tiles();

        auto end_time = std::chrono::high_resolution_clock::now();
        last_frame_time_ms_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    }

    GLuint get_output_texture() const { return output_texture_; }
    uint32_t get_visible_count() const { return visible_gaussians_; }
    float get_frame_time_ms() const { return last_frame_time_ms_; }

private:

    void load_shaders()
    {
        precomp_cov3d_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/precomp_cov3d.comp"));
        preprocess_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/preprocess.comp"));
        prefix_sum_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/prefix_sum.comp"));
        preprocess_sort_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/preprocess_sort.comp"));
        tile_boundary_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/tile_boundary.comp"));
        render_shader_ = std::make_unique<gl_shader_compute>(read_file_text("../assets/shaders/3dgs/render.comp"));
    }

    void create_output_texture()
    {
        output_texture_ = gl_texture_2d();
        output_texture_.setup(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    void recreate_tile_buffers()
    {
        uint32_t tiles_x = (width_ + 15) / 16;
        uint32_t tiles_y = (height_ + 15) / 16;
        num_tiles_ = tiles_x * tiles_y;

        // Tile boundary buffer: [start, end] per tile
        tile_boundary_buffer_.set_buffer_data(num_tiles_ * 2 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    }

    void recreate_sort_buffers()
    {
        if (max_sort_instances_ == 0) max_sort_instances_ = 1024 * 1024;
        sort_keys_.resize(max_sort_instances_);
        sort_payloads_.resize(max_sort_instances_);
        sort_keys_buffer_.set_buffer_data(max_sort_instances_ * sizeof(uint64_t), nullptr, GL_DYNAMIC_DRAW);
        sort_payloads_buffer_.set_buffer_data(max_sort_instances_ * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    }

    void precompute_cov3d(float scale_modifier)
    {
        precomp_cov3d_shader_->bind_ssbo(0, vertex_buffer_);
        precomp_cov3d_shader_->bind_ssbo(1, cov3d_buffer_);

        struct {
            float scale_modifier;
            uint32_t num_gaussians;
            float pad[2];
        } params = { scale_modifier, num_gaussians_, {0, 0} };

        GLuint ubo;
        glCreateBuffers(1, &ubo);
        glNamedBufferData(ubo, sizeof(params), &params, GL_DYNAMIC_DRAW);
        precomp_cov3d_shader_->bind_ubo(2, ubo);

        uint32_t groups = (num_gaussians_ + 255) / 256;
        precomp_cov3d_shader_->dispatch_and_barrier(groups, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

        glDeleteBuffers(1, &ubo);
    }

    void preprocess(const perspective_camera & cam, uint32_t sh_degree)
    {
        preprocess_shader_->bind_ssbo(0, vertex_buffer_);
        preprocess_shader_->bind_ssbo(1, cov3d_buffer_);
        preprocess_shader_->bind_ssbo(3, vertex_attr_buffer_);
        preprocess_shader_->bind_ssbo(4, tile_overlap_buffer_);

        float4x4 view_mat = cam.get_view_matrix();
        float4x4 proj_mat = cam.get_projection_matrix(static_cast<float>(width_) / height_);

        float tan_fovy = std::tan(cam.vfov * 0.5f);
        float tan_fovx = tan_fovy * (static_cast<float>(width_) / height_);

        struct alignas(16) {
            float4 camera_position;
            float4x4 proj_mat;
            float4x4 view_mat;
            uint32_t width;
            uint32_t height;
            float tan_fovx;
            float tan_fovy;
            uint32_t sh_degree;
            uint32_t num_gaussians;
            float pad[2];
        } params;

        params.camera_position = float4(cam.pose.position, 1.0f);
        params.proj_mat = proj_mat * view_mat;
        params.view_mat = view_mat;
        params.width = width_;
        params.height = height_;
        params.tan_fovx = tan_fovx;
        params.tan_fovy = tan_fovy;
        params.sh_degree = std::min(sh_degree, scene_.sh_degree);
        params.num_gaussians = num_gaussians_;

        GLuint ubo;
        glCreateBuffers(1, &ubo);
        glNamedBufferData(ubo, sizeof(params), &params, GL_DYNAMIC_DRAW);
        preprocess_shader_->bind_ubo(2, ubo);

        uint32_t groups = (num_gaussians_ + 255) / 256;
        preprocess_shader_->dispatch_and_barrier(groups, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

        glDeleteBuffers(1, &ubo);
    }

    void compute_prefix_sum()
    {
        // CPU-based prefix sum for correctness (GPU version has race conditions)
        std::vector<uint32_t> tile_overlaps(num_gaussians_);
        glGetNamedBufferSubData(tile_overlap_buffer_, 0, num_gaussians_ * sizeof(uint32_t), tile_overlaps.data());

        // Inclusive prefix sum
        uint32_t running_sum = 0;
        for (uint32_t i = 0; i < num_gaussians_; i++)
        {
            running_sum += tile_overlaps[i];
            tile_overlaps[i] = running_sum;
        }

        // Upload result
        glNamedBufferSubData(prefix_sum_buffer_, 0, num_gaussians_ * sizeof(uint32_t), tile_overlaps.data());
    }

    void generate_sort_keys(uint32_t total_instances)
    {
        preprocess_sort_shader_->bind_ssbo(0, vertex_attr_buffer_);
        preprocess_sort_shader_->bind_ssbo(1, prefix_sum_buffer_);
        preprocess_sort_shader_->bind_ssbo(2, sort_keys_buffer_);
        preprocess_sort_shader_->bind_ssbo(3, sort_payloads_buffer_);

        uint32_t tiles_x = (width_ + 15) / 16;

        struct {
            uint32_t tiles_x;
            uint32_t num_gaussians;
            float pad[2];
        } params = { tiles_x, num_gaussians_, {0, 0} };

        GLuint ubo;
        glCreateBuffers(1, &ubo);
        glNamedBufferData(ubo, sizeof(params), &params, GL_DYNAMIC_DRAW);
        preprocess_sort_shader_->bind_ubo(4, ubo);

        uint32_t groups = (num_gaussians_ + 255) / 256;
        preprocess_sort_shader_->dispatch_and_barrier(groups, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

        glDeleteBuffers(1, &ubo);
    }

    void sort_instances_cpu(uint32_t total_instances)
    {
        // CPU sorting fallback (GPU radix sort is complex to implement correctly)
        glGetNamedBufferSubData(sort_keys_buffer_, 0, total_instances * sizeof(uint64_t), sort_keys_.data());
        glGetNamedBufferSubData(sort_payloads_buffer_, 0, total_instances * sizeof(uint32_t), sort_payloads_.data());

        // Create index array for sorting
        std::vector<uint32_t> indices(total_instances);
        for (uint32_t i = 0; i < total_instances; i++) indices[i] = i;

        // Sort by key
        std::sort(indices.begin(), indices.end(), [this](uint32_t a, uint32_t b) {
            return sort_keys_[a] < sort_keys_[b];
        });

        // Reorder
        std::vector<uint64_t> sorted_keys(total_instances);
        std::vector<uint32_t> sorted_payloads(total_instances);
        for (uint32_t i = 0; i < total_instances; i++)
        {
            sorted_keys[i] = sort_keys_[indices[i]];
            sorted_payloads[i] = sort_payloads_[indices[i]];
        }

        // Upload sorted data
        glNamedBufferSubData(sort_keys_buffer_, 0, total_instances * sizeof(uint64_t), sorted_keys.data());
        glNamedBufferSubData(sort_payloads_buffer_, 0, total_instances * sizeof(uint32_t), sorted_payloads.data());
    }

    void compute_tile_boundaries(uint32_t total_instances)
    {
        std::vector<uint32_t> zeros(num_tiles_ * 2, 0);
        glNamedBufferSubData(tile_boundary_buffer_, 0, zeros.size() * sizeof(uint32_t), zeros.data());

        tile_boundary_shader_->bind_ssbo(0, sort_keys_buffer_);
        tile_boundary_shader_->bind_ssbo(1, tile_boundary_buffer_);

        struct {
            uint32_t num_instances;
            uint32_t num_tiles;
            float pad[2];
        } params = { total_instances, num_tiles_, {0, 0} };

        GLuint ubo;
        glCreateBuffers(1, &ubo);
        glNamedBufferData(ubo, sizeof(params), &params, GL_DYNAMIC_DRAW);
        tile_boundary_shader_->bind_ubo(2, ubo);

        uint32_t groups = (total_instances + 255) / 256;
        tile_boundary_shader_->dispatch_and_barrier(groups, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

        glDeleteBuffers(1, &ubo);
    }

    void render_tiles()
    {
        render_shader_->bind_ssbo(0, vertex_attr_buffer_);
        render_shader_->bind_ssbo(1, tile_boundary_buffer_);
        render_shader_->bind_ssbo(2, sort_payloads_buffer_);
        render_shader_->bind_image(3, output_texture_, GL_WRITE_ONLY, GL_RGBA8);

        struct {
            uint32_t width;
            uint32_t height;
            float pad[2];
        } params = { width_, height_, {0, 0} };

        GLuint ubo;
        glCreateBuffers(1, &ubo);
        glNamedBufferData(ubo, sizeof(params), &params, GL_DYNAMIC_DRAW);
        render_shader_->bind_ubo(4, ubo);

        uint32_t tiles_x = (width_ + 15) / 16;
        uint32_t tiles_y = (height_ + 15) / 16;
        render_shader_->dispatch_and_barrier(tiles_x, tiles_y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glDeleteBuffers(1, &ubo);
    }

    // Data
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t num_gaussians_ = 0;
    uint32_t num_tiles_ = 0;
    uint32_t max_sort_instances_ = 0;
    uint32_t visible_gaussians_ = 0;
    float last_frame_time_ms_ = 0.0f;

    gaussian_splat_scene scene_;

    // Buffers
    gl_buffer vertex_buffer_;
    gl_buffer cov3d_buffer_;
    gl_buffer vertex_attr_buffer_;
    gl_buffer tile_overlap_buffer_;
    gl_buffer prefix_sum_buffer_;
    gl_buffer tile_boundary_buffer_;
    gl_buffer sort_keys_buffer_;
    gl_buffer sort_payloads_buffer_;

    // CPU sorting buffers
    std::vector<uint64_t> sort_keys_;
    std::vector<uint32_t> sort_payloads_;

    // Shaders
    std::unique_ptr<gl_shader_compute> precomp_cov3d_shader_;
    std::unique_ptr<gl_shader_compute> preprocess_shader_;
    std::unique_ptr<gl_shader_compute> prefix_sum_shader_;
    std::unique_ptr<gl_shader_compute> preprocess_sort_shader_;
    std::unique_ptr<gl_shader_compute> tile_boundary_shader_;
    std::unique_ptr<gl_shader_compute> render_shader_;

    // Output
    gl_texture_2d output_texture_;
};

/////////////////////////////////
//   3DGS Viewer Application   //
/////////////////////////////////

struct gs_viewer_app final : public polymer_app
{
    perspective_camera cam;
    camera_controller_fps flycam;

    std::unique_ptr<gui::imgui_instance> igm;
    std::unique_ptr<simple_texture_view> fullscreen_surface;
    std::unique_ptr<gaussian_splat_renderer> renderer;

    gaussian_splat_scene scene; // fixme since this copies the gsplat
    std::string scene_filename;

    int sh_degree_override = 3;
    float scale_modifier = 1.0f;
    bool show_imgui = true;

    gs_viewer_app() : polymer_app(1920, 1080, "3DGS Viewer", 1)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        // Load font
        auto droidSansTTFBytes = read_file_binary("../assets/fonts/droid_sans.ttf");
        igm.reset(new gui::imgui_instance(window));
        gui::make_light_theme();
        igm->add_font(droidSansTTFBytes);

        // Setup camera (use LH convention for 3DGS compatibility)
        cam.pose = lookat_lh({ 0, 0, 5.f }, { 0, 0, 0 });
        cam.vfov = to_radians(45.0);  // 45 degrees in radians (matches reference)
        cam.nearclip = 0.1f;
        cam.farclip = 1000.f;
        flycam.set_camera(&cam);
        flycam.leftHanded = true;  // 3DGS uses LH convention
        flycam.movementSpeed = 24.0;

        // Initialize renderer
        renderer = std::make_unique<gaussian_splat_renderer>();
        renderer->initialize(width, height);

        // Initialize fullscreen surface for blitting
        fullscreen_surface = std::make_unique<simple_texture_view>();

        // Try to load default scene
        load_scene("../assets/Placenta.ply");
    }

    void load_scene(const std::string & path)
    {
        if (!is_gaussian_splat_ply(path))
        {
            std::cerr << "Not a valid gaussian splat PLY file: " << path << std::endl;
            return;
        }

        scene = import_gaussian_splat_ply(path);
        scene_filename = get_filename_with_extension(path);

        if (scene.vertices.size() > 0)
        {
            renderer->set_scene(scene);
            sh_degree_override = scene.sh_degree;
            reset_camera();
        }
    }

    void reset_camera()
    {
        if (scene.vertices.empty()) return;

        // Compute center of mass
        float3 center_of_mass(0.0f);
        for (const auto & v : scene.vertices)
        {
            center_of_mass += float3(v.position.x, v.position.y, v.position.z);
        }
        center_of_mass /= static_cast<float>(scene.vertices.size());

        // Compute radius from center of mass
        float max_dist_sq = 0.0f;
        for (const auto & v : scene.vertices)
        {
            float3 pos(v.position.x, v.position.y, v.position.z);
            float dist_sq = length2(pos - center_of_mass);
            max_dist_sq = std::max(max_dist_sq, dist_sq);
        }
        float radius = std::sqrt(max_dist_sq);

        // Position camera to see the whole scene (use LH convention)
        float distance = (radius / std::tan(cam.vfov * 0.5f)) * 0.1f;
        cam.pose = lookat_lh(center_of_mass + float3(0, 0, distance), center_of_mass);
        flycam.update_yaw_pitch();

        std::cout << "Camera reset: center=(" << center_of_mass.x << ", " << center_of_mass.y << ", " << center_of_mass.z
                  << "), radius=" << radius << ", distance=" << distance << std::endl;
    }

    void on_window_resize(int2 size) override
    {
        if (size.x > 0 && size.y > 0)
        {
            renderer->resize(size.x, size.y);
        }
    }

    void on_input(const app_input_event & event) override
    {
        igm->update_input(event);

        if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        {
            flycam.reset();
            return;
        }

        if (event.mods == 0) flycam.handle_input(event);

        if (event.type == app_input_event::KEY && event.action == GLFW_RELEASE)
        {
            if (event.value[0] == GLFW_KEY_R)
            {
                reset_camera();
            }
            if (event.value[0] == GLFW_KEY_TAB)
            {
                show_imgui = !show_imgui;
            }
        }
    }

    void on_update(const app_update_event & e) override
    {
        flycam.update(e.timestep_ms);
    }

    void on_drop(std::vector<std::string> filepaths) override
    {
        for (const auto & path : filepaths)
        {
            const std::string ext = get_extension(path);
            if (ext == "ply")
            {
                load_scene(path);
                break;
            }
        }
    }

    void on_draw() override
    {
        glfwMakeContextCurrent(window);

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderer->render(cam, sh_degree_override, scale_modifier);
        fullscreen_surface->draw(renderer->get_output_texture());

        if (show_imgui)
        {
            igm->begin_frame();
            draw_ui();
            igm->end_frame();
        }

        glfwSwapBuffers(window);
    }

    void draw_ui()
    {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);

        ImGui::Begin("Polymer 3DGS Viewer");

        ImGui::Text("Scene/PLY: %s", scene_filename.empty() ? "None" : scene_filename.c_str());
        ImGui::Text("Gaussians: %zu", scene.vertices.size());
        ImGui::Text("SH Degree: %u", scene.sh_degree);

        ImGui::Separator();

        ImGui::Text("Visible: %u (%.1f%%)", renderer->get_visible_count(), scene.vertices.size() > 0 ? 100.0f * renderer->get_visible_count() / scene.vertices.size() : 0.0f);
        ImGui::Text("Frame Time: %.2f ms", renderer->get_frame_time_ms());
        ImGui::Text("FPS: %.1f", 1000.0f / std::max(renderer->get_frame_time_ms(), 0.001f));

        ImGui::Separator();

        ImGui::SliderInt("SH Degree", &sh_degree_override, 0, 3);
        ImGui::SliderFloat("Scale", &scale_modifier, 0.1f, 3.0f);

        if (ImGui::Button("Reset Camera (R)"))
        {
            reset_camera();
        }

        ImGui::End();
    }
};

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        gs_viewer_app app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

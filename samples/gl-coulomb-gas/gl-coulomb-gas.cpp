// 2D Coulomb Gas Simulator
//
// Ported from Simon Halvdansson's WebGPU implementation.
// Original: https://simonhalvdansson.github.io/posts/coulomb-gas/index.html
//
// Simulates exact pairwise Coulomb repulsion in 2D with 7 confining potentials
// using a tiled shared-memory N-body computation in a compute shader.

#include "polymer-core/lib-polymer.hpp"

#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/glfw-app.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

#include <algorithm>
#include <cmath>

using namespace polymer;
using namespace gui;

constexpr int32_t WG_SIZE = 256;
constexpr int32_t MAX_INSERTED_PARTICLES = 256;
constexpr float SIM_TO_CLIP = 0.7f;
constexpr float DAMPING = 0.8f;

inline int32_t default_radius_for_n(int32_t n)
{
    if (n <= 500) return 4;
    if (n == 1000 || n == 2000) return 3;
    if (n == 5000) return 2;
    return 1;
}

struct inserted_particle_data
{
    float x = 0.0f;
    float y = 0.0f;
    float c = 0.25f;
    float _pad = 0.0f;
};

struct coulomb_gas_config
{
    int32_t n = 20000;
    int32_t steps_per_frame = 1;
    int32_t dt_slider = 5;
    int32_t pot = 0;
    int32_t lemniscate_t_slider = 50;
    int32_t lem_interpol_slider = 25;
    int32_t particle_size_px = 1;
    float opacity = 1.0f;
    bool is_dark = true;
};

struct sample_gl_coulomb_gas final : public polymer_app
{
    std::unique_ptr<imgui_instance> imgui;

    coulomb_gas_config config;

    gl_shader_compute sim_compute;
    gl_shader render_shader;

    gl_buffer pos_a, pos_b, vel_a, vel_b;
    gl_buffer inserted_buf;

    gl_buffer quad_vbo;
    gl_vertex_array_object vao;

    int32_t ping = 0;
    int32_t frame_counter = 0;

    std::vector<inserted_particle_data> inserted_particles;
    int32_t active_placing_index = -1;
    bool placement_dragging = false;
    bool inserted_dirty = true;

    float2 cursor_pos = {0, 0};

    sample_gl_coulomb_gas();
    ~sample_gl_coulomb_gas() = default;

    void allocate_particle_buffers(bool randomize);
    void update_inserted_buffer();
    void set_n(int32_t new_n);
    void set_pot(int32_t new_pot);
    void place_particle_from_cursor(float2 cursor);

    void on_input(const app_input_event & event) override;
    void on_update(const app_update_event & e) override;
    void on_draw() override;
};

sample_gl_coulomb_gas::sample_gl_coulomb_gas() : polymer_app(1280, 1280, "coulomb-gas-sim", 4)
{
    glfwMakeContextCurrent(window);

    imgui.reset(new imgui_instance(window, true));
    gui::make_light_theme();

    const std::string asset_base = global_asset_dir::get()->get_asset_dir();
    const std::string shader_base = asset_base + "/shaders/coulomb-gas/";

    sim_compute = gl_shader_compute(read_file_text(shader_base + "coulomb_gas_sim_comp.glsl"));
    render_shader = gl_shader(
        read_file_text(shader_base + "coulomb_gas_vert.glsl"),
        read_file_text(shader_base + "coulomb_gas_frag.glsl"));

    // Quad VBO: 6 vertices forming 2 triangles
    std::vector<float2> quad_verts = {
        {-0.5f, -0.5f}, { 0.5f, -0.5f}, { 0.5f,  0.5f},
        {-0.5f, -0.5f}, { 0.5f,  0.5f}, {-0.5f,  0.5f},
    };
    quad_vbo.set_buffer_data(quad_verts.size() * sizeof(float2), quad_verts.data(), GL_STATIC_DRAW);

    // Inserted particles buffer (pre-allocated to max size)
    inserted_buf.set_buffer_data(MAX_INSERTED_PARTICLES * sizeof(inserted_particle_data), nullptr, GL_DYNAMIC_DRAW);

    // Particle SSBOs
    config.particle_size_px = default_radius_for_n(config.n);
    allocate_particle_buffers(true);

    gl_check_error(__FILE__, __LINE__);
}

void sample_gl_coulomb_gas::allocate_particle_buffers(bool randomize)
{
    GLsizeiptr buf_size = static_cast<GLsizeiptr>(config.n) * sizeof(float2);

    if (randomize)
    {
        uniform_random_gen rng;
        std::vector<float> pos_data(config.n * 2);
        std::vector<float> vel_data(config.n * 2, 0.0f);

        for (int32_t i = 0; i < config.n; ++i)
        {
            pos_data[2 * i + 0] = (rng.random_float() - 0.5f) * 2.7f;
            pos_data[2 * i + 1] = (rng.random_float() - 0.5f) * 2.7f;
        }

        pos_a.set_buffer_data(buf_size, pos_data.data(), GL_DYNAMIC_DRAW);
        vel_a.set_buffer_data(buf_size, vel_data.data(), GL_DYNAMIC_DRAW);
    }
    else
    {
        pos_a.set_buffer_data(buf_size, nullptr, GL_DYNAMIC_DRAW);
        vel_a.set_buffer_data(buf_size, nullptr, GL_DYNAMIC_DRAW);
    }

    pos_b.set_buffer_data(buf_size, nullptr, GL_DYNAMIC_DRAW);
    vel_b.set_buffer_data(buf_size, nullptr, GL_DYNAMIC_DRAW);

    ping = 0;
}

void sample_gl_coulomb_gas::update_inserted_buffer()
{
    if (!inserted_dirty) return;

    int32_t count = static_cast<int32_t>(std::min(inserted_particles.size(), static_cast<size_t>(MAX_INSERTED_PARTICLES)));
    std::vector<inserted_particle_data> data(MAX_INSERTED_PARTICLES);
    for (int32_t i = 0; i < count; ++i) data[i] = inserted_particles[i];

    inserted_buf.set_buffer_data(MAX_INSERTED_PARTICLES * sizeof(inserted_particle_data), data.data(), GL_DYNAMIC_DRAW);
    inserted_dirty = false;
}

void sample_gl_coulomb_gas::set_n(int32_t new_n)
{
    if (new_n >= 10000) set_pot(0);

    config.n = new_n;
    config.particle_size_px = default_radius_for_n(config.n);

    if (config.n == 200000 || config.n == 500000)
    {
        config.dt_slider = 5;
    }

    allocate_particle_buffers(true);
    frame_counter = 0;
}

void sample_gl_coulomb_gas::set_pot(int32_t new_pot)
{
    config.pot = new_pot;
}

void sample_gl_coulomb_gas::place_particle_from_cursor(float2 cursor)
{
    if (active_placing_index < 0 || active_placing_index >= static_cast<int32_t>(inserted_particles.size())) return;

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    float ndc_x = (cursor.x / static_cast<float>(width)) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (cursor.y / static_cast<float>(height)) * 2.0f;

    inserted_particles[active_placing_index].x = std::clamp(ndc_x / SIM_TO_CLIP, -1.0f, 1.0f);
    inserted_particles[active_placing_index].y = std::clamp(ndc_y / SIM_TO_CLIP, -1.0f, 1.0f);
    inserted_dirty = true;
}

void sample_gl_coulomb_gas::on_input(const app_input_event & event)
{
    imgui->update_input(event);

    if (event.type == app_input_event::CURSOR)
    {
        cursor_pos = event.cursor;
        if (placement_dragging && active_placing_index >= 0)
        {
            place_particle_from_cursor(cursor_pos);
        }
    }

    if (event.type == app_input_event::MOUSE && event.value.x == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (event.is_down() && !ImGui::GetIO().WantCaptureMouse && active_placing_index >= 0)
        {
            placement_dragging = true;
            place_particle_from_cursor(cursor_pos);
        }
        if (event.is_up()) placement_dragging = false;
    }
}

void sample_gl_coulomb_gas::on_update(const app_update_event & e) {}

void sample_gl_coulomb_gas::on_draw()
{
    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    update_inserted_buffer();

    float dt = static_cast<float>(config.dt_slider) / 30000.0f;
    float lemniscate_t = static_cast<float>(config.lemniscate_t_slider) / 50.0f;
    float lem_interpol = static_cast<float>(config.lem_interpol_slider) / 10.0f;
    int32_t inserted_count = static_cast<int32_t>(std::min(inserted_particles.size(), static_cast<size_t>(MAX_INSERTED_PARTICLES)));
    uint32_t num_groups = (static_cast<uint32_t>(config.n) + WG_SIZE - 1) / WG_SIZE;

    sim_compute.bind();

    sim_compute.uniform("u_n", config.n);
    sim_compute.uniform("u_pot", config.pot);
    sim_compute.uniform("u_inserted_count", inserted_count);
    sim_compute.uniform("u_dt", dt);
    sim_compute.uniform("u_damping", DAMPING);
    sim_compute.uniform("u_lemniscate_t", lemniscate_t);
    sim_compute.uniform("u_lem_interpol", lem_interpol);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, inserted_buf);

    for (int32_t s = 0; s < config.steps_per_frame; ++s)
    {
        if (ping == 0)
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos_a);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vel_a);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pos_b);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, vel_b);
        }
        else
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos_b);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vel_b);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pos_a);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, vel_a);
        }

        sim_compute.dispatch(num_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        ping ^= 1;
    }

    sim_compute.unbind();

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glViewport(0, 0, width, height);

    if (config.is_dark)
        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    else
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Determine which buffer contains the most recent positions
    gl_buffer & render_pos_buf = (ping == 1) ? pos_b : pos_a;

    glBindVertexArray(vao);

    // Attribute 0: quad vertex positions (per-vertex)
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float2), nullptr);
    glVertexAttribDivisor(0, 0);

    // Attribute 1: particle sim positions (per-instance)
    glBindBuffer(GL_ARRAY_BUFFER, render_pos_buf);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float2), nullptr);
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    render_shader.bind();
    render_shader.uniform("u_sim_to_clip", SIM_TO_CLIP);
    render_shader.uniform("u_particle_size_px", static_cast<float>(config.particle_size_px));
    render_shader.uniform("u_canvas_w", static_cast<float>(width));
    render_shader.uniform("u_canvas_h", static_cast<float>(height));
    render_shader.uniform("u_alpha", config.opacity);
    render_shader.uniform("u_is_dark", config.is_dark ? 1.0f : 0.0f);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, config.n);

    render_shader.unbind();

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);

    glDisable(GL_BLEND);

    imgui->begin_frame();

    gui::imgui_fixed_window_begin("Coulomb Gas", ui_rect{{0, 0}, {340, height}});

    frame_counter++;
    ImGui::Text("Frame: %d | %.1f FPS", frame_counter, ImGui::GetIO().Framerate);
    ImGui::Separator();

    ImGui::SliderInt("Steps/frame", &config.steps_per_frame, 1, 100);
    ImGui::SliderInt("dt", &config.dt_slider, 1, 40);
    ImGui::Separator();

    // Potential selection
    if (ImGui::CollapsingHeader("Potential", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Symmetric:");
        if (ImGui::RadioButton("Ginibre", config.pot == 0)) set_pot(0);
        ImGui::SameLine();
        if (ImGui::RadioButton("ML l=2", config.pot == 1)) set_pot(1);
        ImGui::SameLine();
        if (ImGui::RadioButton("ML l=10", config.pot == 2)) set_pot(2);

        ImGui::Text("Lemniscate:");
        if (ImGui::RadioButton("k=2", config.pot == 3)) set_pot(3);
        ImGui::SameLine();
        if (ImGui::RadioButton("k=3", config.pot == 4)) set_pot(4);
        ImGui::SameLine();
        if (ImGui::RadioButton("k=5", config.pot == 5)) set_pot(5);
        ImGui::SameLine();
        if (ImGui::RadioButton("Interp", config.pot == 6)) set_pot(6);

        if (config.pot >= 3)
        {
            ImGui::SliderInt("T (critical scaling)", &config.lemniscate_t_slider, 0, 100);
            if (config.pot == 6)
            {
                ImGui::SliderInt("p (interpolation)", &config.lem_interpol_slider, 10, 50);
            }
        }
    }

    ImGui::Separator();

    // Particle count
    if (ImGui::CollapsingHeader("Particle Count", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const int32_t counts[] = {2, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000};
        const char * labels[] = {"2", "50", "100", "200", "500", "1K", "2K", "5K", "10K", "20K", "50K", "100K", "200K", "500K"};

        float avail = ImGui::GetContentRegionAvail().x;
        float x_pos = 0.0f;

        for (int32_t i = 0; i < 14; ++i)
        {
            float btn_w = ImGui::CalcTextSize(labels[i]).x + ImGui::GetStyle().FramePadding.x * 2.0f;

            if (x_pos + btn_w > avail && i > 0)
                x_pos = 0.0f;
            else if (i > 0)
                ImGui::SameLine();

            bool active = (config.n == counts[i]);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

            ImGui::PushID(i);
            if (ImGui::SmallButton(labels[i])) set_n(counts[i]);
            ImGui::PopID();

            if (active) ImGui::PopStyleColor();

            x_pos += btn_w + ImGui::GetStyle().ItemSpacing.x;
        }

        ImGui::Text("Exact pairwise repulsion is O(n^2).");
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderInt("Particle size", &config.particle_size_px, 1, 10);
        ImGui::SliderFloat("Opacity", &config.opacity, 0.1f, 1.0f);
        ImGui::Checkbox("Dark mode", &config.is_dark);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Particles"))
    {
        ImGui::TextWrapped("Each insertion adds a term -c*log|z-p|. Add points, tune charge, place on canvas.");

        for (int32_t i = 0; i < static_cast<int32_t>(inserted_particles.size()); ++i)
        {
            ImGui::PushID(i + 1000);

            bool is_placing = (active_placing_index == i);

            ImGui::Text("Particle %d (%.2f, %.2f)", i + 1, inserted_particles[i].x, inserted_particles[i].y);

            ImGui::SameLine();
            if (is_placing) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(is_placing ? "Placing..." : "Place"))
            {
                active_placing_index = is_placing ? -1 : i;
                placement_dragging = false;
            }
            if (is_placing) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
            {
                inserted_particles.erase(inserted_particles.begin() + i);
                if (active_placing_index == i) active_placing_index = -1;
                else if (active_placing_index > i) active_placing_index--;
                inserted_dirty = true;
                ImGui::PopID();
                break;
            }

            float charge = inserted_particles[i].c;
            if (ImGui::SliderFloat("Charge", &charge, 0.0f, 1.0f))
            {
                inserted_particles[i].c = charge;
                inserted_dirty = true;
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        bool at_max = static_cast<int32_t>(inserted_particles.size()) >= MAX_INSERTED_PARTICLES;
        if (!at_max)
        {
            if (ImGui::Button("+ Add particle"))
            {
                uniform_random_gen rng;
                inserted_particle_data p;
                p.x = rng.random_float() * 2.0f - 1.0f;
                p.y = rng.random_float() * 2.0f - 1.0f;
                p.c = 0.25f;
                p._pad = 0.0f;
                inserted_particles.push_back(p);
                active_placing_index = static_cast<int32_t>(inserted_particles.size()) - 1;
                inserted_dirty = true;
            }
        }
        else
        {
            ImGui::TextDisabled("Max %d particles reached", MAX_INSERTED_PARTICLES);
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
        sample_gl_coulomb_gas app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

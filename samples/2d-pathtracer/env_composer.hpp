#pragma once

#include "scenes.hpp"
#include "polymer-gfx-gl/gl-loaders.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// ============================================================================
// Types
// ============================================================================

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

// ============================================================================
// JSON Serialization
// ============================================================================

template<class F> inline void visit_fields(env_gradient_stop & o, F f)
{
    f("u", o.u);
    f("color", o.color);
}

inline void to_json(json & j, const env_gradient_stop & p)
{
    j = json::object();
    visit_fields(const_cast<env_gradient_stop &>(p), [&j](const char * name, auto & field, auto...) { j[name] = field; });
}

inline void from_json(const json & archive, env_gradient_stop & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto...)
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
    visit_fields(const_cast<env_lobe &>(p), [&j](const char * name, auto & field, auto...) { j[name] = field; });
}

inline void from_json(const json & archive, env_lobe & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto...)
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
    visit_fields(const_cast<env_composer &>(p), [&j](const char * name, auto & field, auto...) { j[name] = field; });
}

inline void from_json(const json & archive, env_composer & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto...)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

// ============================================================================
// UI Interaction State
// ============================================================================

struct env_composer_ui_state
{
    bool show_modal = false;
    int32_t selected_stop = -1;
    int32_t selected_lobe = -1;
    bool dragging_stop = false;
    bool dragging_lobe = false;
};

// ============================================================================
// Math Helpers
// ============================================================================

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

// ============================================================================
// Environment Evaluation
// ============================================================================

inline float3 sample_hsv_interp(const float3 & c0, const float3 & c1, float t, env_interp_mode mode)
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

inline float3 sample_gradient_ring(const std::vector<env_gradient_stop> & stops, float u, env_interp_mode mode)
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

inline float3 sample_lobes(const std::vector<env_lobe> & lobes, float u)
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

inline float3 eval_environment(const env_composer & env, float u)
{
    float3 grad = sample_gradient_ring(env.stops, wrap01(u), env.interpolation);
    float3 lobe = sample_lobes(env.lobes, wrap01(u));
    return grad + lobe;
}

// ============================================================================
// OpenGL Texture Management
// ============================================================================

inline void setup_environment_texture(env_composer & env, GLuint & texture_id)
{
    int32_t resolution = std::max(env.resolution, 64);
    if (env.resolution != resolution) env.resolution = resolution;

    if (texture_id != 0) glDeleteTextures(1, &texture_id);
    glCreateTextures(GL_TEXTURE_1D, 1, &texture_id);
    glTextureStorage1D(texture_id, 1, GL_RGB32F, resolution);
    glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

inline void bake_environment_texture(env_composer & env, GLuint & texture_id, std::vector<float3> & env_baked, bool & env_dirty)
{
    env.resolution = std::max(env.resolution, 64);
    if (texture_id == 0) setup_environment_texture(env, texture_id);

    env_baked.resize(static_cast<size_t>(env.resolution));
    std::vector<float> upload(static_cast<size_t>(env.resolution) * 3);

    for (int32_t i = 0; i < env.resolution; ++i)
    {
        float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(env.resolution);
        float3 c = eval_environment(env, u) * env.gain;
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

    glTextureSubImage1D(texture_id, 0, 0, env.resolution, GL_RGB, GL_FLOAT, upload.data());
    env_dirty = false;
}

// ============================================================================
// Presets
// ============================================================================

inline void apply_environment_preset(env_composer & env, env_composer_ui_state & ui, int preset_id)
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

    ui.selected_stop = env.stops.empty() ? -1 : 0;
    ui.selected_lobe = env.lobes.empty() ? -1 : 0;
}

// ============================================================================
// ImGui Modal
// ============================================================================

// Returns true if the caller should reset the path tracer accumulation buffer.
inline bool draw_environment_composer_modal(env_composer & env, env_composer_ui_state & ui, std::vector<float3> & env_baked, GLuint & environment_texture_1d, bool & env_dirty)
{
    if (ui.show_modal)
    {
        ImGui::OpenPopup("Environment Composer");
        ui.show_modal = false;
    }

    bool open = true;
    if (!ImGui::BeginPopupModal("Environment Composer", &open, ImGuiWindowFlags_AlwaysAutoResize))
        return false;

    bool changed = false;

    changed |= ImGui::Checkbox("Enable Environment Map", &env.enabled);
    changed |= ImGui::DragFloat("Composer Gain", &env.gain, 0.01f, 0.0f, 50.0f, "%.3f");

    int32_t new_resolution = env.resolution;
    if (ImGui::SliderInt("Resolution", &new_resolution, 128, 4096))
    {
        if (new_resolution != env.resolution)
        {
            env.resolution = new_resolution;
            setup_environment_texture(env, environment_texture_1d);
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
    if (ImGui::Button("Single Hard Sun")) { apply_environment_preset(env, ui, 0); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Dual Key")) { apply_environment_preset(env, ui, 1); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Gradient Sky")) { apply_environment_preset(env, ui, 2); changed = true; }
    if (ImGui::Button("Neon Arc")) { apply_environment_preset(env, ui, 3); changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Striped Angular")) { apply_environment_preset(env, ui, 4); changed = true; }

    if (env_dirty) bake_environment_texture(env, environment_texture_1d, env_baked, env_dirty);

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
        if (i == ui.selected_stop) dl->AddCircle(ImVec2(x, stop_pos.y + 21.0f), 6.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
    }

    if (stop_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        float u = marker_to_u(ImGui::GetIO().MousePos.x, stop_pos.x, preview_w);
        env.stops.push_back({u, eval_environment(env, u)});
        ui.selected_stop = static_cast<int>(env.stops.size()) - 1;
        changed = true;
    }
    if (stop_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ui.dragging_stop = false;
        float best_px = 10.0f;
        int best = -1;
        for (int i = 0; i < static_cast<int>(env.stops.size()); ++i)
        {
            float x = stop_pos.x + env.stops[i].u * preview_w;
            float d = std::abs(ImGui::GetIO().MousePos.x - x);
            if (d < best_px) { best_px = d; best = i; }
        }
        ui.selected_stop = best;
        ui.dragging_stop = (best >= 0);
    }
    if (ui.dragging_stop)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ui.selected_stop >= 0)
        {
            env.stops[ui.selected_stop].u = marker_to_u(ImGui::GetIO().MousePos.x, stop_pos.x, preview_w);
            changed = true;
        }
        else
        {
            ui.dragging_stop = false;
        }
    }
    if (ImGui::Button("Add Stop"))
    {
        env.stops.push_back({0.5f, {1.0f, 1.0f, 1.0f}});
        ui.selected_stop = static_cast<int>(env.stops.size()) - 1;
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
        if (i == ui.selected_lobe) dl->AddCircle(ImVec2(x, lobe_pos.y + 22.0f), 8.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
    }

    if (lobe_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        float u = marker_to_u(ImGui::GetIO().MousePos.x, lobe_pos.x, preview_w);
        env.lobes.push_back({u, 0.08f, 8.0f, 0.75f, {1.0f, 1.0f, 1.0f}});
        ui.selected_lobe = static_cast<int>(env.lobes.size()) - 1;
        changed = true;
    }
    if (lobe_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ui.dragging_lobe = false;
        float best_px = 10.0f;
        int best = -1;
        for (int i = 0; i < static_cast<int>(env.lobes.size()); ++i)
        {
            float x = lobe_pos.x + env.lobes[i].u * preview_w;
            float d = std::abs(ImGui::GetIO().MousePos.x - x);
            if (d < best_px) { best_px = d; best = i; }
        }
        ui.selected_lobe = best;
        ui.dragging_lobe = (best >= 0);
    }
    if (ui.dragging_lobe)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ui.selected_lobe >= 0)
        {
            env.lobes[ui.selected_lobe].u = marker_to_u(ImGui::GetIO().MousePos.x, lobe_pos.x, preview_w);
            changed = true;
        }
        else
        {
            ui.dragging_lobe = false;
        }
    }
    if (ImGui::Button("Add Lobe"))
    {
        env.lobes.push_back({0.5f, 0.08f, 6.0f, 0.75f, {1.0f, 1.0f, 1.0f}});
        ui.selected_lobe = static_cast<int>(env.lobes.size()) - 1;
        changed = true;
    }

    ImGui::Separator();
    ImGuiColorEditFlags color_flags = ImGuiColorEditFlags_Float;
    if (env.interpolation != env_interp_mode::rgb_linear)
    {
        color_flags |= ImGuiColorEditFlags_InputHSV | ImGuiColorEditFlags_DisplayHSV;
    }

    if (ui.selected_stop >= 0 && ui.selected_stop < static_cast<int>(env.stops.size()))
    {
        env_gradient_stop & s = env.stops[ui.selected_stop];
        changed |= ImGui::SliderFloat("Stop U", &s.u, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit3("Stop Color", &s.color.x, color_flags);
        if (ImGui::Button("Delete Stop"))
        {
            env.stops.erase(env.stops.begin() + ui.selected_stop);
            ui.selected_stop = std::min(ui.selected_stop, static_cast<int>(env.stops.size()) - 1);
            changed = true;
        }
    }

    if (ui.selected_lobe >= 0 && ui.selected_lobe < static_cast<int>(env.lobes.size()))
    {
        env_lobe & l = env.lobes[ui.selected_lobe];
        changed |= ImGui::SliderFloat("Lobe U", &l.u, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Width", &l.width, 0.001f, 0.5f, "%.4f");
        changed |= ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 80.0f, "%.2f");
        changed |= ImGui::SliderFloat("Falloff", &l.falloff, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::ColorEdit3("Lobe Color", &l.color.x, color_flags);
        if (ImGui::Button("Delete Lobe"))
        {
            env.lobes.erase(env.lobes.begin() + ui.selected_lobe);
            ui.selected_lobe = std::min(ui.selected_lobe, static_cast<int>(env.lobes.size()) - 1);
            changed = true;
        }
    }

    if (changed) env_dirty = true;

    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return changed;
}

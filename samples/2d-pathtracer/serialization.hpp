#pragma once

#include "scenes.hpp"
#include "env_composer.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

// ============================================================================
// Camera
// ============================================================================

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

// ============================================================================
// Scene Archive
// ============================================================================

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

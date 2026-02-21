#pragma once

#include "polymer-core/lib-polymer.hpp"
#include "nlohmann/json.hpp"

#include <cmath>
#include <string>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <filesystem>

using namespace polymer;
using json = nlohmann::json;

enum class prim_type : uint32_t { circle = 0, box = 1, capsule = 2, segment = 3, lens = 4, ngon = 5, image_sdf = 6 };
enum class material_type : uint32_t { diffuse = 0, mirror = 1, glass = 2, water = 3, diamond = 4 };
enum class visibility_mode : uint32_t
{
    normal = 0,
    primary_holdout = 1,
    primary_no_direct = 2
};

inline void to_json(json & archive, const prim_type & p) { archive = static_cast<uint32_t>(p); }
inline void from_json(const json & archive, prim_type & p) { p = static_cast<prim_type>(archive.get<uint32_t>()); }

inline void to_json(json & archive, const material_type & p) { archive = static_cast<uint32_t>(p); }
inline void from_json(const json & archive, material_type & p) { p = static_cast<material_type>(archive.get<uint32_t>()); }

inline void to_json(json & archive, const visibility_mode & p) { archive = static_cast<uint32_t>(p); }
inline void from_json(const json & archive, visibility_mode & p) { p = static_cast<visibility_mode>(archive.get<uint32_t>()); }

inline float2 rotate_2d(const float2 & p, const float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return {c * p.x + s * p.y, -s * p.x + c * p.y};
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
        catch (const std::filesystem::filesystem_error &)
        {
            continue;
        }
    }

    return {};
}


//////////////////////////////
//   camera_controller_2d   //
//////////////////////////////

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

// layout std430
struct gpu_sdf_primitive
{
    float2 position = {0.0f, 0.0f};
    float rotation = 0.0f;
    uint32_t prim = 0;
    float4 params = {1.0f, 0.0f, 0.0f, 0.0f};
    uint32_t material = 0;
    float ior_base = 1.5f;
    float cauchy_b = 0.0f;
    float cauchy_c = 0.0f;
    float3 albedo = {1.0f, 1.0f, 1.0f};
    float emission = 0.0f;
    float3 absorption = {0.0f, 0.0f, 0.0f};
    float emission_half_angle = POLYMER_PI;
};
static_assert(sizeof(gpu_sdf_primitive) == 80, "gpu_sdf_primitive must be 80 bytes");

constexpr uint32_t gpu_material_mask = 0xFFu;
constexpr uint32_t gpu_visibility_shift = 8u;
constexpr uint32_t gpu_image_invert_bit = 1u << 16u;

/////////////////////////
//   scene_primitive   //
/////////////////////////

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
    float3 absorption           = {0.0f, 0.0f, 0.0f};
    float emission_half_angle   = POLYMER_PI;
    visibility_mode visibility  = visibility_mode::normal;
    bool invert_image           = false;
    bool selected               = false;

    gpu_sdf_primitive pack() const
    {
        gpu_sdf_primitive g;
        g.position   = position;
        g.rotation   = rotation;
        g.prim       = static_cast<uint32_t>(type);
        g.params     = params;
        const uint32_t packed_material = static_cast<uint32_t>(mat) & gpu_material_mask;
        const uint32_t packed_visibility = static_cast<uint32_t>(visibility) << gpu_visibility_shift;
        g.material   = packed_material | packed_visibility | (invert_image ? gpu_image_invert_bit : 0u);
        g.ior_base   = ior_base;
        g.cauchy_b   = cauchy_b;
        g.cauchy_c   = cauchy_c;
        g.albedo     = albedo;
        g.emission   = emission;
        g.absorption = absorption;
        g.emission_half_angle = emission_half_angle;
        return g;
    }
};

template<class F> inline void visit_fields(scene_primitive & o, F f)
{
    f("type", o.type);
    f("material", o.mat);
    f("position", o.position);
    f("rotation", o.rotation);
    f("params", o.params);
    f("albedo", o.albedo);
    f("emission", o.emission);
    f("ior_base", o.ior_base);
    f("cauchy_b", o.cauchy_b);
    f("cauchy_c", o.cauchy_c);
    f("absorption", o.absorption);
    f("emission_half_angle", o.emission_half_angle);
    f("visibility", o.visibility);
    f("invert_image", o.invert_image);
}

inline void to_json(json & j, const scene_primitive & p)
{
    j = json::object();
    visit_fields(const_cast<scene_primitive &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, scene_primitive & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

///////////////////////////
//  path_tracer_config   //
///////////////////////////

struct path_tracer_config
{
    int32_t max_bounces = 64;
    int32_t samples_per_frame = 1;
    float environment_intensity = 0.000f;
    float firefly_clamp = 32.0f;
    float exposure = 0.25;
    bool debug_overlay = false;
};

template<class F> inline void visit_fields(path_tracer_config & o, F f)
{
    f("max_bounces", o.max_bounces);
    f("samples_per_frame", o.samples_per_frame);
    f("environment_intensity", o.environment_intensity);
    f("firefly_clamp", o.firefly_clamp);
    f("exposure", o.exposure);
    f("debug_overlay", o.debug_overlay);
}

inline void to_json(json & j, const path_tracer_config & p)
{
    j = json::object();
    visit_fields(const_cast<path_tracer_config &>(p), [&j](const char * name, auto & field, auto... metadata) { j[name] = field; });
}

inline void from_json(const json & archive, path_tracer_config & m)
{
    const json obj = archive;
    visit_fields(m, [&obj](const char * name, auto & field, auto... metadata)
    {
        if (obj.contains(name)) field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

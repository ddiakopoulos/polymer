#pragma once

#include "polymer-core/lib-polymer.hpp"
#include "nlohmann/json.hpp"

#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

using namespace polymer;

enum class prim_type : uint32_t { circle = 0, box = 1, capsule = 2, segment = 3, lens = 4, ngon = 5 };
enum class material_type : uint32_t { diffuse = 0, mirror = 1, glass = 2, water = 3, diamond = 4 };
using json = nlohmann::json;

inline json normalize_json_format(const json & archive)
{
    if (archive.is_array())
    {
        json obj = json::object();
        for (const auto & pair : archive)
        {
            if (pair.is_array() && pair.size() == 2 && pair[0].is_string())
            {
                obj[pair[0].get<std::string>()] = pair[1];
            }
        }
        return obj;
    }
    return archive;
}

inline void to_json(json & archive, const prim_type & p) { archive = static_cast<uint32_t>(p); }
inline void from_json(const json & archive, prim_type & p) { p = static_cast<prim_type>(archive.get<uint32_t>()); }

inline void to_json(json & archive, const material_type & p) { archive = static_cast<uint32_t>(p); }
inline void from_json(const json & archive, material_type & p) { p = static_cast<material_type>(archive.get<uint32_t>()); }

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
    bool selected               = false;

    gpu_sdf_primitive pack() const
    {
        gpu_sdf_primitive g;
        g.position   = position;
        g.rotation   = rotation;
        g.prim       = static_cast<uint32_t>(type);
        g.params     = params;
        g.material   = static_cast<uint32_t>(mat);
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
        field = obj.at(name).get<std::remove_reference_t<decltype(field)>>();
    });
}

struct path_tracer_config
{
    int32_t max_bounces         = 64;
    int32_t samples_per_frame   = 1;
    float environment_intensity = 0.000f;
    float firefly_clamp         = 32.0f;
    float exposure              = 0.25;
    bool debug_overlay          = false;
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
struct scene_preset
{
    const char * name;
    std::vector<scene_primitive> (*build)();
};

inline std::vector<scene_primitive> scene_cornell_box()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::circle;
    light.mat = material_type::diffuse;
    light.position = {0.0f, 2.3f};
    light.params = {0.4f, 0.0f, 0.0f, 0.0f};
    light.albedo = {1.0f, 0.95f, 0.9f};
    light.emission = 15.0f;
    s.push_back(light);

    scene_primitive floor;
    floor.type = prim_type::box;
    floor.mat = material_type::diffuse;
    floor.position = {0.0f, -3.0f};
    floor.params = {3.3f, 0.3f, 0.0f, 0.0f};
    floor.albedo = {0.8f, 0.8f, 0.8f};
    s.push_back(floor);

    scene_primitive left_wall;
    left_wall.type = prim_type::box;
    left_wall.mat = material_type::diffuse;
    left_wall.position = {-3.0f, 0.0f};
    left_wall.params = {0.3f, 3.3f, 0.0f, 0.0f};
    left_wall.albedo = {0.8f, 0.2f, 0.2f};
    s.push_back(left_wall);

    scene_primitive right_wall;
    right_wall.type = prim_type::box;
    right_wall.mat = material_type::diffuse;
    right_wall.position = {3.0f, 0.0f};
    right_wall.params = {0.3f, 3.3f, 0.0f, 0.0f};
    right_wall.albedo = {0.2f, 0.8f, 0.2f};
    s.push_back(right_wall);

    scene_primitive ceiling;
    ceiling.type = prim_type::box;
    ceiling.mat = material_type::diffuse;
    ceiling.position = {0.0f, 3.0f};
    ceiling.params = {3.3f, 0.3f, 0.0f, 0.0f};
    ceiling.albedo = {0.8f, 0.8f, 0.8f};
    s.push_back(ceiling);

    scene_primitive sphere;
    sphere.type = prim_type::circle;
    sphere.mat = material_type::glass;
    sphere.position = {0.0f, -1.5f};
    sphere.params = {0.7f, 0.0f, 0.0f, 0.0f};
    sphere.albedo = {1.0f, 1.0f, 1.0f};
    sphere.ior_base = 1.5f;
    sphere.cauchy_b = 0.004f;
    sphere.cauchy_c = 0.0f;
    s.push_back(sphere);

    return s;
}

inline std::vector<scene_primitive> scene_prism()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::box;
    light.mat = material_type::diffuse;
    light.position = {-3.0f, 0.0f};
    light.params = {0.1f, 1.5f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 20.0f;
    light.emission_half_angle = static_cast<float>(POLYMER_PI) * 0.5f;
    s.push_back(light);

    scene_primitive prism;
    prism.type = prim_type::ngon;
    prism.mat = material_type::glass;
    prism.position = {0.0f, 0.0f};
    prism.params = {1.0f, 3.0f, 0.0f, 0.0f};
    prism.albedo = {1.0f, 1.0f, 1.0f};
    prism.ior_base = 1.5f;
    prism.cauchy_b = 0.01f;
    s.push_back(prism);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {4.0f, 0.0f};
    screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    return s;
}

inline std::vector<scene_primitive> scene_converging_lens()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::box;
    light.mat = material_type::diffuse;
    light.position = {-4.0f, 0.0f};
    light.params = {0.1f, 2.0f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 20.0f;
    light.emission_half_angle = static_cast<float>(POLYMER_PI) * 0.5f;
    s.push_back(light);

    scene_primitive lens;
    lens.type = prim_type::lens;
    lens.mat = material_type::glass;
    lens.position = {0.0f, 0.0f};
    lens.params = {2.0f, 2.0f, 1.5f, 0.0f};
    lens.albedo = {1.0f, 1.0f, 1.0f};
    lens.ior_base = 1.5f;
    lens.cauchy_b = 0.004f;
    s.push_back(lens);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {4.0f, 0.0f};
    screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    return s;
}

inline std::vector<scene_primitive> scene_diamond()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::circle;
    light.mat = material_type::diffuse;
    light.position = {0.0f, 3.0f};
    light.params = {0.5f, 0.0f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 25.0f;
    s.push_back(light);

    scene_primitive diamond;
    diamond.type = prim_type::ngon;
    diamond.mat = material_type::diamond;
    diamond.position = {0.0f, 0.0f};
    diamond.params = {1.0f, 8.0f, 0.0f, 0.0f};
    diamond.albedo = {1.0f, 1.0f, 1.0f};
    diamond.ior_base = 2.42f;
    diamond.cauchy_b = 0.044f;
    diamond.cauchy_c = 0.001f;
    s.push_back(diamond);

    scene_primitive floor;
    floor.type = prim_type::box;
    floor.mat = material_type::diffuse;
    floor.position = {0.0f, -2.0f};
    floor.params = {5.0f, 0.3f, 0.0f, 0.0f};
    floor.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(floor);

    return s;
}

inline std::vector<scene_primitive> scene_telescope()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::box;
    light.mat = material_type::diffuse;
    light.position = {-6.0f, 0.0f};
    light.params = {0.1f, 2.0f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 20.0f;
    s.push_back(light);

    scene_primitive objective;
    objective.type = prim_type::lens;
    objective.mat = material_type::glass;
    objective.position = {-2.0f, 0.0f};
    objective.params = {2.5f, 2.5f, 1.8f, 0.0f};
    objective.albedo = {1.0f, 1.0f, 1.0f};
    objective.ior_base = 1.5f;
    objective.cauchy_b = 0.004f;
    s.push_back(objective);

    scene_primitive eyepiece;
    eyepiece.type = prim_type::lens;
    eyepiece.mat = material_type::glass;
    eyepiece.position = {3.0f, 0.0f};
    eyepiece.params = {1.2f, 1.2f, 0.8f, 0.0f};
    eyepiece.albedo = {1.0f, 1.0f, 1.0f};
    eyepiece.ior_base = 1.5f;
    eyepiece.cauchy_b = 0.004f;
    s.push_back(eyepiece);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {6.0f, 0.0f};
    screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    return s;
}

inline std::vector<scene_primitive> scene_achromatic_doublet()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::box;
    light.mat = material_type::diffuse;
    light.position = {-5.0f, 0.0f};
    light.params = {0.1f, 2.0f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 20.0f;
    s.push_back(light);

    scene_primitive crown;
    crown.type = prim_type::lens;
    crown.mat = material_type::glass;
    crown.position = {-0.15f, 0.0f};
    crown.params = {2.0f, 2.0f, 1.2f, 0.0f};
    crown.albedo = {1.0f, 1.0f, 1.0f};
    crown.ior_base = 1.52f;
    crown.cauchy_b = 0.004f;
    s.push_back(crown);

    scene_primitive flint;
    flint.type = prim_type::lens;
    flint.mat = material_type::glass;
    flint.position = {0.55f, 0.0f};
    flint.params = {2.0f, 3.0f, 1.2f, 0.0f};
    flint.albedo = {1.0f, 1.0f, 1.0f};
    flint.ior_base = 1.62f;
    flint.cauchy_b = 0.012f;
    s.push_back(flint);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {5.0f, 0.0f};
    screen.params = {0.1f, 3.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    return s;
}

inline std::vector<scene_primitive> scene_laser_mirrors()
{
    std::vector<scene_primitive> s;

    scene_primitive laser;
    laser.type = prim_type::circle;
    laser.mat = material_type::diffuse;
    laser.position = {-4.0f, -1.0f};
    laser.rotation = 0.0f;
    laser.params = {0.15f, 0.0f, 0.0f, 0.0f};
    laser.albedo = {1.0f, 0.1f, 0.1f};
    laser.emission = 50.0f;
    laser.emission_half_angle = 0.12f;
    s.push_back(laser);

    scene_primitive m1;
    m1.type = prim_type::box;
    m1.mat = material_type::mirror;
    m1.position = {3.0f, -1.0f};
    m1.rotation = static_cast<float>(POLYMER_PI) * 0.25f;
    m1.params = {0.1f, 1.2f, 0.0f, 0.0f};
    m1.albedo = {0.95f, 0.95f, 0.95f};
    s.push_back(m1);

    scene_primitive m2;
    m2.type = prim_type::box;
    m2.mat = material_type::mirror;
    m2.position = {3.0f, 2.5f};
    m2.rotation = -static_cast<float>(POLYMER_PI) * 0.25f;
    m2.params = {0.1f, 1.2f, 0.0f, 0.0f};
    m2.albedo = {0.95f, 0.95f, 0.95f};
    s.push_back(m2);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {-4.0f, 2.5f};
    screen.params = {0.1f, 2.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    return s;
}

inline std::vector<scene_primitive> scene_nested_media_stack()
{
    std::vector<scene_primitive> s;

    scene_primitive light;
    light.type = prim_type::box;
    light.mat = material_type::diffuse;
    light.position = {-5.5f, 0.0f};
    light.params = {0.1f, 1.8f, 0.0f, 0.0f};
    light.albedo = {1.0f, 1.0f, 1.0f};
    light.emission = 24.0f;
    light.emission_half_angle = static_cast<float>(POLYMER_PI) * 0.45f;
    s.push_back(light);

    scene_primitive outer_water;
    outer_water.type = prim_type::circle;
    outer_water.mat = material_type::water;
    outer_water.position = {0.0f, 0.0f};
    outer_water.params = {1.85f, 0.0f, 0.0f, 0.0f};
    outer_water.albedo = {1.0f, 1.0f, 1.0f};
    outer_water.ior_base = 1.333f;
    outer_water.cauchy_b = 0.003f;
    outer_water.cauchy_c = 0.0f;
    outer_water.absorption = {0.10f, 0.03f, 0.01f};
    s.push_back(outer_water);

    scene_primitive inner_glass;
    inner_glass.type = prim_type::circle;
    inner_glass.mat = material_type::glass;
    inner_glass.position = {0.0f, 0.0f};
    inner_glass.params = {0.95f, 0.0f, 0.0f, 0.0f};
    inner_glass.albedo = {1.0f, 1.0f, 1.0f};
    inner_glass.ior_base = 1.52f;
    inner_glass.cauchy_b = 0.006f;
    inner_glass.cauchy_c = 0.0f;
    inner_glass.absorption = {0.0f, 0.0f, 0.0f};
    s.push_back(inner_glass);

    scene_primitive screen;
    screen.type = prim_type::box;
    screen.mat = material_type::diffuse;
    screen.position = {5.5f, 0.0f};
    screen.params = {0.12f, 3.0f, 0.0f, 0.0f};
    screen.albedo = {0.9f, 0.9f, 0.9f};
    s.push_back(screen);

    scene_primitive floor;
    floor.type = prim_type::box;
    floor.mat = material_type::diffuse;
    floor.position = {0.0f, -3.2f};
    floor.params = {6.0f, 0.25f, 0.0f, 0.0f};
    floor.albedo = {0.85f, 0.85f, 0.85f};
    s.push_back(floor);

    return s;
}

inline std::vector<scene_preset> get_scene_presets()
{
    return
    {
        {"cornell", scene_cornell_box},
        {"prism", scene_prism},
        {"converging lens", scene_converging_lens},
        {"diamond", scene_diamond},
        {"telescope", scene_telescope},
        {"achromatic doublet", scene_achromatic_doublet},
        {"lasers (pew pew)", scene_laser_mirrors},
        {"nested media", scene_nested_media_stack},
    };
}

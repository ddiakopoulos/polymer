#pragma once

#include "2dpt-utils.hpp"
#include "env_composer.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

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

#pragma once

#ifndef polymer_system_utils_hpp
#define polymer_system_utils_hpp

#include "ecs/core-ecs.hpp"
#include "ecs/typeid.hpp"

#include "environment.hpp"

namespace polymer
{

    template<class F> void enumerate_components(F f)
    {
        f(get_typename<mesh_component>(), get_typeid<mesh_component>());
        f(get_typename<material_component>(), get_typeid<material_component>());
        f(get_typename<geometry_component>(), get_typeid<geometry_component>());
        f(get_typename<point_light_component>(), get_typeid<point_light_component>());
        f(get_typename<directional_light_component>(), get_typeid<directional_light_component>());
        f(get_typename<scene_graph_component>(), get_typeid<scene_graph_component>());
    }

} // end namespace polymer

#endif // end polymer_system_utils_hpp

#pragma once

#ifndef serialization_hpp
#define serialization_hpp

#include "uniforms.hpp"
#include "asset-handle-utils.hpp"
#include "material.hpp"
#include "scene.hpp"
#include "gl-procedural-sky.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

// Variadic unpacking of the metadata in the style of sgorsten. The tricky bit is the use of SFINAE with `enable_if_t.
// The compiler will keep bailing out until requested type matches the type in the parameter pack, by either of the two other templates below 
// This is loosely inspired by `findArg` found here (https://github.com/WyattTechnology/Wyatt-STM/blob/master/wstm/find_arg.h) (BSD-3),
// but using pointers instead of default-constructed objects.
template<class T, class A, class... O>
std::enable_if_t<!std::is_same<T, A>::value, const T *> unpack(const A & first, const O & ... others)
{
    // Recursively resolve piece of metadata until `others...` exhausted
    return unpack<T>(others...);
}

// Resolves the metadata when provided with a parameter pack. In the case of a single piece of metadata, this is the target.
template<class T, class... O>
const T * unpack(const T & meta, const O & ... others) { return &meta; }

// Base template to that is resolved when there's no metadata
template<class T>
const T * unpack() { return nullptr; }

template<class T> struct range_metadata { T min, max; };
struct editor_hidden { };
struct input_field { };

CEREAL_REGISTER_TYPE_WITH_NAME(material_interface, "material_interface");
CEREAL_REGISTER_TYPE_WITH_NAME(polymer_pbr_standard, "polymer_pbr_standard");
CEREAL_REGISTER_TYPE_WITH_NAME(polymer_blinn_phong_standard, "polymer_pbr_standard");
CEREAL_REGISTER_POLYMORPHIC_RELATION(material_interface, polymer_pbr_standard);
CEREAL_REGISTER_POLYMORPHIC_RELATION(material_interface, polymer_blinn_phong_standard);

template<class F> void visit_subclasses(material_interface * p, F f)
{
    f("polymer_pbr_standard", dynamic_cast<polymer_pbr_standard *>(p));
    f("polymer_blinn_phong_standard", dynamic_cast<polymer_blinn_phong_standard *>(p));
}

template<class F> void visit_fields(polymer_pbr_standard & o, F f)
{
    f("base_albedo", o.baseAlbedo);
    f("opacity", o.opacity, range_metadata<float>{ 0.f, 1.f });
    f("roughness_factor", o.roughnessFactor, range_metadata<float>{ 0.04f, 1.f });
    f("metallic_factor", o.metallicFactor, range_metadata<float>{ 0.f, 1.f });
    f("base_emissive", o.baseEmissive);
    f("emissive_strength", o.emissiveStrength, range_metadata<float>{ 0.f, 1.f });
    f("specularLevel", o.specularLevel, range_metadata<float>{ 0.f, 2.f });
    f("occulusion_strength", o.occlusionStrength, range_metadata<float>{ 0.f, 1.f });
    f("ambient_strength", o.ambientStrength, range_metadata<float>{ 0.f, 1.f });
    f("shadow_opacity", o.shadowOpacity, range_metadata<float>{ 0.f, 1.f });
    f("texcoord_scale", o.texcoordScale, range_metadata<int>{ -32, 32 });

    f("albedo_handle", o.albedo);
    f("normal_handle", o.normal);
    f("metallic_handle", o.metallic);
    f("roughness_handle", o.roughness);
    f("emissive_handle", o.emissive);
    f("height_handle", o.height);
    f("occlusion_handle", o.occlusion);

    f("program_handle", o.shader, editor_hidden{}); // hidden because shaders are tied to materials
}

template<class F> void visit_fields(polymer_blinn_phong_standard & o, F f)
{
    f("program_handle", o.shader, editor_hidden{});
}

namespace cereal
{

    // Asset Handles
    template<class Archive> void serialize(Archive & archive, texture_handle & m)  { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, gpu_mesh_handle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, cpu_mesh_handle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, material_handle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, shader_handle & m)   { archive(cereal::make_nvp("id", m.name)); }

    // Linalg Types
    template<class Archive> void serialize(Archive & archive, int2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, int3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, int4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2x2 & m) { archive(cereal::make_size_tag(2), m[0], m[1]); }
    template<class Archive> void serialize(Archive & archive, float3x3 & m) { archive(cereal::make_size_tag(3), m[0], m[1], m[2]); }
    template<class Archive> void serialize(Archive & archive, float4x4 & m) { archive(cereal::make_size_tag(4), m[0], m[1], m[2], m[3]); }

    // Polymer Types
    template<class Archive> void serialize(Archive & archive, aabb_2d & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, aabb_3d & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Pose & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }
    template<class Archive> void serialize(Archive & archive, Frustum & m) { archive(cereal::make_size_tag(6)); for (auto const & p : m.planes) archive(p); }
    template<class Archive> void serialize(Archive & archive, Ray & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Plane & m) { archive(cereal::make_nvp("equation", m.equation)); }
    template<class Archive> void serialize(Archive & archive, Line & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Segment & m) { archive(cereal::make_nvp("a", m.a), cereal::make_nvp("b", m.b)); }
    template<class Archive> void serialize(Archive & archive, Sphere & m) { archive(cereal::make_nvp("center", m.center), cereal::make_nvp("radius", m.radius)); }
}

namespace cereal
{
    template <typename T>
    void deserialize_from_json(const std::string & pathToAsset, T & e)
    {
        assert(pathToAsset.size() > 0);
        const std::string ascii = read_file_text(pathToAsset);
        std::istringstream input_stream(ascii);
        cereal::JSONInputArchive input_archive(input_stream);
        input_archive(e);
    }

    template <typename T>
    std::string serialize_to_json(T e)
    {
        std::ostringstream oss;
        {
            cereal::JSONOutputArchive json(oss);
            json(e);
        }
        return oss.str();
    }

    template <typename T>
    std::string serialize_to_json(std::shared_ptr<T> e)
    {
        assert(e != nullptr);
        std::ostringstream oss;
        {
            cereal::JSONOutputArchive json(oss);
            json(e);
        }
        return oss.str();
    }
}

///////////////////////////////////////
//   Material System Serialization   //
///////////////////////////////////////

namespace cereal
{
    template<class Archive> 
    void serialize(Archive & archive, material_interface & m)
    {
        visit_subclasses(&m, [&archive](const char * name, auto * p)
        {
            if (p)
            {
                visit_fields(*p, [&archive](const char * name, auto & field, auto... metadata)
                {
                    archive(cereal::make_nvp(name, field));
                });
            }
        });
    }
}

#endif // end serialization_hpp

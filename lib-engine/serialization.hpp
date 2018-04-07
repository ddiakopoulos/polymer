#pragma once

#ifndef serialization_hpp
#define serialization_hpp

#include "uniforms.hpp"
#include "asset-defs.hpp"
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

// There are two distinct but related bits of functionality here. The first is the `visit_fields` and `visit_subclasses` paradigm,
// reflecting class properties and defining polymorphic relationships, respectively. ImGui uses these definitions to build object
// interfaces in the scene editor as defined in `gui.hpp`. Furthermore, metadata attributes can be provided as a third argument in `visit_fields`, 
// whereby ImGui can be made aware of additional relevant properties (like hiding the field in the inspector or defining min/max ranges). The 
// second bit of functionality makes use of the C++11 Cereal library. Unfortunately, Cereal requires a further definition of polymorphic
// relationships in its own format, however JSON serialization falls through to an existing `visit_field` or `visit_subclass` template.

/////////////////////////////////////////
//   Engine Relationship Declarations  //
/////////////////////////////////////////

template<class F> void visit_subclasses(ProceduralSky * p, F f)
{
    f("hosek_model",    dynamic_cast<HosekProceduralSky *>(p));
}

template<class F> void visit_fields(HosekProceduralSky & o, F f)
{
    f("sun_position_theta",     o.sunPosition.x, range_metadata<float>{ 0.f, (float) POLYMER_PI });
    f("sun_position_phi",       o.sunPosition.y, range_metadata<float>{ 0.f, (float) POLYMER_TWO_PI });
    f("normalized_sun_y",       o.normalizedSunY, range_metadata<float>{ 0.f, (float)POLYMER_PI });
    f("albedo",                 o.albedo, range_metadata<float>{ 0.01f, 4.f});
    f("turbidity",              o.turbidity, range_metadata<float>{ 1.f, 14.f });

    o.recompute(o.turbidity, o.albedo, o.normalizedSunY);
}

template<class F> void visit_fields(Pose & o, F f) { f("position", o.position); f("orientation", o.orientation); }

template<class F> void visit_fields(GlTextureHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlShaderHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlMeshHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GeometryHandle & m, F f) { f("id", m.name); }

template<class F> void visit_subclasses(GameObject * p, F f)
{
    f("static_mesh",            dynamic_cast<StaticMesh *>(p));
    f("point_light",            dynamic_cast<PointLight *>(p));
    f("directional_light",      dynamic_cast<DirectionalLight *>(p));
}

template<class F> void visit_fields(GameObject & o, F f)
{
    f("id", o.id);
}

template<class F> void visit_fields(Renderable & o, F f)
{
    visit_fields(*dynamic_cast<GameObject *>(&o), [&](const char * name, auto & field, auto... metadata) { f(name, field); });
    f("material", o.mat);
    f("receive_shadow", o.receive_shadow);
    f("cast_shadow", o.cast_shadow);
}

template<class F> void visit_fields(StaticMesh & o, F f)
{
    visit_fields(*dynamic_cast<Renderable *>(&o), [&](const char * name, auto & field, auto... metadata) { f(name, field); });
    f("pose", o.pose);
    f("scale", o.scale);
    f("geometry_handle", o.geom);
    f("mesh_handle", o.mesh);
}

template<class F> void visit_fields(PointLight & o, F f)
{
    f("color", o.data.color);
    f("position", o.data.position);
    f("radius", o.data.radius);
}

template<class F> void visit_fields(DirectionalLight & o, F f)
{
    f("color", o.data.color);
    f("amount", o.data.amount);
    f("direction", o.data.direction);
}

template<class F> void visit_subclasses(Material * p, F f)
{
    f("MetallicRoughnessMaterial", dynamic_cast<MetallicRoughnessMaterial *>(p));
}

template<class F> void visit_fields(MetallicRoughnessMaterial & o, F f)
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
    f("texcoord_scale", o.texcoordScale, input_field{}, range_metadata<int>{ -32, 32 });

    f("albedo_handle", o.albedo);
    f("normal_handle", o.normal);
    f("metallic_handle", o.metallic);
    f("roughness_handle", o.roughness);
    f("emissive_handle", o.emissive);
    f("height_handle", o.height);
    f("occlusion_handle", o.occlusion);

    f("program_handle", o.program);
}

////////////////////////////////
//  Base Type Serialization   //
////////////////////////////////

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, int2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, int3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, int4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2x2 & m) { archive(cereal::make_size_tag(2), m[0], m[1]); }
    template<class Archive> void serialize(Archive & archive, float3x3 & m) { archive(cereal::make_size_tag(3), m[0], m[1], m[2]); }
    template<class Archive> void serialize(Archive & archive, float4x4 & m) { archive(cereal::make_size_tag(4), m[0], m[1], m[2], m[3]); }
    template<class Archive> void serialize(Archive & archive, Frustum & m) { archive(cereal::make_size_tag(6)); for (auto const & p : m.planes) archive(p); }

    template<class Archive> void serialize(Archive & archive, Pose & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }
    template<class Archive> void serialize(Archive & archive, Bounds2D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Bounds3D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Ray & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Plane & m) { archive(cereal::make_nvp("equation", m.equation)); }
    template<class Archive> void serialize(Archive & archive, Line & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Segment & m) { archive(cereal::make_nvp("a", m.a), cereal::make_nvp("b", m.b)); }
    template<class Archive> void serialize(Archive & archive, Sphere & m) { archive(cereal::make_nvp("center", m.center), cereal::make_nvp("radius", m.radius)); }
}

////////////////////////////////////
//   Asset Handle Serialization   //
////////////////////////////////////

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, GlTextureHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GlShaderHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GlMeshHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GeometryHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, MaterialHandle & m) { archive(cereal::make_nvp("id", m.name)); }
}

////////////////////////////////////////////////////
//   Engine Relationship Declarations For Cereal  //
////////////////////////////////////////////////////

CEREAL_REGISTER_TYPE_WITH_NAME(StaticMesh,                      "StaticMesh");
CEREAL_REGISTER_TYPE_WITH_NAME(PointLight,                      "PointLight");
CEREAL_REGISTER_TYPE_WITH_NAME(DirectionalLight,                "DirectionalLight");

CEREAL_REGISTER_TYPE_WITH_NAME(Material,                        "MaterialBase");
CEREAL_REGISTER_TYPE_WITH_NAME(MetallicRoughnessMaterial,       "MetallicRoughnessMaterial");

CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Renderable, StaticMesh)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Renderable, PointLight)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Renderable, DirectionalLight)

CEREAL_REGISTER_POLYMORPHIC_RELATION(Material, MetallicRoughnessMaterial);

///////////////////////////////////
//   Game Object Serialization   //
///////////////////////////////////

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

    template<class Archive> void serialize(Archive & archive, StaticMesh & m)
    {
        archive(cereal::make_nvp("renderable", cereal::base_class<Renderable>(&m)));
        archive(cereal::make_nvp("game_object", cereal::base_class<GameObject>(&m)));
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
    };

    template<class Archive> void serialize(Archive & archive, PointLight & m)
    {
        archive(cereal::make_nvp("renderable", cereal::base_class<Renderable>(&m)));
        archive(cereal::make_nvp("game_object", cereal::base_class<GameObject>(&m)));
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
    };

    template<class Archive> void serialize(Archive & archive, DirectionalLight & m)
    {
        archive(cereal::make_nvp("renderable", cereal::base_class<Renderable>(&m)));
        archive(cereal::make_nvp("game_object", cereal::base_class<GameObject>(&m)));
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
    };

    // Base classes for Cereal need to be defined, but they are no-ops since the polymorphic
    // serialization/deserialization actually happens in `visit_fields`
    template<class Archive> void serialize(Archive & archive, GameObject & m) { }
    template<class Archive> void serialize(Archive & archive, Renderable & m) { }
}

///////////////////////////////////////
//   Material System Serialization   //
///////////////////////////////////////

namespace cereal
{
    template<class Archive> 
    void serialize(Archive & archive, Material & m)
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

#pragma once

#ifndef polymer_gltf_io_hpp
#define polymer_gltf_io_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/geometry.hpp"
#include "polymer-model-io/model-io.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace polymer
{

    struct gltf_texture_info
    {
        std::string name;
        std::string uri;
        std::vector<uint8_t> embedded_data;
        std::string mime_type;
        int32_t texcoord_index = 0;
    };

    struct gltf_pbr_material
    {
        std::string name;
        float4 base_color_factor = {1.f, 1.f, 1.f, 1.f};
        float metallic_factor = 1.f;
        float roughness_factor = 1.f;
        float3 emissive_factor = {0.f, 0.f, 0.f};

        enum class alpha_mode_t : uint32_t
        {
            opaque,
            mask,
            blend
        };

        alpha_mode_t alpha_mode = alpha_mode_t::opaque;
        float alpha_cutoff = 0.5f;
        bool double_sided = false;

        int32_t base_color_texture = -1;
        int32_t metallic_roughness_texture = -1;
        int32_t normal_texture = -1;
        int32_t occlusion_texture = -1;
        int32_t emissive_texture = -1;

        float normal_scale = 1.f;
        float occlusion_strength = 1.f;
    };

    struct gltf_primitive
    {
        runtime_mesh mesh;
        int32_t material_index = -1;
    };

    struct gltf_skinned_primitive
    {
        runtime_skinned_mesh mesh;
        int32_t material_index = -1;
    };

    struct gltf_node
    {
        std::string name;
        float4x4 local_transform;
        float4x4 world_transform;
        int32_t parent_index = -1;
        std::vector<int32_t> children;
        int32_t mesh_index = -1;
        int32_t skin_index = -1;
    };

    struct gltf_scene
    {
        std::vector<gltf_primitive> primitives;
        std::vector<gltf_skinned_primitive> skinned_primitives;
        std::vector<gltf_pbr_material> materials;
        std::vector<gltf_texture_info> textures;
        std::vector<gltf_node> nodes;
        std::vector<int32_t> root_nodes;
        std::vector<skeletal_animation> animations;
        std::vector<bone> skeleton;
    };

    struct gltf_import_options
    {
        bool load_animations = true;
        bool load_materials = true;
        bool load_textures = true;
        bool compute_tangents = true;
        bool compute_normals = true;
    };

    gltf_scene import_gltf_scene(const std::string & path, const gltf_import_options & options = {});

    std::unordered_map<std::string, runtime_mesh> import_gltf_model(const std::string & path);

    bool is_gltf_file(const std::string & path);

} // end namespace polymer

#endif // end polymer_gltf_io_hpp

#pragma once

#ifndef polymer_asset_import_utils_hpp
#define polymer_asset_import_utils_hpp

#include "polymer-engine/scene.hpp"
#include "polymer-engine/material.hpp"
#include "polymer-model-io/gltf-io.hpp"

#include "stb/stb_image.h"

namespace polymer
{

    // Load an image and extract a single channel to a new single-channel texture
    // channel: 0=R, 1=G, 2=B, 3=A
    inline gl_texture_2d load_image_channel(const std::string & path, int channel, bool flip = false)
    {
        std::vector<uint8_t> binary_file = read_file_binary(path);

        if (flip) stbi_set_flip_vertically_on_load(1);
        else stbi_set_flip_vertically_on_load(0);

        int width, height, num_channels;
        uint8_t * data = stbi_load_from_memory(binary_file.data(), static_cast<int>(binary_file.size()), &width, &height, &num_channels, 0);

        if (!data)
        {
            throw std::runtime_error("Failed to load image: " + path);
        }

        if (channel >= num_channels)
        {
            stbi_image_free(data);
            throw std::runtime_error("Requested channel " + std::to_string(channel) + " but image only has " + std::to_string(num_channels) + " channels");
        }

        std::vector<uint8_t> single_channel_data(width * height);
        for (int i = 0; i < width * height; ++i)
        {
            single_channel_data[i] = data[i * num_channels + channel];
        }

        stbi_image_free(data);

        gl_texture_2d tex;
        tex.setup(width, height, GL_RED, GL_RED, GL_UNSIGNED_BYTE, single_channel_data.data(), true);
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        return tex;
    }
    // Creates a model entity with mesh, material, and geometry components
    inline entity create_model(const std::string & mesh_handle, scene & the_scene)
    {
        base_object & obj = the_scene.instantiate_mesh(
            mesh_handle,                    // name
            transform(float3(0, 0, 0)),     // pose
            float3(1.f, 1.f, 1.f),          // scale
            mesh_handle                     // mesh/geometry handle
        );
        return obj.get_entity();
    }

    // Creates a model entity with mesh, material, and geometry components using a specific material
    inline entity create_model_with_material(const std::string & mesh_handle, const std::string & material_handle, scene & the_scene)
    {
        base_object & obj = the_scene.instantiate_mesh(
            mesh_handle,
            transform(float3(0, 0, 0)),
            float3(1.f, 1.f, 1.f),
            mesh_handle,
            material_handle
        );
        return obj.get_entity();
    }

    // Imports a glTF file with full PBR material support
    inline std::vector<entity> import_gltf_asset_runtime(const std::string & filepath, scene & the_scene)
    {
        std::vector<entity> created_entities;

        const std::string name_no_ext = get_filename_without_extension(filepath);

        gltf_import_options options;
        options.load_materials = true;
        options.load_textures = true;
        options.compute_normals = true;
        options.compute_tangents = true;

        gltf_scene gltf = import_gltf_scene(filepath, options);

        std::vector<std::string> texture_handles;
        texture_handles.resize(gltf.textures.size());

        for (size_t i = 0; i < gltf.textures.size(); ++i)
        {
            const gltf_texture_info & tex_info = gltf.textures[i];
            std::string tex_handle_name = name_no_ext + "/texture_" + std::to_string(i);

            if (!tex_info.uri.empty())
            {
                std::string lower_uri = tex_info.uri;
                std::transform(lower_uri.begin(), lower_uri.end(), lower_uri.begin(), ::tolower);

                bool is_srgb = (lower_uri.find("basecolor") != std::string::npos ||
                                lower_uri.find("albedo") != std::string::npos ||
                                lower_uri.find("diffuse") != std::string::npos ||
                                lower_uri.find("emissive") != std::string::npos);

                if (is_srgb)
                {
                    create_handle_for_asset(tex_handle_name.c_str(), load_image_srgb(tex_info.uri, false));
                }
                else
                {
                    create_handle_for_asset(tex_handle_name.c_str(), load_image(tex_info.uri, false));
                }
                texture_handles[i] = tex_handle_name;
            }
        }

        std::vector<std::string> material_handles;
        material_handles.resize(gltf.materials.size());

        for (size_t i = 0; i < gltf.materials.size(); ++i)
        {
            const gltf_pbr_material & gltf_mat = gltf.materials[i];
            std::string mat_handle_name = name_no_ext + "/material_" + std::to_string(i);
            if (!gltf_mat.name.empty())
            {
                mat_handle_name = name_no_ext + "/" + gltf_mat.name;
            }

            std::shared_ptr<polymer_pbr_standard> pbr_mat = std::make_shared<polymer_pbr_standard>();

            auto & albedo_prop = pbr_mat->uniform_table["u_albedo"];
            if (auto * val = nonstd::get_if<polymer::property<float3>>(&albedo_prop))
            {
                val->set(float3(gltf_mat.base_color_factor.x, gltf_mat.base_color_factor.y, gltf_mat.base_color_factor.z));
            }

            auto & metallic_prop = pbr_mat->uniform_table["u_metallic"];
            if (auto * val = nonstd::get_if<polymer::property<float>>(&metallic_prop))
            {
                val->set(gltf_mat.metallic_factor);
            }

            auto & roughness_prop = pbr_mat->uniform_table["u_roughness"];
            if (auto * val = nonstd::get_if<polymer::property<float>>(&roughness_prop))
            {
                val->set(gltf_mat.roughness_factor);
            }

            auto & emissive_prop = pbr_mat->uniform_table["u_emissive"];
            if (auto * val = nonstd::get_if<polymer::property<float3>>(&emissive_prop))
            {
                val->set(gltf_mat.emissive_factor);
            }

            auto & occlusion_prop = pbr_mat->uniform_table["u_occlusionStrength"];
            if (auto * val = nonstd::get_if<polymer::property<float>>(&occlusion_prop))
            {
                val->set(gltf_mat.occlusion_strength);
            }

            pbr_mat->opacity.set(static_cast<float>(gltf_mat.base_color_factor.w));
            pbr_mat->double_sided.set(gltf_mat.double_sided);

            if (gltf_mat.base_color_texture >= 0 && gltf_mat.base_color_texture < static_cast<int32_t>(texture_handles.size()))
            {
                pbr_mat->albedo = texture_handle(texture_handles[gltf_mat.base_color_texture]);
            }

            if (gltf_mat.metallic_roughness_texture >= 0 && gltf_mat.metallic_roughness_texture < static_cast<int32_t>(gltf.textures.size()))
            {
                const gltf_texture_info & mr_tex = gltf.textures[gltf_mat.metallic_roughness_texture];
                if (!mr_tex.uri.empty())
                {
                    std::string roughness_handle = mat_handle_name + "_roughness";
                    std::string metallic_handle = mat_handle_name + "_metallic";

                    create_handle_for_asset(roughness_handle.c_str(), load_image_channel(mr_tex.uri, 1, false));
                    create_handle_for_asset(metallic_handle.c_str(), load_image_channel(mr_tex.uri, 2, false));

                    pbr_mat->roughness = texture_handle(roughness_handle);
                    pbr_mat->metallic = texture_handle(metallic_handle);
                }
            }

            if (gltf_mat.normal_texture >= 0 && gltf_mat.normal_texture < static_cast<int32_t>(texture_handles.size()))
            {
                pbr_mat->normal = texture_handle(texture_handles[gltf_mat.normal_texture]);
            }

            if (gltf_mat.occlusion_texture >= 0 && gltf_mat.occlusion_texture < static_cast<int32_t>(texture_handles.size()))
            {
                pbr_mat->occlusion = texture_handle(texture_handles[gltf_mat.occlusion_texture]);
            }

            if (gltf_mat.emissive_texture >= 0 && gltf_mat.emissive_texture < static_cast<int32_t>(texture_handles.size()))
            {
                pbr_mat->emissive = texture_handle(texture_handles[gltf_mat.emissive_texture]);
            }

            the_scene.mat_library->register_material(mat_handle_name, pbr_mat);
            material_handles[i] = mat_handle_name;
        }

        std::vector<entity> children;
        size_t prim_idx = 0;

        for (const gltf_primitive & prim : gltf.primitives)
        {
            runtime_mesh mesh_copy = prim.mesh;
            rescale_geometry(mesh_copy, 1.f);

            std::string mesh_handle_name = name_no_ext + "/mesh_" + std::to_string(prim_idx);

            create_handle_for_asset(mesh_handle_name.c_str(), make_mesh_from_geometry(mesh_copy));
            create_handle_for_asset(mesh_handle_name.c_str(), std::move(mesh_copy));

            std::string mat_to_use = material_library::kDefaultMaterialId;
            if (prim.material_index >= 0 && prim.material_index < static_cast<int32_t>(material_handles.size()))
            {
                mat_to_use = material_handles[prim.material_index];
            }

            entity e = create_model_with_material(mesh_handle_name, mat_to_use, the_scene);
            children.push_back(e);

            prim_idx++;
        }

        for (const gltf_skinned_primitive & prim : gltf.skinned_primitives)
        {
            runtime_mesh mesh_copy;
            mesh_copy.vertices = prim.mesh.vertices;
            mesh_copy.normals = prim.mesh.normals;
            mesh_copy.tangents = prim.mesh.tangents;
            mesh_copy.bitangents = prim.mesh.bitangents;
            mesh_copy.texcoord0 = prim.mesh.texcoord0;
            mesh_copy.texcoord1 = prim.mesh.texcoord1;
            mesh_copy.colors = prim.mesh.colors;
            mesh_copy.faces = prim.mesh.faces;
            rescale_geometry(mesh_copy, 1.f);

            std::string mesh_handle_name = name_no_ext + "/skinned_mesh_" + std::to_string(prim_idx);

            create_handle_for_asset(mesh_handle_name.c_str(), make_mesh_from_geometry(mesh_copy));
            create_handle_for_asset(mesh_handle_name.c_str(), std::move(mesh_copy));

            std::string mat_to_use = material_library::kDefaultMaterialId;
            if (prim.material_index >= 0 && prim.material_index < static_cast<int32_t>(material_handles.size()))
            {
                mat_to_use = material_handles[prim.material_index];
            }

            entity e = create_model_with_material(mesh_handle_name, mat_to_use, the_scene);
            children.push_back(e);

            prim_idx++;
        }

        if (children.size() == 1)
        {
            created_entities.push_back(children[0]);
        }
        else if (children.size() > 1)
        {
            base_object & root_obj = the_scene.instantiate_empty(
                "root/" + name_no_ext,
                transform(float3(0, 0, 0)),
                float3(1.f, 1.f, 1.f)
            );
            entity root_entity = root_obj.get_entity();
            created_entities.push_back(root_entity);

            for (entity child : children)
            {
                the_scene.get_graph().add_child(root_entity, child);
                created_entities.push_back(child);
            }
        }

        return created_entities;
    }

    // Imports assets from a file path and creates corresponding entities
    // Supports: images (png, tga, jpg) and meshes (obj, fbx, ply, mesh)
    inline std::vector<entity> import_asset_runtime(const std::string & filepath, scene & the_scene)
    {
        // Keep a list of all the entities we create as part of the import process
        std::vector<entity> created_entities;

        // Normalize paths to lowercase
        auto path = filepath;
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        const std::string ext = get_extension(path);
        const std::string name_no_ext = get_filename_without_extension(path);

        // Handle image/texture types. No entities are directly created.
        if (ext == "png" || ext == "tga" || ext == "jpg")
        {
            create_handle_for_asset(name_no_ext.c_str(), load_image(path, false));
            return {};
        }

        // Handle glTF files with full material support
        if (ext == "gltf" || ext == "glb")
        {
            return import_gltf_asset_runtime(filepath, the_scene);
        }

        // Handle mesh types
        auto imported_models = import_model(path);
        const size_t num_models = imported_models.size();
        std::vector<entity> children;

        for (auto & m : imported_models)
        {
            auto & mesh = m.second;
            rescale_geometry(mesh, 1.f);

            const std::string handle_id = name_no_ext + "/" + m.first;

            create_handle_for_asset(handle_id.c_str(), make_mesh_from_geometry(mesh));
            create_handle_for_asset(handle_id.c_str(), std::move(mesh));

            if (num_models == 1)
            {
                created_entities.push_back(create_model(handle_id, the_scene));
            }
            else
            {
                children.push_back(create_model(handle_id, the_scene));
            }
        }

        // If we have multiple meshes, create a root entity and parent the children
        if (children.size())
        {
            base_object & root_obj = the_scene.instantiate_empty(
                "root/" + name_no_ext,
                transform(float3(0, 0, 0)),
                float3(1.f, 1.f, 1.f)
            );
            const entity root_entity = root_obj.get_entity();
            created_entities.push_back(root_entity);

            for (const entity child : children)
            {
                the_scene.get_graph().add_child(root_entity, child);
                created_entities.push_back(child);
            }
        }

        return created_entities;
    }

} // end namespace polymer

#endif // end polymer_asset_import_utils_hpp

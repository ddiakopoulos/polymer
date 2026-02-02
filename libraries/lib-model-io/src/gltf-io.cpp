#include "polymer-model-io/gltf-io.hpp"
#include "polymer-core/util/file-io.hpp"
#include "polymer-core/util/string-utils.hpp"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#include <iostream>
#include <fstream>
#include <cstring>

using namespace polymer;

inline int32_t find_node_index(const cgltf_data * data, const cgltf_node * node)
{
    if (!node) return -1;
    return static_cast<int32_t>(node - data->nodes);
}

inline int32_t find_material_index(const cgltf_data * data, const cgltf_material * material)
{
    if (!material) return -1;
    return static_cast<int32_t>(material - data->materials);
}

inline int32_t find_texture_index(const cgltf_data * data, const cgltf_texture * texture)
{
    if (!texture) return -1;
    return static_cast<int32_t>(texture - data->textures);
}

inline int32_t find_skin_index(const cgltf_data * data, const cgltf_skin * skin)
{
    if (!skin) return -1;
    return static_cast<int32_t>(skin - data->skins);
}

inline int32_t find_mesh_index(const cgltf_data * data, const cgltf_mesh * mesh)
{
    if (!mesh) return -1;
    return static_cast<int32_t>(mesh - data->meshes);
}

inline std::vector<float2> extract_vec2_accessor(const cgltf_accessor * accessor)
{
    std::vector<float2> result;
    if (!accessor) return result;

    result.resize(accessor->count);
    cgltf_accessor_unpack_floats(accessor, reinterpret_cast<float *>(result.data()), accessor->count * 2);
    return result;
}

inline std::vector<float3> extract_vec3_accessor(const cgltf_accessor * accessor)
{
    std::vector<float3> result;
    if (!accessor) return result;

    result.resize(accessor->count);
    cgltf_accessor_unpack_floats(accessor, reinterpret_cast<float *>(result.data()), accessor->count * 3);
    return result;
}

inline std::vector<float4> extract_vec4_accessor(const cgltf_accessor * accessor)
{
    std::vector<float4> result;
    if (!accessor) return result;

    result.resize(accessor->count);
    cgltf_accessor_unpack_floats(accessor, reinterpret_cast<float *>(result.data()), accessor->count * 4);
    return result;
}

inline std::vector<int4> extract_ivec4_accessor(const cgltf_accessor * accessor)
{
    std::vector<int4> result;
    if (!accessor) return result;

    result.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i)
    {
        cgltf_uint out[4] = {0, 0, 0, 0};
        cgltf_accessor_read_uint(accessor, i, out, 4);
        result[i] = int4(static_cast<int>(out[0]), static_cast<int>(out[1]), static_cast<int>(out[2]), static_cast<int>(out[3]));
    }
    return result;
}

inline std::vector<uint3> extract_indices(const cgltf_accessor * accessor)
{
    std::vector<uint3> result;
    if (!accessor) return result;

    size_t num_triangles = accessor->count / 3;
    result.resize(num_triangles);

    for (size_t i = 0; i < num_triangles; ++i)
    {
        result[i].x = static_cast<uint32_t>(cgltf_accessor_read_index(accessor, i * 3 + 0));
        result[i].y = static_cast<uint32_t>(cgltf_accessor_read_index(accessor, i * 3 + 1));
        result[i].z = static_cast<uint32_t>(cgltf_accessor_read_index(accessor, i * 3 + 2));
    }

    return result;
}

inline float4x4 get_node_local_transform(const cgltf_node * node)
{
    float matrix[16];
    cgltf_node_transform_local(node, matrix);
    return float4x4(
        {matrix[0], matrix[1], matrix[2], matrix[3]},
        {matrix[4], matrix[5], matrix[6], matrix[7]},
        {matrix[8], matrix[9], matrix[10], matrix[11]},
        {matrix[12], matrix[13], matrix[14], matrix[15]}
    );
}

inline float4x4 get_node_world_transform(const cgltf_node * node)
{
    float matrix[16];
    cgltf_node_transform_world(node, matrix);
    return float4x4(
        {matrix[0], matrix[1], matrix[2], matrix[3]},
        {matrix[4], matrix[5], matrix[6], matrix[7]},
        {matrix[8], matrix[9], matrix[10], matrix[11]},
        {matrix[12], matrix[13], matrix[14], matrix[15]}
    );
}

inline gltf_texture_info gltf_load_texture_info(const cgltf_data * data, const cgltf_texture * texture, const std::string & base_path)
{
    gltf_texture_info info;

    if (!texture) return info;

    if (texture->name) info.name = texture->name;

    const cgltf_image * image = texture->image;
    if (!image) return info;

    if (image->uri)
    {
        std::string normalized_base = base_path;
        for (char & c : normalized_base) if (c == '\\') c = '/';
        size_t last_slash = normalized_base.find_last_of('/');
        std::string parent_dir = (last_slash != std::string::npos) ? normalized_base.substr(0, last_slash) : normalized_base;
        info.uri = parent_dir + "/" + std::string(image->uri);
    }
    else if (image->buffer_view)
    {
        const cgltf_buffer_view * view = image->buffer_view;
        const uint8_t * buffer_data = static_cast<const uint8_t *>(view->buffer->data);
        info.embedded_data.assign(buffer_data + view->offset, buffer_data + view->offset + view->size);
    }

    if (image->mime_type) info.mime_type = image->mime_type;

    return info;
}

inline gltf_pbr_material gltf_load_material(const cgltf_data * data, const cgltf_material * material)
{
    gltf_pbr_material mat;

    if (!material) return mat;

    if (material->name) mat.name = material->name;

    if (material->has_pbr_metallic_roughness)
    {
        const cgltf_pbr_metallic_roughness & pbr = material->pbr_metallic_roughness;

        mat.base_color_factor = float4(
            pbr.base_color_factor[0],
            pbr.base_color_factor[1],
            pbr.base_color_factor[2],
            pbr.base_color_factor[3]
        );
        mat.metallic_factor = pbr.metallic_factor;
        mat.roughness_factor = pbr.roughness_factor;

        if (pbr.base_color_texture.texture)
        {
            mat.base_color_texture = find_texture_index(data, pbr.base_color_texture.texture);
        }
        if (pbr.metallic_roughness_texture.texture)
        {
            mat.metallic_roughness_texture = find_texture_index(data, pbr.metallic_roughness_texture.texture);
        }
    }

    mat.emissive_factor = float3(
        material->emissive_factor[0],
        material->emissive_factor[1],
        material->emissive_factor[2]
    );

    if (material->normal_texture.texture)
    {
        mat.normal_texture = find_texture_index(data, material->normal_texture.texture);
        mat.normal_scale = material->normal_texture.scale;
    }

    if (material->occlusion_texture.texture)
    {
        mat.occlusion_texture = find_texture_index(data, material->occlusion_texture.texture);
        mat.occlusion_strength = material->occlusion_texture.scale;
    }

    if (material->emissive_texture.texture)
    {
        mat.emissive_texture = find_texture_index(data, material->emissive_texture.texture);
    }

    switch (material->alpha_mode)
    {
        case cgltf_alpha_mode_opaque: mat.alpha_mode = gltf_pbr_material::alpha_mode_t::opaque; break;
        case cgltf_alpha_mode_mask: mat.alpha_mode = gltf_pbr_material::alpha_mode_t::mask; break;
        case cgltf_alpha_mode_blend: mat.alpha_mode = gltf_pbr_material::alpha_mode_t::blend; break;
        default: break;
    }

    mat.alpha_cutoff = material->alpha_cutoff;
    mat.double_sided = material->double_sided;

    return mat;
}

inline runtime_mesh gltf_load_primitive(const cgltf_primitive * prim)
{
    runtime_mesh mesh;

    if (!prim) return mesh;

    if (prim->type != cgltf_primitive_type_triangles)
    {
        std::cerr << "[gltf-io] Warning: Skipping non-triangle primitive type" << std::endl;
        return mesh;
    }

    // Store tangent vec4 temporarily (w is handedness for bitangent computation)
    std::vector<float4> tangents_vec4;

    for (size_t i = 0; i < prim->attributes_count; ++i)
    {
        const cgltf_attribute * attr = &prim->attributes[i];

        switch (attr->type)
        {
            case cgltf_attribute_type_position:
                mesh.vertices = extract_vec3_accessor(attr->data);
                break;

            case cgltf_attribute_type_normal:
                mesh.normals = extract_vec3_accessor(attr->data);
                break;

            case cgltf_attribute_type_tangent:
            {
                // glTF tangents are vec4 where XYZ is the tangent direction and W is the handedness
                // Store the full vec4 so we can compute bitangents after normals are loaded
                tangents_vec4 = extract_vec4_accessor(attr->data);
                mesh.tangents.resize(tangents_vec4.size());
                for (size_t j = 0; j < tangents_vec4.size(); ++j)
                {
                    mesh.tangents[j] = float3(tangents_vec4[j].x, tangents_vec4[j].y, tangents_vec4[j].z);
                }
                break;
            }

            case cgltf_attribute_type_texcoord:
                if (attr->index == 0)
                {
                    mesh.texcoord0 = extract_vec2_accessor(attr->data);
                }
                else if (attr->index == 1)
                {
                    mesh.texcoord1 = extract_vec2_accessor(attr->data);
                }
                break;

            case cgltf_attribute_type_color:
            {
                std::vector<float4> colors_rgba = extract_vec4_accessor(attr->data);
                mesh.colors.resize(colors_rgba.size());
                for (size_t j = 0; j < colors_rgba.size(); ++j)
                {
                    mesh.colors[j] = colors_rgba[j];
                }
                break;
            }

            default:
                break;
        }
    }

    // Compute bitangents from normals and tangents (with handedness from w component)
    // Bitangent = cross(normal, tangent) * handedness
    if (!tangents_vec4.empty() && mesh.normals.size() == tangents_vec4.size())
    {
        mesh.bitangents.resize(tangents_vec4.size());
        for (size_t j = 0; j < tangents_vec4.size(); ++j)
        {
            float3 bitangent = cross(mesh.normals[j], mesh.tangents[j]) * tangents_vec4[j].w;
            mesh.bitangents[j] = bitangent;
        }
    }

    if (prim->indices)
    {
        mesh.faces = extract_indices(prim->indices);
    }

    return mesh;
}

inline runtime_skinned_mesh gltf_load_skinned_primitive(const cgltf_data * data, const cgltf_primitive * prim, const cgltf_skin * skin)
{
    runtime_skinned_mesh mesh;

    if (!prim) return mesh;

    runtime_mesh base_mesh = gltf_load_primitive(prim);
    mesh.vertices = std::move(base_mesh.vertices);
    mesh.normals = std::move(base_mesh.normals);
    mesh.tangents = std::move(base_mesh.tangents);
    mesh.bitangents = std::move(base_mesh.bitangents);
    mesh.texcoord0 = std::move(base_mesh.texcoord0);
    mesh.texcoord1 = std::move(base_mesh.texcoord1);
    mesh.colors = std::move(base_mesh.colors);
    mesh.faces = std::move(base_mesh.faces);
    mesh.material = std::move(base_mesh.material);

    for (size_t i = 0; i < prim->attributes_count; ++i)
    {
        const cgltf_attribute * attr = &prim->attributes[i];

        if (attr->type == cgltf_attribute_type_joints && attr->index == 0)
        {
            mesh.boneIndices = extract_ivec4_accessor(attr->data);
        }
        else if (attr->type == cgltf_attribute_type_weights && attr->index == 0)
        {
            mesh.boneWeights = extract_vec4_accessor(attr->data);
        }
    }

    if (skin)
    {
        mesh.bones.resize(skin->joints_count);
        for (size_t i = 0; i < skin->joints_count; ++i)
        {
            bone & b = mesh.bones[i];
            const cgltf_node * joint = skin->joints[i];

            if (joint->name) b.name = joint->name;
            b.parentIndex = 0xFFFFFFFF;

            if (joint->parent)
            {
                for (size_t j = 0; j < skin->joints_count; ++j)
                {
                    if (skin->joints[j] == joint->parent)
                    {
                        b.parentIndex = static_cast<uint32_t>(j);
                        break;
                    }
                }
            }

            b.initialPose = get_node_world_transform(joint);

            if (skin->inverse_bind_matrices)
            {
                float ibm[16];
                cgltf_accessor_read_float(skin->inverse_bind_matrices, i, ibm, 16);
                b.bindPose = float4x4(
                    {ibm[0], ibm[1], ibm[2], ibm[3]},
                    {ibm[4], ibm[5], ibm[6], ibm[7]},
                    {ibm[8], ibm[9], ibm[10], ibm[11]},
                    {ibm[12], ibm[13], ibm[14], ibm[15]}
                );
            }
        }
    }

    return mesh;
}

inline skeletal_animation gltf_load_animation(const cgltf_data * data, const cgltf_animation * anim, const cgltf_skin * skin)
{
    skeletal_animation result;

    if (!anim) return result;

    if (anim->name) result.name = anim->name;

    std::unordered_map<int32_t, std::shared_ptr<animation_track>> node_tracks;

    for (size_t c = 0; c < anim->channels_count; ++c)
    {
        const cgltf_animation_channel * channel = &anim->channels[c];
        const cgltf_animation_sampler * sampler = channel->sampler;

        if (!channel->target_node) continue;

        int32_t node_idx = find_node_index(data, channel->target_node);

        int32_t bone_idx = -1;
        if (skin)
        {
            for (size_t j = 0; j < skin->joints_count; ++j)
            {
                if (skin->joints[j] == channel->target_node)
                {
                    bone_idx = static_cast<int32_t>(j);
                    break;
                }
            }
        }
        else
        {
            bone_idx = node_idx;
        }

        if (bone_idx < 0) continue;

        if (node_tracks.find(bone_idx) == node_tracks.end())
        {
            auto track = std::make_shared<animation_track>();
            track->boneIndex = static_cast<uint32_t>(bone_idx);
            node_tracks[bone_idx] = track;
        }

        auto & track = node_tracks[bone_idx];

        std::vector<float> times;
        times.resize(sampler->input->count);
        cgltf_accessor_unpack_floats(sampler->input, times.data(), times.size());

        for (size_t t = 0; t < times.size(); ++t)
        {
            uint32_t frame_key = static_cast<uint32_t>(times[t] * 24.0f);

            if (frame_key < result.startFrame) result.startFrame = frame_key;
            if (frame_key > result.endFrame) result.endFrame = frame_key;

            std::shared_ptr<animation_keyframe> keyframe;
            for (auto & kf : track->keyframes)
            {
                if (kf->key == frame_key)
                {
                    keyframe = kf;
                    break;
                }
            }

            if (!keyframe)
            {
                keyframe = std::make_shared<animation_keyframe>();
                keyframe->key = frame_key;
                track->keyframes.push_back(keyframe);
            }

            switch (channel->target_path)
            {
                case cgltf_animation_path_type_translation:
                {
                    float val[3];
                    cgltf_accessor_read_float(sampler->output, t, val, 3);
                    keyframe->translation = float3(val[0], val[1], val[2]);
                    break;
                }
                case cgltf_animation_path_type_rotation:
                {
                    float val[4];
                    cgltf_accessor_read_float(sampler->output, t, val, 4);
                    keyframe->rotation = float4(val[0], val[1], val[2], val[3]);
                    break;
                }
                case cgltf_animation_path_type_scale:
                {
                    float val[3];
                    cgltf_accessor_read_float(sampler->output, t, val, 3);
                    keyframe->scale = float3(val[0], val[1], val[2]);
                    break;
                }
                default:
                    break;
            }
        }
    }

    for (auto & [bone_idx, track] : node_tracks)
    {
        track->keyframeCount = static_cast<uint32_t>(track->keyframes.size());
        result.tracks.push_back(track);
    }

    result.trackCount = static_cast<uint32_t>(result.tracks.size());

    return result;
}

gltf_scene polymer::import_gltf_scene(const std::string & path, const gltf_import_options & options)
{
    gltf_scene scene;

    cgltf_options cgltf_opts = {};
    cgltf_data * data = nullptr;

    cgltf_result result = cgltf_parse_file(&cgltf_opts, path.c_str(), &data);
    if (result != cgltf_result_success)
    {
        std::cerr << "[gltf-io] Error: Failed to parse glTF file: " << path << std::endl;
        return scene;
    }

    result = cgltf_load_buffers(&cgltf_opts, data, path.c_str());
    if (result != cgltf_result_success)
    {
        std::cerr << "[gltf-io] Error: Failed to load glTF buffers: " << path << std::endl;
        cgltf_free(data);
        return scene;
    }

    if (options.load_textures)
    {
        for (size_t i = 0; i < data->textures_count; ++i)
        {
            scene.textures.push_back(gltf_load_texture_info(data, &data->textures[i], path));
        }
    }

    if (options.load_materials)
    {
        for (size_t i = 0; i < data->materials_count; ++i)
        {
            scene.materials.push_back(gltf_load_material(data, &data->materials[i]));
        }
    }

    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        const cgltf_node * node = &data->nodes[i];
        gltf_node gn;

        if (node->name) gn.name = node->name;
        gn.local_transform = get_node_local_transform(node);
        gn.world_transform = get_node_world_transform(node);
        gn.parent_index = find_node_index(data, node->parent);
        gn.mesh_index = find_mesh_index(data, node->mesh);
        gn.skin_index = find_skin_index(data, node->skin);

        for (size_t c = 0; c < node->children_count; ++c)
        {
            gn.children.push_back(find_node_index(data, node->children[c]));
        }

        scene.nodes.push_back(gn);
    }

    if (data->scene)
    {
        for (size_t i = 0; i < data->scene->nodes_count; ++i)
        {
            scene.root_nodes.push_back(find_node_index(data, data->scene->nodes[i]));
        }
    }

    const cgltf_skin * primary_skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;

    for (size_t m = 0; m < data->meshes_count; ++m)
    {
        const cgltf_mesh * mesh = &data->meshes[m];
        std::string mesh_name = mesh->name ? mesh->name : ("mesh_" + std::to_string(m));

        for (size_t p = 0; p < mesh->primitives_count; ++p)
        {
            const cgltf_primitive * prim = &mesh->primitives[p];

            bool has_skinning = false;
            for (size_t a = 0; a < prim->attributes_count; ++a)
            {
                if (prim->attributes[a].type == cgltf_attribute_type_joints ||
                    prim->attributes[a].type == cgltf_attribute_type_weights)
                {
                    has_skinning = true;
                    break;
                }
            }

            if (has_skinning && primary_skin)
            {
                gltf_skinned_primitive skinned_prim;
                skinned_prim.mesh = gltf_load_skinned_primitive(data, prim, primary_skin);
                skinned_prim.material_index = find_material_index(data, prim->material);

                if (options.compute_normals && skinned_prim.mesh.normals.empty())
                {
                    compute_normals(skinned_prim.mesh);
                }
                if (options.compute_tangents && skinned_prim.mesh.tangents.empty())
                {
                    compute_tangents(skinned_prim.mesh);
                }

                scene.skinned_primitives.push_back(std::move(skinned_prim));
            }
            else
            {
                gltf_primitive gp;
                gp.mesh = gltf_load_primitive(prim);
                gp.material_index = find_material_index(data, prim->material);

                if (options.compute_normals && gp.mesh.normals.empty())
                {
                    compute_normals(gp.mesh);
                }
                if (options.compute_tangents && gp.mesh.tangents.empty())
                {
                    compute_tangents(gp.mesh);
                }

                scene.primitives.push_back(std::move(gp));
            }
        }
    }

    if (primary_skin)
    {
        for (size_t i = 0; i < primary_skin->joints_count; ++i)
        {
            bone b;
            const cgltf_node * joint = primary_skin->joints[i];
            if (joint->name) b.name = joint->name;

            b.parentIndex = 0xFFFFFFFF;
            if (joint->parent)
            {
                for (size_t j = 0; j < primary_skin->joints_count; ++j)
                {
                    if (primary_skin->joints[j] == joint->parent)
                    {
                        b.parentIndex = static_cast<uint32_t>(j);
                        break;
                    }
                }
            }

            b.initialPose = get_node_world_transform(joint);

            if (primary_skin->inverse_bind_matrices)
            {
                float ibm[16];
                cgltf_accessor_read_float(primary_skin->inverse_bind_matrices, i, ibm, 16);
                b.bindPose = float4x4(
                    {ibm[0], ibm[1], ibm[2], ibm[3]},
                    {ibm[4], ibm[5], ibm[6], ibm[7]},
                    {ibm[8], ibm[9], ibm[10], ibm[11]},
                    {ibm[12], ibm[13], ibm[14], ibm[15]}
                );
            }

            scene.skeleton.push_back(b);
        }
    }

    if (options.load_animations)
    {
        for (size_t i = 0; i < data->animations_count; ++i)
        {
            skeletal_animation anim = gltf_load_animation(data, &data->animations[i], primary_skin);
            if (anim.tracks.size() > 0)
            {
                scene.animations.push_back(std::move(anim));
            }
        }
    }

    cgltf_free(data);

    return scene;
}

std::unordered_map<std::string, runtime_mesh> polymer::import_gltf_model(const std::string & path)
{
    std::unordered_map<std::string, runtime_mesh> result;

    cgltf_options cgltf_opts = {};
    cgltf_data * data = nullptr;

    cgltf_result cgltf_res = cgltf_parse_file(&cgltf_opts, path.c_str(), &data);
    if (cgltf_res != cgltf_result_success)
    {
        std::cerr << "[gltf-io] Error: Failed to parse glTF file: " << path << std::endl;
        return result;
    }

    cgltf_res = cgltf_load_buffers(&cgltf_opts, data, path.c_str());
    if (cgltf_res != cgltf_result_success)
    {
        std::cerr << "[gltf-io] Error: Failed to load glTF buffers: " << path << std::endl;
        cgltf_free(data);
        return result;
    }

    for (size_t m = 0; m < data->meshes_count; ++m)
    {
        const cgltf_mesh * mesh = &data->meshes[m];
        std::string mesh_name = mesh->name ? mesh->name : ("mesh_" + std::to_string(m));

        for (size_t p = 0; p < mesh->primitives_count; ++p)
        {
            const cgltf_primitive * prim = &mesh->primitives[p];
            runtime_mesh loaded_mesh = gltf_load_primitive(prim);

            if (loaded_mesh.normals.empty() && !loaded_mesh.vertices.empty())
            {
                compute_normals(loaded_mesh);
            }

            std::string prim_name = mesh_name;
            if (mesh->primitives_count > 1)
            {
                prim_name += "_prim" + std::to_string(p);
            }

            result[prim_name] = std::move(loaded_mesh);
        }
    }

    cgltf_free(data);

    return result;
}

bool polymer::is_gltf_file(const std::string & path)
{
    std::string ext = get_extension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "gltf" || ext == "glb";
}

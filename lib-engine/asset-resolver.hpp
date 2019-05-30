/*
 * File: lib-engine/asset-resolver.hpp
 * This file provies a resolution mechanism for asset handles to be associated and loaded with their
 * underlying resource, from either memory or disk. Handles are serialized by a variety of containers,
 * including `environment`, `material_library`, and `shader_library`. During deserialization, these
 * handles are not assocated with any actual resource. This class compares handles in the containers
 * to assigned assets in the `asset_handle<T>` table. If an unassigned resource is found, the asset
 * handle identifier is used as a key to recursively search an asset folder for a matching filename
 * where the asset is loaded.
 *
 * (todo) Presently we assume that all handle identifiers refer to unique assets, however this is a weak
 * assumption and is likely untrue in practice and should be fixed.
 *
 * (todo) The asset_resolver is single-threaded and called on the main thread because it may also
 * touch GPU resources. This must be changed to load asynchronously. 
 */

#pragma once

#ifndef polymer_asset_resolver_hpp
#define polymer_asset_resolver_hpp

#include "asset-handle.hpp"
#include "asset-handle-utils.hpp"
#include "string_utils.hpp"
#include "renderer-pbr.hpp"
#include "system-collision.hpp"
#include "environment.hpp"
#include "system-render.hpp"

#include "../lib-model-io/model-io.hpp"
#include "json.hpp"

namespace polymer
{

    template <typename T>
    inline void remove_duplicates(std::vector<T> & vec)
    {
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }

    // Polymer asset handles for meshes are of the form `root_name/sub_name`
    // This function returns `root_file_name` 
    inline std::string find_root(const std::string & name)
    {
        auto result = split(name, '/');
        if (!result.empty()) return result[0];
        else return name;
    }

    // The purpose of an asset resolver is to match an asset_handle to an asset on disk. This is done
    // for scene objects (meshes, geometry) and materials (shaders, textures, cubemaps). 
    class asset_resolver
    {
        environment * scene;
        material_library * library;

        // Unresolved asset names
        std::vector<std::string> mesh_names;
        std::vector<std::string> shader_names;
        std::vector<std::string> material_names;
        std::vector<std::string> texture_names;

        std::vector<std::string> search_paths;

        // fixme - what to do if we find multiples? 
        void walk_directory(path root)
        {
            for (auto & entry : recursive_directory_iterator(root))
            {
                const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
                auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
                for (auto & chr : path) if (chr == '\\') chr = '/';

                std::string ext = get_extension(path);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                std::string filename_no_ext = get_filename_without_extension(path);
                std::transform(filename_no_ext.begin(), filename_no_ext.end(), filename_no_ext.begin(), ::tolower);

                auto report_resolved_asset = [&](const std::string & name, const std::string & type_id)
                {
                    // std::cout << ">> \tResolved " << "(" << type_id << ") " << name << std::endl;
                    log::get()->engine_log->info("resolved {} ({})", name, type_id);
                };

                if (ext == "material")
                {
                    library->import_material(path);
                    report_resolved_asset(name, "material");
                }
                else if (ext == "png" || ext == "tga" || ext == "jpg" || ext == "jpeg")
                {
                    for (const auto & name : texture_names)
                    {
                        if (name == filename_no_ext)
                        {
                            create_handle_for_asset(name.c_str(), load_image(path, false));
                            report_resolved_asset(name, typeid(gl_texture_2d).name());
                        }
                    }
                }
                else if (ext == "dds")
                {
                    for (const auto & name : texture_names)
                    {
                        if (name == filename_no_ext)
                        {
                            auto cubemap_binary = read_file_binary(path);
                            gli::texture_cube cubemap_as_gli_cubemap(gli::load_dds((char *)cubemap_binary.data(), cubemap_binary.size()));
                            create_handle_for_asset(filename_no_ext.c_str(), load_cubemap(cubemap_as_gli_cubemap));
                            report_resolved_asset(name, "dds-cubemap");
                        }
                    }
                }
                else if (ext == "obj" || ext == "fbx" || ext == "mesh")
                {
                    // Name could either be something like "my_mesh" or "my_mesh/sub_component"
                    // `mesh_names` contains both CPU and GPU geometry handle ids
                    for (const auto & name : mesh_names)
                    {
                        // "my_mesh/sub_component" should match to "my_mesh.obj" or similar
                        if (find_root(name) == filename_no_ext)
                        {
                            std::unordered_map<std::string, runtime_mesh> imported_models = import_model(path);

                            for (auto & m : imported_models)
                            {
                                auto & mesh = m.second;
                                rescale_geometry(mesh, 1.f);

                                const std::string handle_id = filename_no_ext + "/" + m.first;

                                create_handle_for_asset(handle_id.c_str(), make_mesh_from_geometry(mesh));
                                create_handle_for_asset(handle_id.c_str(), std::move(mesh));

                                report_resolved_asset(name, typeid(gl_mesh).name());
                            }
                        }
                    }
                }
            }
        }

    public:

        void resolve()
        {
            assert(scene != nullptr);
            assert(library != nullptr);

            // Material Names
            for (auto & m : scene->render_system->materials)
                material_names.push_back(m.second.material.name);

            // GPU Geometry
            for (auto & m : scene->render_system->meshes)
                mesh_names.push_back(m.second.mesh.name);

            // CPU Geometry (same list as GPU)
            for (auto & m : scene->collision_system->meshes)
                mesh_names.push_back(m.second.geom.name);

            remove_duplicates(material_names);
            remove_duplicates(mesh_names);

            for (auto & mat : library->instances)
            {
                if (auto * pbr = dynamic_cast<polymer_pbr_standard*>(mat.second.get()))
                {
                    shader_names.push_back(pbr->shader.name);

                    texture_names.push_back(pbr->albedo.name);
                    texture_names.push_back(pbr->normal.name);
                    texture_names.push_back(pbr->metallic.name);
                    texture_names.push_back(pbr->roughness.name);
                    texture_names.push_back(pbr->emissive.name);
                    texture_names.push_back(pbr->height.name);
                    texture_names.push_back(pbr->occlusion.name);
                }

                if (auto * phong = dynamic_cast<polymer_blinn_phong_standard*>(mat.second.get()))
                {
                    shader_names.push_back(phong->shader.name);

                    texture_names.push_back(phong->diffuse.name);
                    texture_names.push_back(phong->normal.name);
                }
            }

            // IBLs 
            if (auto * ibl_cubemap_component = scene->render_system->get_cubemap())
            {
                texture_names.push_back(ibl_cubemap_component->ibl_irradianceCubemap.name);
                texture_names.push_back(ibl_cubemap_component->ibl_radianceCubemap.name);
            }

            remove_duplicates(shader_names);
            remove_duplicates(texture_names);
            
            for (auto & path : search_paths)
            {
                log::get()->engine_log->info("resolving directory " + path);
                walk_directory(path);
            }

        }

        asset_resolver::asset_resolver(environment * scene, material_library * library) 
            : scene(scene), library(library)
        {
        }

        void add_search_path(const std::string & search_path)
        {
            search_paths.push_back(search_path);
        }
    };

} // end namespace polymer

#endif // end polymer_asset_resolver_hpp

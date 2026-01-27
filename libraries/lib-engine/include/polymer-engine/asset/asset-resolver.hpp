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

#include "polymer-engine/asset/asset-handle.hpp"
#include "polymer-engine/asset/asset-handle-utils.hpp"
#include "polymer-engine/renderer/renderer-pbr.hpp"
#include "polymer-engine/material-library.hpp"

#include "polymer-core/util/string-utils.hpp"

#include "polymer-model-io/model-io.hpp"
#include "nlohmann/json.hpp"

using namespace std::filesystem;

namespace polymer
{
    class scene;

    class global_asset_dir : public singleton<global_asset_dir>
    {
        friend class polymer::singleton<global_asset_dir>;
        std::string the_asset_dir = "";

        std::string find_asset_directory(const std::vector<std::string> search_paths)
        {
            for (auto & search_path : search_paths)
            {
                log::get()->engine_log->info("searching {}", search_path);

                for (const auto & entry : recursive_directory_iterator(search_path))
                {
                    if (is_directory(entry.path()) && entry.path().filename() == "assets")
                    {
                        log::get()->engine_log->info("found asset dir {}", entry.path().string());
                        return entry.path().string();
                    }
                }
            }

            return {};
        }

        public:

        global_asset_dir()
        {
            std::vector<std::string> search_paths = {std::filesystem::current_path().string(),
                                                     std::filesystem::current_path().parent_path().string(),
                                                     std::filesystem::current_path().parent_path().parent_path().string(),
                                                     std::filesystem::current_path().parent_path().parent_path().parent_path().string()};

            the_asset_dir = find_asset_directory(search_paths);
        }

        const std::string get_asset_dir() const
        {
            return the_asset_dir;
        }
    };

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

    // The purpose of an asset resolver is to match an asset_handle to a file on disk. This is done
    // for scene objects (meshes, geometry) and materials (shaders, textures, cubemaps). The asset
    // resolver works in two passes, namely because materials require other shaders and textures
    // and we need to resolve those too. 
    class asset_resolver
    {
        scene * the_scene;
        material_library * mat_library;

        // Unresolved asset names
        std::vector<std::string> mesh_names;
        std::vector<std::string> shader_names;
        std::vector<std::string> material_names;
        std::vector<std::string> texture_names;

        std::vector<std::string> search_paths;

        std::unordered_map<uint32_t, bool> resolved;

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

                // returns true if not in the cache
                auto asset_resolve_cache = [&](const std::string & name, const std::string & type_id) -> bool
                {
                    const uint32_t key = poly_hash_fnv1a(type_id + "/" + name);
                    auto iter = resolved.find(key);

                    if (iter == resolved.end())
                    {
                        resolved[key] = true;
                        log::get()->engine_log->info("resolved: {} ({})", name, type_id);
                        return true;
                    }
                    return false;
                };

                if (ext == "material")
                {
                    if (asset_resolve_cache(name, "material"))
                    {
                        mat_library->import_material(path);
                    }
                }
                else if (ext == "png" || ext == "tga" || ext == "jpg" || ext == "jpeg")
                {
                    for (const auto & name : texture_names)
                    {
                        if (name == filename_no_ext)
                        {
                            if (asset_resolve_cache(name, typeid(gl_texture_2d).name()))
                            {
                                create_handle_for_asset(name.c_str(), load_image(path, false));
                            }
 
                        }
                    }
                }
                else if (ext == "dds")
                {
                    for (const auto & name : texture_names)
                    {
                        if (name == filename_no_ext)
                        {
                            if (asset_resolve_cache(name, "dds-cubemap"))
                            {
                                auto cubemap_binary = read_file_binary(path);
                                gli::texture_cube cubemap_as_gli_cubemap(gli::load_dds((char *)cubemap_binary.data(), cubemap_binary.size()));
                                create_handle_for_asset(filename_no_ext.c_str(), load_cubemap(cubemap_as_gli_cubemap));

                            }
                        }
                    }
                }
                else if (ext == "obj" || ext == "fbx" || ext == "ply" || ext == "mesh")
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
                                if (asset_resolve_cache(name, typeid(gl_mesh).name()))
                                {
                                    auto & mesh = m.second;
                                    rescale_geometry(mesh, 1.f);

                                    const std::string handle_id = filename_no_ext + "/" + m.first;

                                    create_handle_for_asset(handle_id.c_str(), make_mesh_from_geometry(mesh));
                                    create_handle_for_asset(handle_id.c_str(), std::move(mesh));
                                }
                            }
                        }
                    }
                }
            }
        }

    public:

        void resolve();

        asset_resolver(polymer::scene * the_scene, material_library * mat_library)
            : the_scene(the_scene), mat_library(mat_library) {}

        void add_search_path(const std::string & search_path)
        {
            search_paths.push_back(search_path);
        }
    };

} // end namespace polymer

#endif // end polymer_asset_resolver_hpp

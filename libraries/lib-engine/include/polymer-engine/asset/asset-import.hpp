#pragma once

#ifndef polymer_asset_import_utils_hpp
#define polymer_asset_import_utils_hpp

#include "polymer-engine/scene.hpp"

namespace polymer
{
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

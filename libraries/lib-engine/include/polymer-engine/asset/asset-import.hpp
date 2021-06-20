#pragma once

#ifndef polymer_asset_import_utils_hpp
#define polymer_asset_import_utils_hpp

#include "polymer-engine/scene.hpp"
#include "polymer-engine/asset/asset-resolver.hpp"

namespace polymer
{

    inline entity create_model(const std::string & geom_handle,
        const std::string & mesh_handle,
        scene & the_scene,
        entity_system_manager & orch)
    {
        const entity e = the_scene.track_entity(orch.create_entity());

        the_scene.identifier_system->create(e, mesh_handle);
        the_scene.xform_system->create(e, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::material_component model_mat(e);
        model_mat.material = material_handle(material_library::kDefaultMaterialId);
        the_scene.render_system->create(e, std::move(model_mat));

        polymer::mesh_component model_mesh(e);
        model_mesh.mesh = gpu_mesh_handle(mesh_handle);
        the_scene.render_system->create(e, std::move(model_mesh));

        polymer::geometry_component model_geom(e);
        model_geom.geom = cpu_mesh_handle(mesh_handle);
        the_scene.collision_system->create(e, std::move(model_geom));

        return e;
    }

    inline std::vector<entity> import_asset_runtime(const std::string & filepath,
        scene & the_scene,
        entity_system_manager & esm)
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

            if (num_models == 1) created_entities.push_back(create_model(handle_id, handle_id, the_scene, esm));
            else children.push_back(create_model(handle_id, handle_id, the_scene, esm));
        }

        if (children.size())
        {
            const entity root_entity = the_scene.track_entity(esm.create_entity());
            created_entities.push_back(root_entity);
            the_scene.identifier_system->create(root_entity, "root/" + name_no_ext);
            the_scene.xform_system->create(root_entity, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
            for (const entity child : children)
            {
                the_scene.xform_system->add_child(root_entity, child);
                created_entities.push_back(child);
            }
        }

        // Flatten unordered_map of mesh assets into a list of names
        //std::vector<std::string> submesh_names;
        //for (auto m : imported_models) submesh_names.push_back(m.first);
        //create_asset_descriptor(filepath, submesh_names); // write out .meta on import

        return created_entities;
    }

} // end namespace polymer

#endif // end polymer_asset_import_utils_hpp

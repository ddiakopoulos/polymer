#pragma once

#ifndef polymer_asset_import_utils_hpp
#define polymer_asset_import_utils_hpp

#include "environment.hpp"
#include "../lib-model-io/model-io.hpp"

namespace polymer
{

    inline entity create_model(const std::string & geom_handle,
        const std::string & mesh_handle,
        environment & env,
        entity_orchestrator & orch)
    {
        const entity e = env.track_entity(orch.create_entity());

        env.identifier_system->create(e, mesh_handle);
        env.xform_system->create(e, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

        polymer::material_component model_mat(e);
        model_mat.material = material_handle(material_library::kDefaultMaterialId);
        env.render_system->create(e, std::move(model_mat));

        polymer::mesh_component model_mesh(e);
        model_mesh.mesh = gpu_mesh_handle(mesh_handle);
        env.render_system->create(e, std::move(model_mesh));

        polymer::geometry_component model_geom(e);
        model_geom.geom = cpu_mesh_handle(mesh_handle);
        env.collision_system->create(e, std::move(model_geom));;

        return e;
    }

    inline std::vector<entity> import_asset(const std::string & filepath,
        environment & env,
        entity_orchestrator & orch)
    {
        std::vector<entity> created_entities;

        auto path = filepath;

        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        const std::string ext = get_extension(path);

        // Handle image/texture types
        if (ext == "png" || ext == "tga" || ext == "jpg")
        {
            create_handle_for_asset(get_filename_without_extension(path).c_str(), load_image(path, false));
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

            const std::string handle_id = get_filename_without_extension(path) + "-" + m.first;

            create_handle_for_asset(handle_id.c_str(), make_mesh_from_geometry(mesh));
            create_handle_for_asset(handle_id.c_str(), std::move(mesh));

            if (num_models == 1) created_entities.push_back(create_model(handle_id, handle_id, env, orch));
            else children.push_back(create_model(handle_id, handle_id, env, orch));
        }

        if (children.size())
        {
            const entity root_entity = env.track_entity(orch.create_entity());
            created_entities.push_back(root_entity);
            env.identifier_system->create(root_entity, "root-" + std::to_string(root_entity));
            env.xform_system->create(root_entity, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });
            for (const entity child : children)
            {
                env.xform_system->add_child(root_entity, child);
                created_entities.push_back(child);
            }
        }

        return created_entities;
    }

} // end namespace polymer

#endif // end polymer_asset_import_utils_hpp

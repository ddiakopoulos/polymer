#include "polymer-engine/asset/asset-resolver.hpp"
#include "polymer-engine/scene.hpp"
#include "polymer-engine/object.hpp"

#include <cassert>

using namespace polymer;

template <>global_asset_dir * polymer::singleton<global_asset_dir>::single = nullptr;

void asset_resolver::resolve()
{
    assert(the_scene != nullptr);
    assert(mat_library != nullptr);

    // Collect asset names from scene graph objects using the new pattern
    for (auto & [e, obj] : the_scene->get_graph().graph_objects)
    {
        // Material Names
        if (auto * mat_comp = obj.get_component<material_component>())
        {
            material_names.push_back(mat_comp->material.name);
        }

        // GPU Geometry (mesh component)
        if (auto * mesh_comp = obj.get_component<mesh_component>())
        {
            mesh_names.push_back(mesh_comp->mesh.name);
        }

        // CPU Geometry (geometry component for collision)
        if (auto * geom_comp = obj.get_component<geometry_component>())
        {
            mesh_names.push_back(geom_comp->geom.name);
        }

        // IBL cubemap textures
        if (auto * ibl_comp = obj.get_component<ibl_component>())
        {
            texture_names.push_back(ibl_comp->ibl_irradianceCubemap.name);
            texture_names.push_back(ibl_comp->ibl_radianceCubemap.name);
        }
    }

    remove_duplicates(material_names);
    remove_duplicates(mesh_names);

    auto collect_shaders_and_textures = [this]()
    {
        for (auto & mat : mat_library->instances)
        {
            if (auto * pbr = dynamic_cast<polymer_pbr_standard*>(mat.second.instance.get()))
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

            if (auto * phong = dynamic_cast<polymer_blinn_phong_standard*>(mat.second.instance.get()))
            {
                shader_names.push_back(phong->shader.name);

                texture_names.push_back(phong->diffuse.name);
                texture_names.push_back(phong->normal.name);
            }
        }

        remove_duplicates(shader_names);
        remove_duplicates(texture_names);
    };

    // First Pass. Grab shaders and textures programmatically defined.
    collect_shaders_and_textures();

    // Resolve known assets, including materials.
    for (auto & path : search_paths)
    {
        log::get()->engine_log->info("[1] resolving directory " + path);
        walk_directory(path);
    }

    // Second Pass. Collect shaders and textures again, because materials might define them and import them.
    collect_shaders_and_textures();

    // Resolve known assets again, this time including shaders and textures that may
    // have been identified by an imported material.
    // @fixme - we need to remove already resolved assets from being imported a second time!
    for (auto & path : search_paths)
    {
        log::get()->engine_log->info("[2] resolving directory " + path);
        walk_directory(path);
    }
}

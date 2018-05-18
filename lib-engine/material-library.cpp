#include "material.hpp"
#include "material-library.hpp"
#include "serialization.inl"
#include "asset-handle-utils.hpp"
#include "logging.hpp"
#include "file_io.hpp"

using namespace polymer;

const std::string material_library::kDefaultMaterialId = "default-material";

material_library::material_library(const std::string & library_path) : library_path(library_path)
{
    // Create a default material and create an asset handle for it (also add to local instances)
    std::shared_ptr<polymer_default_material> default = std::make_shared<polymer_default_material>();
    create_handle_for_asset(kDefaultMaterialId.c_str(), static_cast<std::shared_ptr<material_interface>>(default));
    instances[kDefaultMaterialId] = default; 

    // Check if the library already exists and load if possible
    std::ifstream library(library_path);
    if (library.good())
    {
        cereal::deserialize_from_json(library_path, instances);
    }
    else
    {
        // Create new library with default material
        serialize();
    }

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : instances)
    {
        create_handle_for_asset(instance.first.c_str(), static_cast<std::shared_ptr<material_interface>>(instance.second));
    }
}

material_library::~material_library()
{
    // Save on exit
    serialize();

    // Should we also call material_handle::destroy(...) for all the material assets? 
    instances.clear();
}

void material_library::serialize()
{
    const std::string & json_out = cereal::serialize_to_json(instances);
    polymer::write_file_text(library_path, json_out);
}

void material_library::remove_material(const std::string & name)
{
    const auto itr = instances.find(name);
    if (itr != instances.end())
    {
        instances.erase(itr);
        material_handle::destroy(name);
        log::get()->assetLog->info("removing {} from the material list", name);
        serialize();
    }
    else
    {
        log::get()->assetLog->info("{} was not found in the material list", name);
    }
}
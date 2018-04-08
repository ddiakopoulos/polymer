#include "material-library.hpp"
#include "serialization.hpp"
#include "asset-defs.hpp"
#include "material.hpp"
#include "logging.hpp"

using namespace polymer;

material_library::material_library(const std::string & library_path) : library_path(library_path)
{
    // Create a default material
    std::shared_ptr<DefaultMaterial> default = std::make_shared<DefaultMaterial>();
    create_handle_for_asset("default-material", static_cast<std::shared_ptr<Material>>(default));
    cereal::deserialize_from_json(library_path, instances);

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : instances)
    {
        create_handle_for_asset(instance.first.c_str(), static_cast<std::shared_ptr<Material>>(instance.second));
    }
}

material_library::~material_library()
{
    // Should we also call MaterialHandle::destroy(...) for all the material assets? 
    instances.clear();
}

void material_library::create_material(const std::string & name, std::shared_ptr<MetallicRoughnessMaterial> mat)
{
    auto itr = std::find(instances.begin(), instances.end(), name);
    if (itr != instances.end())
    {
        Logger::get_instance()->assetLog->info("material list already contains {}", name);
        return;
    }
    create_handle_for_asset(name.c_str(), std::dynamic_pointer_cast<Material>(mat));
    instances[name] = mat;
}

void material_library::remove_material(const std::string & name)
{
    auto itr = std::find(instances.begin(), instances.end(), name);
    if (itr != instances.end())
    {
        instances.erase(itr);
        MaterialHandle::destroy(name);
        auto jsonString = cereal::serialize_to_json(library_path);
        write_file_text("materials.json", jsonString);
        Logger::get_instance()->assetLog->info("removing {} from the material list", name);
    }
    else
    {
        Logger::get_instance()->assetLog->info("{} was not found in the material list", name);
    }
}
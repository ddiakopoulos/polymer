#include "material.hpp"
#include "material-library.hpp"
#include "serialization.inl"
#include "asset-handle-utils.hpp"
#include "logging.hpp"
#include "file_io.hpp"
#include "json.hpp"

using namespace polymer;
using json = nlohmann::json;

const std::string material_library::kDefaultMaterialId = "default-material";

material_library::material_library(const std::string & library_path) : library_path(library_path)
{
    // Create a default material and create an asset handle for it (also add to local instances)
    std::shared_ptr<polymer_default_material> default = std::make_shared<polymer_default_material>();
    create_handle_for_asset(kDefaultMaterialId.c_str(), static_cast<std::shared_ptr<material_interface>>(default));
    instances[kDefaultMaterialId] = default; 

    // Check if the library already exists and load if possible
    //std::ifstream library(library_path);
    //if (library.good())
    //{
    //    //cereal::deserialize_from_json(library_path, instances);
    //}
    //else
    {
        // Create new library with default material
        export_library();
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
    export_library();

    // Should we also call material_handle::destroy(...) for all the material assets? 
    instances.clear();
}

void material_library::import_library()
{

}

void material_library::export_library()
{
    json instance_out;

    for (auto & material : instances)
    {
        const auto material_name = material.first;
        json mat_instance;

        visit_subclasses(material.second.get(), [&](const char * name, auto * p)
        {
            if (p)
            {
                std::string type_name = get_typename<std::remove_pointer<decltype(p)>::type>();
                std::string material_type_id = "@" + type_name;
                json mat;

                visit_fields(*p, [&](const char * field_name, auto & field_value, auto... metadata)
                {
                    mat[field_name] = field_value;
                });

                mat_instance[material_name] = mat;
            }
        });
    }

    std::cout << instance_out.dump(4) << std::endl;

    //polymer::write_file_text(library_path, json_out);
}


void material_library::remove_material(const std::string & name)
{
    const auto itr = instances.find(name);
    if (itr != instances.end())
    {
        instances.erase(itr);
        material_handle::destroy(name);
        log::get()->assetLog->info("removing {} from the material list", name);
        export_library();
    }
    else
    {
        log::get()->assetLog->info("{} was not found in the material list", name);
    }
}
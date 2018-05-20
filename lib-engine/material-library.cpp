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
    std::ifstream library(library_path);
    if (library.good())
    {
        import_library();
    }
    else
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
    const json library_doc = json::parse(read_file_text(library_path));

    // Iterate over material instances
    for (auto it = library_doc.begin(); it != library_doc.end(); ++it)
    {
        const json & instance = it.value();
        for (auto inst = instance.begin(); inst != instance.end(); ++inst)
        {
            if (starts_with(inst.key(), "@"))
            {
                const std::string type_key = inst.key();
                const std::string type_name = type_key.substr(1);

                if (type_name == get_typename<polymer_pbr_standard>())
                {
                    std::shared_ptr<polymer_pbr_standard> new_instance(new polymer_pbr_standard());
                    *new_instance = inst.value();
                    instances[it.key()] = new_instance;
                }
                else if (type_name == get_typename<polymer_blinn_phong_standard>())
                {
                    std::shared_ptr<polymer_blinn_phong_standard> new_instance(new polymer_blinn_phong_standard());
                    *new_instance = inst.value();
                    instances[it.key()] = new_instance;
                }
            }
            else throw std::runtime_error("type key mismatch!");
        }
    }
}

void material_library::export_library()
{
    json library;

    for (auto & material_instance : instances)
    {
        visit_subclasses(material_instance.second.get(), [&](const char * name, auto * material_ptr)
        {
            if (material_ptr)
            {
                std::string type_name = get_typename<std::remove_pointer<decltype(material_ptr)>::type>();
                std::string material_type_id = "@" + type_name;
                library[material_instance.first][material_type_id] = *material_ptr;
            }
        });
    }

    std::cout << library.dump(4) << std::endl;
    polymer::write_file_text(library_path, library.dump(4) );
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
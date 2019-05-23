#include "material.hpp"
#include "material-library.hpp"
#include "serialization.hpp"
#include "asset-handle-utils.hpp"
#include "logging.hpp"
#include "file_io.hpp"
#include "json.hpp"
#include "environment.hpp"
#include "string_utils.hpp"

using namespace polymer;
using json = nlohmann::json;

const std::string material_library::kDefaultMaterialId = "default-material";

void material_library::search()
{
    for (const auto & path : search_paths)
    {
        for (auto & entry : recursive_directory_iterator(path))
        {
            const size_t root_len = path.length(), ext_len = entry.path().extension().string().length();
            auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
            for (auto & chr : path) if (chr == '\\') chr = '/';

            if (entry.path().extension().string() == ".material")
            {
                import_material(path);
            }
        }
    }

};

material_library::material_library(const std::string & default_search_path)
{
    search_paths.push_back(default_search_path);

    // Create an empty asset for textures. 
    create_handle_for_asset("", gl_texture_2d());

    // Create a default material and create an asset handle for it (also add to local instances)
    std::shared_ptr<polymer_default_material> default = std::make_shared<polymer_default_material>();
    create_handle_for_asset(kDefaultMaterialId.c_str(), static_cast<std::shared_ptr<material_interface>>(default));
    instances[kDefaultMaterialId] = default; 

    search();

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : instances)
    {
        create_handle_for_asset(instance.first.c_str(), static_cast<std::shared_ptr<material_interface>>(instance.second));
    }
}

material_library::~material_library()
{
    //export_all();


    // Should we also call material_handle::destroy(...) for all the material assets? 
    instances.clear();
}

void material_library::export_all()
{
    // Save all instances to disk
    for (auto & instance : instances)
    {
        export_material(instance.first, search_paths[0]);
    }
}

void material_library::import_material(const std::string & path)
{
    // todo - de-duplicate based on name
    const json instance_doc = json::parse(read_file_text(path));
    const std::string name = get_filename_without_extension(path);
    assert(!name.empty());

    for (auto inst = instance_doc.begin(); inst != instance_doc.end(); ++inst)
    {
        if (starts_with(inst.key(), "@"))
        {
            const std::string type_key = inst.key();
            const std::string type_name = type_key.substr(1);

            if (type_name == get_typename<polymer_pbr_standard>())
            {
                std::shared_ptr<polymer_pbr_standard> new_instance(new polymer_pbr_standard());
                *new_instance = inst.value();
                instances[name] = new_instance;
            }
            else if (type_name == get_typename<polymer_blinn_phong_standard>())
            {
                std::shared_ptr<polymer_blinn_phong_standard> new_instance(new polymer_blinn_phong_standard());
                *new_instance = inst.value();
                instances[name] = new_instance;
            }
        }
        else throw std::runtime_error("type key mismatch!");
    }
}

void material_library::export_material(const std::string & name, const std::string & path)
{
    auto itr = instances.find(name);
    if (itr == instances.end()) throw std::runtime_error("no material by that name in the library");

    json library;
    visit_subclasses(itr->second.get(), [&](const char * name, auto * material_ptr)
    {
        if (material_ptr)
        {
            std::string type_name = get_typename<std::remove_pointer<decltype(material_ptr)>::type>();
            std::string material_type_id = "@" + type_name;
            library[material_type_id] = *material_ptr;
        }
    });

    polymer::write_file_text(path + "/" + name + ".material", library.dump(4));
}

void material_library::remove_material(const std::string & name)
{
    const auto itr = instances.find(name);
    if (itr != instances.end())
    {
        instances.erase(itr);
        material_handle::destroy(name);
        log::get()->engine_log->info("removing {} from the material list", name);
    }
    else
    {
        log::get()->engine_log->info("{} was not found in the material list", name);
    }
}

void material_library::add_search_path(const std::string & search_path)
{
    search_paths.push_back(search_path);
    search();
}
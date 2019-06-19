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

material_library::material_library()
{
    // Create an empty/null asset for textures. We use this when we want to clear texture handle slots when
    // editing via the polymer scene editor ui.
    create_handle_for_asset("", gl_texture_2d());

    // Create a default material and create an asset handle for it (also add to local instances)
    std::shared_ptr<polymer_default_material> default = std::make_shared<polymer_default_material>();
    create_handle_for_asset(kDefaultMaterialId.c_str(), static_cast<std::shared_ptr<base_material>>(default));

    material_instance inst;
    inst.name = kDefaultMaterialId;
    inst.type = instance_type::procedural;
    inst.instance = default;

    instances[kDefaultMaterialId] = inst; 
}

material_library::~material_library()
{
    //export_all();
    instances.clear(); // Should we also call material_handle::destroy(...) for all the material assets? 
}

void material_library::export_all()
{
    // Save all instances to disk
    for (auto & instance : instances)
    {
        export_material(instance.second.name);
    }
}

void material_library::import_material(const std::string & path)
{
    // todo - de-duplicate based on name
    const json instance_doc = json::parse(read_file_text(path));
    const std::string name = get_filename_without_extension(path);
    const std::string parent_path = parent_directory_from_filepath(path);
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

                material_instance inst;
                inst.name = name;
                inst.origin_path = parent_path;
                inst.instance = new_instance;
                instances[name] = inst;

                create_handle_for_asset(name.c_str(), static_cast<std::shared_ptr<base_material>>(new_instance));
            }
            else if (type_name == get_typename<polymer_blinn_phong_standard>())
            {
                std::shared_ptr<polymer_blinn_phong_standard> new_instance(new polymer_blinn_phong_standard());
                *new_instance = inst.value();

                material_instance inst;
                inst.name = name;
                inst.origin_path = parent_path;
                inst.instance = new_instance;
                instances[name] = inst;

                create_handle_for_asset(name.c_str(), static_cast<std::shared_ptr<base_material>>(new_instance));
            }
        }
        else throw std::runtime_error("type key mismatch!");
    }
}

void material_library::export_material(const std::string & key)
{
    auto itr = instances.find(key);
    if (itr == instances.end()) throw std::runtime_error("no material by that name in the library");

    if (itr->second.type == instance_type::serializable)
    {
        json out_instance_doc;
        visit_subclasses(itr->second.instance.get(), [&](const char * name, auto * material_ptr)
        {
            if (material_ptr)
            {
                std::string type_name = get_typename<std::remove_pointer<decltype(material_ptr)>::type>();
                std::string material_type_id = "@" + type_name;
                out_instance_doc[material_type_id] = *material_ptr;
            }
        });

        polymer::write_file_text(itr->second.origin_path + "/" + key + ".material", out_instance_doc.dump(4));
    }
}

void material_library::remove_material(const std::string & key)
{
    const auto itr = instances.find(key);
    if (itr != instances.end())
    {
        instances.erase(itr);
        material_handle::destroy(key);
        log::get()->engine_log->info("removing {} from the material list", key);
    }
    else
    {
        log::get()->engine_log->info("{} was not found in the material list", key);
    }
}

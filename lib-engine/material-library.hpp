#pragma once

#ifndef polymer_material_library_hpp
#define polymer_material_library_hpp

#include "asset-handle.hpp"

namespace polymer
{

    // Forward declarations
    struct base_material;
    struct polymer_default_material;
    class polymer_pbr_standard;
    class polymer_blinn_phong_standard;

    // Materials are serialized separately from a scene. 
    // All materials also live in the static table of asset_handles, but
    // this is where serialization/deserialization occurs.
    struct material_library
    {
        enum class instance_type
        {
            serializable,
            procedural
        };

        struct material_instance
        {
            std::string name;
            std::string origin_path;
            std::shared_ptr<base_material> instance;
            instance_type type {instance_type::serializable};
        };

        static const std::string kDefaultMaterialId;
        std::map<std::string, material_instance> instances;

        material_library();
        ~material_library();

        // Use this function to programmatically register new materials in the instance table and the global asset handle table
        template<typename T> void register_material(const std::string & name, std::shared_ptr<T> mat);

        // Removes from local instances and deletes handle from global table
        void remove_material(const std::string & key);

        // Deserializes *.material file from disk, importing into the local instances and creating a handle in the global table
        void import_material(const std::string & path);

        // Serializes a named material instance into a *.material file onto disk
        void export_material(const std::string & key);

        // Batch export all named instances, equivalent to calling `export_material(...)` on every known material
        void export_all();
    };

    template<typename T>
    void material_library::register_material(const std::string & name, std::shared_ptr<T> mat)
    {
        const auto itr = instances.find(name);
        if (itr != instances.end())
        {
            log::get()->engine_log->info("material list already contains {}", name);
            return;
        }

        // register_material is only used as a programmatic api, thus the instances don't need to retain
        // any other information used for serialization
        material_instance inst;
        inst.type = instance_type::procedural;
        inst.name = name;
        inst.instance = mat;
        instances[name] = inst;

        create_handle_for_asset(name.c_str(), std::dynamic_pointer_cast<base_material>(mat));
    }
}

#endif // end polymer_material_library_hpp

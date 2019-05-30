#pragma once

#ifndef polymer_material_library_hpp
#define polymer_material_library_hpp

#include "asset-handle.hpp"

namespace polymer
{

    // Forward declarations
    struct material_interface;
    struct polymer_default_material;
    class polymer_pbr_standard;
    class polymer_blinn_phong_standard;

    // Materials are serialized separately from a scene. 
    // All materials also live in the static table of asset_handles, but
    // this is where serialization/deserialization occurs.
    struct material_library
    {
        static const std::string kDefaultMaterialId;
        std::map<std::string, std::shared_ptr<material_interface>> instances;

        material_library();
        ~material_library();

        // Use this function to programmatically register new materials in the instance table and the global asset handle table
        template<typename T> void register_material(const std::string & name, std::shared_ptr<T> mat);

        // Removes from local instances and deletes handle from global table
        void remove_material(const std::string & name);

        // Deserializes *.material file from disk, importing into the local instances and creating a handle in the global table
        void import_material(const std::string & path);

        // Serializes a named material instance into a *.material file onto disk
        void export_material(const std::string & name, const std::string & path);
       
        // Batch export all named instances, equivalent to calling `export_material(...)` on every known material
        void export_all(const std::string & to_path);
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
        create_handle_for_asset(name.c_str(), std::dynamic_pointer_cast<material_interface>(mat));
        instances[name] = mat;
    }
}

#endif // end polymer_material_library_hpp

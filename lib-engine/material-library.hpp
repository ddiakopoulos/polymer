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
        std::string search_path;
        material_library(const std::string & search_path);
        ~material_library();
        template<typename T> void create_material(const std::string & name, std::shared_ptr<T> mat);
        void remove_material(const std::string & name);
        void import_material(const std::string & path);
        void export_material(const std::string & name, const std::string & path);
        void export_all();
        void search();
    };

    template<typename T>
    void material_library::create_material(const std::string & name, std::shared_ptr<T> mat)
    {
        const auto itr = instances.find(name);
        if (itr != instances.end())
        {
            log::get()->assetLog->info("material list already contains {}", name);
            return;
        }
        create_handle_for_asset(name.c_str(), std::dynamic_pointer_cast<material_interface>(mat));
        instances[name] = mat;
    }
}

#endif // end polymer_material_library_hpp

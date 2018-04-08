#pragma once

#include "asset-handle.hpp"
#include "file_io.hpp"

namespace polymer
{
    // Forward declarations
    struct Material;
    struct DefaultMaterial;
    class MetallicRoughnessMaterial;

    // Materials are serialized separately from a scene. Cereal-deserializes
    // directly into |instances| and is used as a convenience container,
    // although all materials also live in the static table of asset_handles.
    struct material_library
    {
        static const std::string kDefaultMaterialId;
        std::map<std::string, std::shared_ptr<Material>> instances;
        std::string library_path;
        material_library(const std::string & library_path);
        //~material_library();
        void create_material(const std::string & name, std::shared_ptr<MetallicRoughnessMaterial> mat);
        void remove_material(const std::string & name);
    };
}
#pragma once

#ifndef polymer_asset_handle_utils_hpp
#define polymer_asset_handle_utils_hpp

#include "polymer-engine/asset/asset-handle.hpp"
#include "polymer-engine/ecs/typeid.hpp"

namespace polymer
{
    template<class T> inline asset_handle<T> create_handle_for_asset(const char * asset_id, T && asset)
    {
        static_assert(!std::is_pointer<T>::value, "cannot create a handle for a raw pointer");
        return { asset_handle<T>(asset_id, std::move(asset)) };
    }

    template<> inline asset_handle<geometry> create_handle_for_asset(const char * asset_id, geometry && asset)
    {
        assert(asset.vertices.size() > 0); // verify that this the geometry is not empty
        return { asset_handle<geometry>(asset_id, std::move(asset)) };
    }

    template<> inline asset_handle<gl_mesh> create_handle_for_asset(const char * asset_id, gl_mesh && asset)
    {
        assert(asset.get_vertex_data_buffer() > 0); // verify that this is a well-formed gl_mesh object
        return { asset_handle<gl_mesh>(asset_id, std::move(asset)) };
    }

    // Forward declarations
    struct base_material;
    class gl_shader_asset;

    typedef asset_handle<gl_texture_2d>                    texture_handle;
    typedef asset_handle<gl_texture_cube>                  cubemap_handle;
    typedef asset_handle<gl_mesh>                          gpu_mesh_handle;
    typedef asset_handle<geometry>                         cpu_mesh_handle;
    typedef asset_handle<std::shared_ptr<base_material>>   material_handle;
    typedef asset_handle<std::shared_ptr<gl_shader_asset>> shader_handle;

    POLYMER_SETUP_TYPEID(texture_handle);
    POLYMER_SETUP_TYPEID(cubemap_handle);
    POLYMER_SETUP_TYPEID(gpu_mesh_handle);
    POLYMER_SETUP_TYPEID(cpu_mesh_handle);
    POLYMER_SETUP_TYPEID(material_handle);
    POLYMER_SETUP_TYPEID(shader_handle);

} // end namespace polymer

#endif // end polymer_asset_handle_utils_hpp

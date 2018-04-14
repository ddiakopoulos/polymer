#pragma once

#ifndef polymer_asset_handle_utils_hpp
#define polymer_asset_handle_utils_hpp

#include "asset-handle.hpp"

template<class T> inline asset_handle<T> create_handle_for_asset(const char * asset_id, T && asset)
{
    std::cout << "assigning: " << asset_id << std::endl;
    static_assert(!std::is_pointer<T>::value, "cannot create a handle for a raw pointer");
    return { asset_handle<T>(asset_id, std::move(asset)) };
}

template<> inline asset_handle<Geometry> create_handle_for_asset(const char * asset_id, Geometry && asset)
{
    assert(asset.vertices.size() > 0); // verify that this the geometry is not empty
    return { asset_handle<Geometry>(asset_id, std::move(asset)) };
}

template<> inline asset_handle<GlMesh> create_handle_for_asset(const char * asset_id, GlMesh && asset)
{
    assert(asset.get_vertex_data_buffer() > 0); // verify that this is a well-formed GlMesh object
    return { asset_handle<GlMesh>(asset_id, std::move(asset)) };
}

// Note that the asset_handle system strongly typed, meaning that the difference betweeen 
// forward-declaring classes/structs is not just a semantic. This comment is basically
// a reminder that I "fixed" a bug where the following forward declaration was a class,
// and nothing could find the asset, but all the data and names were correctly setup.
namespace polymer
{
    struct material_interface;
    class gl_shader_asset;
}

typedef asset_handle<GlTexture2D>                        			texture_handle;
typedef asset_handle<GlMesh>                             			gpu_mesh_handle;
typedef asset_handle<Geometry>                           			cpu_mesh_handle;
typedef asset_handle<std::shared_ptr<polymer::material_interface>>  material_handle;
typedef asset_handle<std::shared_ptr<polymer::gl_shader_asset>>    	shader_handle;

#endif // end polymer_asset_handle_utils_hpp

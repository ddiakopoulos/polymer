#pragma once

#include "asset-handle.hpp"

template<class T> inline asset_handle<T> create_handle_for_asset(const char * asset_id, T && asset)
{
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

namespace polymer
{
    class Material;
}

typedef asset_handle<GlTexture2D>                        GlTextureHandle;
typedef asset_handle<GlShader>                           GlShaderHandle;
typedef asset_handle<GlMesh>                             GlMeshHandle;
typedef asset_handle<Geometry>                           GeometryHandle;
typedef asset_handle<std::shared_ptr<polymer::Material>> MaterialHandle;

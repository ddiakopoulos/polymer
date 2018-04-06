#pragma once

#ifndef asset_handles_hpp
#define asset_handles_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"
#include "logging.hpp"

#include <memory>
#include <unordered_map>

static inline uint64_t system_time_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// Note that the asset of `UniqueAsset` must be default constructable.
template<typename T>
struct UniqueAsset : public non_copyable
{
    T asset;
    bool assigned{ false };
    uint64_t timestamp;
};

template<typename T>
class AssetHandle
{
    static std::unordered_map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
    mutable std::shared_ptr<UniqueAsset<T>> handle{ nullptr };
    AssetHandle(const::std::string & id, std::shared_ptr<UniqueAsset<T>> h) : name(id), handle(h) {} // private constructor for the static list() method below

public:

    std::string name;

    AssetHandle() : AssetHandle("") {}
    AssetHandle(const std::string & asset_id, T && asset) : AssetHandle(asset_id.c_str()) { assign(std::move(asset)); }
    AssetHandle(const std::string & asset_id) : AssetHandle(asset_id.c_str()) {}
    AssetHandle(const char * asset_id)
    {
        name = asset_id;
        if (name.empty())
        {
            name = "default";
        }
    }

    AssetHandle(const AssetHandle & r)
    {
        handle = r.handle;
        name = r.name;
    }

    // Return reference to underlying resource. 
    T & get() const
    { 
        // Check if this handle has a cached asset
        if (handle)
        {
            return handle->asset; 
        }
        // Lazy load
        else 
        {
            // If not, this is a virgin handle and we should lookup from the static table.
            auto & a = table[name];
            if (!a)
            {
                a = std::make_shared<UniqueAsset<T>>();
                a->timestamp = system_time_ns();
                a->assigned = false;
                Logger::get_instance()->assetLog->info("asset type {} ({}) was default constructed", typeid(T).name(), name);
            }
            handle = a;
            return handle->asset;
        }
    }

    T & assign(T && asset)
    {
        auto & a = table[name];

        // New asset
        if (!a)
        {
            a = std::make_shared<UniqueAsset<T>>();
            a->timestamp = system_time_ns();
        }

        handle = a;
        handle->asset = std::move(asset);
        handle->assigned = true;
        handle->timestamp = system_time_ns();

        Logger::get_instance()->assetLog->info("asset type {} with id {} was assigned", typeid(T).name(), name);
        //Logger::get_instance()->assetLog->info("asset type {} with id {} was assigned", typeid(this).name(), name);

        return handle->asset;
    }

    bool assigned() const
    {
        if (handle && handle->assigned) return true;
        auto & a = table[name];
        if (a)
        {
            handle = a;
            return handle->assigned;
        }
        return false;
    }

    static std::vector<AssetHandle> list()
    {
        std::vector<AssetHandle> results;
        for (const auto & a : table) results.push_back(AssetHandle<T>(a.first, a.second));
        return results;
    }

    static bool destroy(const std::string & asset_id)
    {
        auto iter = table.find(asset_id);
        if (iter != table.end())
        {
            Logger::get_instance()->assetLog->info("asset type {} with id {} was destroyed", typeid(T).name(), iter->first);
            table.erase(iter);
            return true;
        }
        return false;
    }
};

template<class T> 
std::unordered_map<std::string, std::shared_ptr<UniqueAsset<T>>> AssetHandle<T>::table;

template<class T> inline AssetHandle<T> create_handle_for_asset(const char * asset_id, T && asset)
{
    static_assert(!std::is_pointer<T>::value, "cannot create a handle for a raw pointer");
    return { AssetHandle<T>(asset_id, std::move(asset)) };
}

template<> inline AssetHandle<Geometry> create_handle_for_asset(const char * asset_id, Geometry && asset)
{
    assert(asset.vertices.size() > 0); // verify that this the geometry is not empty
    return { AssetHandle<Geometry>(asset_id, std::move(asset)) };
}

template<> inline AssetHandle<GlMesh> create_handle_for_asset(const char * asset_id, GlMesh && asset)
{
    assert(asset.get_vertex_data_buffer() > 0); // verify that this is a well-formed GlMesh object
    return { AssetHandle<GlMesh>(asset_id, std::move(asset)) };
}

typedef AssetHandle<GlTexture2D> GlTextureHandle;
typedef AssetHandle<GlShader> GlShaderHandle;
typedef AssetHandle<GlMesh> GlMeshHandle;
typedef AssetHandle<Geometry> GeometryHandle;

#endif // end asset_handles_hpp

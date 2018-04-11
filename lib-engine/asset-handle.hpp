#pragma once

#ifndef polymer_asset_handle_hpp
#define polymer_asset_handle_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"
#include "logging.hpp"
#include <unordered_map>

static inline uint64_t system_time_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// Note that the asset of `polymer_unique_asset` must be default constructable.
template<typename T>
struct polymer_unique_asset : public non_copyable
{
    T asset;
    bool assigned{ false };
    uint64_t timestamp;
};

// An asset handle contains static table of string <=> asset mappings. Although unique assets
// are constructed on the heap at runtime, they are loaned out as references. 
// Assets stored with the system must be default constructable; this is primarily done for
// prototyping since it is far more tedious than the alternative of an extensive resolve mechanism.
// Asset handles are not thread safe, but safety can be added with relative ease. 
template<typename T>
class asset_handle
{
    static std::unordered_map<std::string, std::shared_ptr<polymer_unique_asset<T>>> table;
    mutable std::shared_ptr<polymer_unique_asset<T>> handle{ nullptr };
    asset_handle(const::std::string & id, std::shared_ptr<polymer_unique_asset<T>> h) : name(id), handle(h) {} // private constructor for the static list() method below

public:

    std::string name;

    asset_handle() : asset_handle("") {}
    asset_handle(const std::string & asset_id, T && asset) : asset_handle(asset_id.c_str()) { assign(std::move(asset)); }
    asset_handle(const std::string & asset_id) : asset_handle(asset_id.c_str()) {}
    asset_handle(const char * asset_id)
    {
        name = asset_id;
        if (name.empty())
        {
            name = "default";
        }
    }

    asset_handle(const asset_handle & r)
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
                a = std::make_shared<polymer_unique_asset<T>>();
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
            a = std::make_shared<polymer_unique_asset<T>>();
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

    static std::vector<asset_handle> list()
    {
        std::vector<asset_handle> results;
        for (const auto & a : table) results.push_back(asset_handle<T>(a.first, a.second));
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
std::unordered_map<std::string, std::shared_ptr<polymer_unique_asset<T>>> asset_handle<T>::table;

#endif // end polymer_asset_handle_hpp

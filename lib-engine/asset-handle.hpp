#pragma once

#ifndef polymer_asset_handle_hpp
#define polymer_asset_handle_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"
#include "logging.hpp"
#include <unordered_map>

//#define ASSET_DEBUG_SPAM

namespace polymer
{

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
        // Private constructor for the static list() method below.
        asset_handle(const::std::string & id, std::shared_ptr<polymer_unique_asset<T>> h) : name(id), handle(h) {}

    public:

        std::string name;

        asset_handle() : asset_handle("") {}
        asset_handle(const std::string & asset_id, T && asset) : asset_handle(asset_id.c_str()) { assign(std::move(asset)); }
        asset_handle(const std::string & asset_id) : asset_handle(asset_id.c_str()) {}
        asset_handle(const char * asset_id)
        {
            name = asset_id;
            if (name.empty()) { name = "empty"; }
        }

        asset_handle(const asset_handle & r)
        {
            handle = r.handle;
            name = r.name;
        }

        // Return reference to underlying resource. 
        T & get() const
        {
            // Check if this handle has a cached asset. 
            if (handle)
            {
                return handle->asset;
            }
            // Lazy load
            else
            {
                // If not, this is a virgin handle and we should lookup from the static table.
                auto & a = table[name];

                // If there's no loaded asset in the table for this identifier, default construct one. 
                // This behavior might be changed in the future. Previously we were throwing a runtime_exception,
                // however it was particularly annoying during prototyping. Default constructing an object
                // might be the lesser evil, but tends to result in questions like, "why is my asset not loading?"
                // since we might have forgotten to load an asset into a handle, or mistyped a handle id. 
                if (!a)
                {
                    a = std::make_shared<polymer_unique_asset<T>>();
                    a->timestamp = system_time_ns();
                    a->assigned = false;
                    log::get()->import_log->warn("asset_handle type {} ({}) was default constructed", typeid(T).name(), name);
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
            handle->assigned = name.empty() || name == "empty" ? false : true;
            handle->timestamp = system_time_ns();

            #ifdef ASSET_DEBUG_SPAM
            log::get()->import_log->info("asset type {} with id {} was assigned", typeid(T).name(), name);
            #endif

            return handle->asset;
        }

        bool assigned() const
        {
            if (handle && handle->assigned) return true;

            // Search for it, but don't default construct
            auto itr = table.find(name);
            if (itr != table.end())
            {
                handle = itr->second;
                return handle->assigned;
            }
            return false;
        }

        // List will return all the asset_handles of type T.
        static std::vector<asset_handle> list()
        {
            std::vector<asset_handle> results;
            for (const auto & a : table)
            {
                results.push_back(asset_handle<T>(a.first, a.second));
            }
            return results;
        }

        static bool destroy(const std::string & asset_id)
        {
            auto iter = table.find(asset_id);
            if (iter != table.end())
            {
                #ifdef ASSET_DEBUG_SPAM
                log::get()->import_log->info("asset type {} with id {} was destroyed", typeid(T).name(), iter->first);
                #endif
                iter->second.reset();
                table.erase(iter);
                return true;
            }
            return false;
        }
    };

    template<class T>
    std::unordered_map<std::string, std::shared_ptr<polymer_unique_asset<T>>> asset_handle<T>::table;

} // end namespace polymer

#endif // end polymer_asset_handle_hpp

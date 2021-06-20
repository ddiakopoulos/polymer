#pragma once

#ifndef polymer_memory_pool_hpp
#define polymer_memory_pool_hpp

#include <stdint.h>
#include <vector>
#include <type_traits>
#include <limits>
#include <unordered_map>
#include <mutex>

namespace polymer
{
    // An extremely simple, thread-safe pool with heap allocations via std::vector
    class memory_pool
    {
        std::timed_mutex m;

        struct data_blob
        {
            bool acquired {false};
            std::vector<uint8_t> data;
        };

        std::unordered_map<uint8_t *, data_blob> storage_blobs;
        uint32_t bytes_per_blob {0};

     public:

        memory_pool(const uint32_t num_bytes, const uint32_t num_blobs)
             : bytes_per_blob(num_bytes)
        {
            std::lock_guard<std::timed_mutex> guard(m);

            for (uint32_t i = 0; i < num_blobs; ++i)
            {
                data_blob c;
                c.acquired = false;
                c.data.resize(bytes_per_blob);
                uint8_t * data_ptr = c.data.data();
                storage_blobs[data_ptr] = std::move(c);
            }
        }

        uint8_t * acquire()
        {
            std::lock_guard<std::timed_mutex> guard(m);

            // Find available blob
            for (auto & f : storage_blobs)
            {
                if (f.second.acquired == false)
                {
                    f.second.acquired = true;
                    return f.first;
                }
            }

            return nullptr;
        }

        void release(uint8_t * f, bool clear = false)
        {
            std::lock_guard<std::timed_mutex> guard(m);
            storage_blobs[f].acquired = false;
            if (clear) 
            {
                auto & blob = storage_blobs[f].data;
                std::fill(blob.begin(), blob.end(), 0);
            }
        }

        uint32_t free_slots()
        {
            std::lock_guard<std::timed_mutex> guard(m);
            uint32_t num_free_blobs = 0;
            for (const auto & f : storage_blobs)
                if (!f.second.acquired) num_free_blobs++;
            return num_free_blobs;
        }

        size_t total_slots() const { return storage_blobs.size(); }
        uint32_t bytes_per_slot() const { return bytes_per_blob; }
    };

} // end namespace polymer

#endif

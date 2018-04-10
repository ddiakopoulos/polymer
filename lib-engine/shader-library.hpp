#pragma once

#ifndef shader_library_hpp
#define shader_library_hpp

#include "gl-api.hpp"
#include "util.hpp"
#include "string_utils.hpp"
#include "gl-loaders.hpp"

#include <regex>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <atomic>

#include "../../../lib-engine/asset-handle-utils.hpp"

using namespace std::experimental::filesystem;
using namespace std::chrono;

namespace polymer 
{
    inline system_clock::time_point write_time(const std::string & file_path)
    {
        try { return last_write_time(path(file_path)); }
        catch (...) { return system_clock::time_point::min(); };
    }

    class gl_shader_monitor
    {
        std::unordered_map<std::string, std::shared_ptr<gl_shader_asset>> assets;
        std::string root_path;
        std::thread watch_thread;
        std::mutex watch_mutex;
        std::atomic<bool> watch_should_exit{ false };

        void walk_asset_dir();

    public:

        gl_shader_monitor(const std::string & asset_path);
        ~gl_shader_monitor();

        // Call this regularly on the gl thread
        void handle_recompile();

        // Watch vertex and fragment
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path);

        // Watch vertex and fragment with includes
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & include_path);

        // Watch vertex and fragment and geometry with includes
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & geom_path, const std::string & include_path);
    };
}

#endif shader_library_hpp
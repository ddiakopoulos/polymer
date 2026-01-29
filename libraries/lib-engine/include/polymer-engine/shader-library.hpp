#pragma once

#ifndef polymer_shader_library_hpp
#define polymer_shader_library_hpp

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-gfx-gl/gl-loaders.hpp"

#include "polymer-core/util/util.hpp"
#include "polymer-core/util/string-utils.hpp"

#include "polymer-engine/shader.hpp"

#include <regex>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <atomic>

using namespace std::filesystem;
using namespace std::chrono;

namespace polymer 
{

    class gl_shader_monitor
    {
        std::unordered_map<std::string, std::shared_ptr<gl_shader_asset>> assets;

        std::vector<std::string> search_paths;

        std::thread watch_thread;
        std::mutex watch_mutex;
        std::atomic<bool> watch_should_exit{ false };
        std::atomic<int> polling_thread_frequency{ 1000 };

        void walk_asset_dir();

    public:

        // This must be constructed on the gl thread
        gl_shader_monitor(const std::string & asset_path);
        ~gl_shader_monitor();

        void add_search_path(const std::string & path);

        // Call this regularly on the gl thread
        void handle_recompile(const uint32_t polling_thread_frequency_milliseconds = 1009);

        // Watch vertex and fragment (no #includes)
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path);

        // Watch vertex and fragment with #includes
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & include_path);

        // Watch vertex and fragment and geometry with #includes
        void watch(const std::string & name, const std::string & vert_path, const std::string & frag_path, const std::string & geom_path, const std::string & include_path);
    };
}

#endif // end polymer_shader_library_hpp
#pragma once

#ifndef polymer_shader_asset_hpp
#define polymer_shader_asset_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/util/string-utils.hpp"

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-gfx-gl/gl-loaders.hpp"

#include "polymer-engine/asset/asset-handle-utils.hpp"

#include <regex>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <atomic>

namespace polymer
{
    struct shader_variant
    {
        uint64_t hash;
        std::vector<std::string> defines;
        gl_shader shader;
        bool enabled(const std::string & define) { for (auto & d : defines) if (d == define) return true; return false; }
    };

    class gl_shader_asset
    {
        std::string name;
        std::string vertexPath, fragmentPath, geomPath, includePath;
        std::vector<std::string> includes;
        std::unordered_map<uint64_t, std::shared_ptr<shader_variant>> shaders;
        bool shouldRecompile{ true };
        int64_t writeTime{ 0 };
        friend class gl_shader_monitor;

    public:

        gl_shader_asset(const std::string & n, const std::string & v, const std::string & f, const std::string & g = "", const std::string & inc = "");
        gl_shader compile_variant(const std::vector<std::string> defines);
        std::shared_ptr<shader_variant> get_variant(const std::vector<std::string> defines = {});
        gl_shader & get(); // returns compiled shader, assumes no defines
        uint64_t hash(const std::vector<std::string> & defines);
        void recompile_all();
    };
}

#endif // end polymer_shader_asset_hpp

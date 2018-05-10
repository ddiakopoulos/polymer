#pragma once

#ifndef polymer_shader_asset_hpp
#define polymer_shader_asset_hpp

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



namespace polymer
{
    struct shader_variant
    {
        std::vector<std::string> defines; // eventually capture/store in material
        GlShader shader;
        bool enabled(const std::string & define) { for (auto & d : defines) if (d == define) return true; return false; }
    };

    class gl_shader_asset
    {
        std::string name;
        std::string vertexPath, fragmentPath, geomPath, includePath;
        std::vector<std::string> includes;
        std::unordered_map<uint64_t, std::shared_ptr<shader_variant>> shaders;
        bool shouldRecompile = false;
        int64_t writeTime = 0;
        friend class gl_shader_monitor;

    public:

        gl_shader_asset(const std::string & n, const std::string & v, const std::string & f, const std::string & g = "", const std::string & inc = "");
        GlShader compile_variant(const std::vector<std::string> defines);
        std::shared_ptr<shader_variant> get_variant(const std::vector<std::string> defines = {});
        GlShader & default(); // returns compiled shader, assumes no defines
        void recompile_all();
    };
}

#endif // end polymer_shader_asset_hpp

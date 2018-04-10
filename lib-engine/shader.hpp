#pragma once

#ifndef shader_monitor_h
#define shader_monitor_h

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

    // 32 bit Fowler–Noll–Vo Hash
    inline uint32_t hash_fnv1a(const std::string & str)
    {
        static const uint32_t fnv1aBase32 = 0x811C9DC5u;
        static const uint32_t fnv1aPrime32 = 0x01000193u;

        uint32_t result = fnv1aBase32;

        for (auto & c : str)
        {
            result ^= static_cast<uint32_t>(c);
            result *= fnv1aPrime32;
        }
        return result;
    }

    inline std::string preprocess_includes(const std::string & source, const std::string & includeSearchPath, std::vector<std::string> & includes, int depth)
    {
        if (depth > 4) throw std::runtime_error("exceeded max include recursion depth");

        static const std::regex re("^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*");
        std::stringstream input;
        std::stringstream output;

        input << source;

        size_t lineNumber = 1;
        std::smatch matches;
        std::string line;

        while (std::getline(input, line))
        {
            if (std::regex_search(line, matches, re))
            {
                std::string includeFile = matches[1];
                std::string includeString = read_file_text(includeSearchPath + "/" + includeFile);

                if (!includeFile.empty())
                {
                    includes.push_back(includeSearchPath + "/" + includeFile);
                    output << preprocess_includes(includeString, includeSearchPath, includes, depth++) << std::endl;
                }
            }
            else
            {
                output << "#line " << lineNumber << std::endl;
                output << line << std::endl;
            }
            ++lineNumber;
        }
        return output.str();
    }

    inline std::string preprocess_version(const std::string & source)
    {
        std::stringstream input;
        std::stringstream output;

        input << source;

        size_t lineNumber = 1;
        std::string line;
        std::string version;

        while (std::getline(input, line))
        {
            if (line.find("#version") != std::string::npos) version = line;
            else output << line << std::endl;
            ++lineNumber;
        }

        std::stringstream result;
        result << version << std::endl << output.str();
        return result.str();
    }

    inline GlShader preprocess(
        const std::string & vertexShader,
        const std::string & fragmentShader,
        const std::string & geomShader,
        const std::string & includeSearchPath,
        const std::vector<std::string> & defines,
        std::vector<std::string> & includes)
    {
        std::stringstream vertex;
        std::stringstream fragment;
        std::stringstream geom;

        for (const auto define : defines)
        {
            if (vertexShader.size()) vertex << "#define " << define << std::endl;
            if (fragmentShader.size()) fragment << "#define " << define << std::endl;
            if (geomShader.size()) geom << "#define " << define << std::endl;
        }

        if (vertexShader.size()) vertex << vertexShader;
        if (fragmentShader.size()) fragment << fragmentShader;
        if (geomShader.size()) geom << geomShader;

        if (geomShader.size())
        {
            return GlShader(
                preprocess_version(preprocess_includes(vertex.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(fragment.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(geom.str(), includeSearchPath, includes, 0)));
        }
        else
        {
            return GlShader(
                preprocess_version(preprocess_includes(vertex.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(fragment.str(), includeSearchPath, includes, 0)));
        }
    }

    inline GlComputeProgram preprocess_compute_defines(const std::string & computeShader, const std::vector<std::string> & defines)
    {
        std::stringstream compute;
        for (const auto define : defines) if (computeShader.size()) compute << "#define " << define << std::endl;
        if (computeShader.size()) compute << computeShader;
        return GlComputeProgram(preprocess_version(compute.str()));
    }

    struct shader_variant
    {
        std::vector<std::string> defines; // eventually capture/store in material
        GlShader shader;
        bool enabled(const std::string & define) { for (auto & d : defines) if (d == define) return true; return false; }
    };

    class gl_shader_asset
    {
        std::string name;

        std::string vertexPath;
        std::string fragmentPath;
        std::string geomPath;

        std::string includePath;
        std::vector<std::string> includes;

        std::unordered_map<uint64_t, std::shared_ptr<shader_variant>> shaders;

        bool shouldRecompile = false;
        int64_t writeTime = 0;

    public:

        gl_shader_asset(
            const std::string & n,
            const std::string & v,
            const std::string & f,
            const std::string & g = "",
            const std::string & inc = "") : name(n), vertexPath(v), fragmentPath(f), geomPath(g), includePath(inc) { }

        gl_shader_asset() = default;

        std::shared_ptr<shader_variant> get_variant(const std::vector<std::string> defines = {});

        void recompile_all();

        GlShader compile_variant(const std::vector<std::string> defines);
}

#endif

#pragma once

//@todo - This should probably be part of lib-engine! 

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

#include "../../../lib-engine/asset-defs.hpp"

using namespace std::experimental::filesystem;
using namespace std::chrono;

inline system_clock::time_point write_time(const std::string & file_path)
{
    try { return last_write_time(path(file_path));}
    catch (...) { return system_clock::time_point::min(); };
}

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

    // Store this in the `asset_handle` table...
    struct gl_shader_asset
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

        gl_shader_asset(
            const std::string & n,
            const std::string & v,
            const std::string & f,
            const std::string & g = "",
            const std::string & inc = "") : name(n), vertexPath(v), fragmentPath(f), geomPath(g), includePath(inc) { }

        gl_shader_asset() = default;

        std::shared_ptr<shader_variant> get_variant(const std::vector<std::string> defines = {})
        {
            //scoped_timer t("get - " + name);

            uint64_t sumOfHashes = 0;
            for (auto & define : defines) sumOfHashes += hash_fnv1a(define);

            auto itr = shaders.find(sumOfHashes);
            if (itr != shaders.end())
            {
                return itr->second;
            }


            auto newVariant = std::make_shared<shader_variant>();
            newVariant->shader = std::move(compile_variant(defines));
            newVariant->defines = defines;
            shaders[sumOfHashes] = newVariant;
            return newVariant;
        }

        void recompile_all()
        {
            // Compile at least the default variant with no includes defined... 
            if (shaders.empty())
            {
                auto newVariant = std::make_shared<shader_variant>();
                newVariant->shader = std::move(compile_variant({}));
                shaders[0] = newVariant;
            }

            for (auto & variant : shaders)
            {
                variant.second->shader = compile_variant(variant.second->defines);
            }
        }

        GlShader compile_variant(const std::vector<std::string> defines)
        {
            GlShader result;

            try
            {
                if (defines.size() > 0 || includePath.size() > 0)
                {
                    result = preprocess(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath), includePath, defines, includes);
                }
                else
                {
                    result = GlShader(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath));
                }
            }
            catch (const std::exception & e)
            {
                //@todo use logger
                std::cout << "Shader recompilation error: " << e.what() << std::endl;
            }

            return std::move(result);
        }
    };

    class gl_shader_monitor
    {

        void walk_root_directory(path root)
        {
            for (auto & entry : recursive_directory_iterator(root))
            {
                const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
                auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
                for (auto & chr : path) if (chr == '\\') chr = '/';

                for (auto & asset : assets)
                {
                    // Regular shader assets
                    if (path == asset.second->vertexPath || path == asset.second->fragmentPath || path == asset.second->geomPath)
                    {
                        auto writeTime = duration_cast<seconds>(write_time(path).time_since_epoch()).count();
                        if (writeTime > asset.second->writeTime)
                        {
                            asset.second->writeTime = writeTime;
                            asset.second->shouldRecompile = true;
                            //@todo use logger
                            std::cout << "Modified Shader: " << asset.second->vertexPath << std::endl;
                        }
                    }

                    // Each shader keeps a list of the files it includes. gl_shader_monitor watches a base path,
                    // so we should be able to recompile shaders dependent on common includes
                    for (auto & includePath : asset.second->includes)
                    {
                        if (get_filename_with_extension(path) == get_filename_with_extension(includePath))
                        {
                            auto writeTime = duration_cast<seconds>(write_time(path).time_since_epoch()).count();
                            if (writeTime > asset.second->writeTime)
                            {
                                asset.second->writeTime = writeTime;
                                asset.second->shouldRecompile = true;
                                //@todo use logger
                                std::cout << "Modified Include: " << includePath << std::endl;
                                break;
                            }
                        }
                    }

                }
            }
        }

        std::unordered_map<std::string, std::shared_ptr<gl_shader_asset>> assets;

        std::string root_path;
        std::thread watch_thread;
        std::mutex watch_mutex;
        std::atomic<bool> watch_should_exit{ false };

    public:

        gl_shader_monitor(const std::string & root_path) : root_path(root_path) 
        {
            watch_thread = std::thread([this, root_path]()
            {
                while (!watch_should_exit)
                {
                    try
                    {
                        std::lock_guard<std::mutex> guard(watch_mutex);
                        walk_root_directory(root_path);
                    }
                    catch (const std::exception & e)
                    {
                        //@todo use logger
                        std::cout << "Filesystem error: " << e.what() << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            });
        }

        ~gl_shader_monitor()
        {
            watch_should_exit = true;
            if (watch_thread.joinable()) watch_thread.join();
        }

        // Call this regularly on the gl thread
        void handle_recompile()
        {
            for (auto & asset : assets)
            {
                if (asset.second->shouldRecompile)
                {
                    std::lock_guard<std::mutex> guard(watch_mutex);

                    asset.second->recompile_all();
                    asset.second->shouldRecompile = false;
                }
            }
        }

        // Watch vertex and fragment
        void watch(
            const std::string & name,
            const std::string & vert_path,
            const std::string & frag_path)
        {
            auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path);
            assets[name] = asset;
            create_handle_for_asset(name.c_str(), std::move(asset));
        }

        // Watch vertex and fragment with includes
        void watch(
            const std::string & name,
            const std::string & vert_path,
            const std::string & frag_path,
            const std::string & include_path)
        {
            auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path, "", include_path);
            assets[name] = asset;
            create_handle_for_asset(name.c_str(), std::move(asset));
        }

        // Watch vertex and fragment and geometry with includes
        void watch(
            const std::string & name,
            const std::string & vert_path,
            const std::string & frag_path,
            const std::string & geom_path,
            const std::string & include_path)
        {
            auto asset = std::make_shared<gl_shader_asset>(name, vert_path, frag_path, geom_path, include_path);
            assets[name] = asset;
            create_handle_for_asset(name.c_str(), std::move(asset));
        }

        std::shared_ptr<gl_shader_asset> get_asset(const std::string & name)
        {
            return assets[name];
        }

    };

}

#endif

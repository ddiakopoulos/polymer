#pragma once

#ifndef polymer_string_utils_hpp
#define polymer_string_utils_hpp

#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <cctype>
#include <algorithm>

#include "polymer-core/util/util.hpp"

namespace polymer
{

    constexpr inline char get_platform_separator() 
    {
    #ifdef POLYMER_PLATFORM_WINDOWS
        return '\\';
    #else
        return '/';
    #endif
    }

    inline uint32_t replace_in_string(std::string & mutable_input, const std::string & look_for, const std::string & replace_with)
    {
        uint32_t occurances = 0;

        if (look_for.size() > 0)
        {
            size_t start = 0;    
            size_t find = std::string::npos;
            do
            {
                find = mutable_input.find(look_for, start);
                if (find != std::string::npos)
                {
                    mutable_input.replace(find, look_for.length(), replace_with);
                    start = find + replace_with.length();
                    occurances++;
                }
            }
            while (find != -1);
        }
        return occurances;
    }

    inline std::string to_lower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    inline void to_lower(std::string & str) 
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    }

    inline std::string to_upper(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    }

    inline void to_upper(std::string & str) 
    {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    }

    inline std::string ltrim(std::string str) 
    {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(),
            [](unsigned char c){ return isgraph(c); }));
        return str;
    }

    inline std::string rtrim(std::string str) 
    {
        str.erase(std::find_if(str.rbegin(), str.rend(),
            [](unsigned char c){ return isgraph(c); }).base(), str.end());
        return str;
    }

    inline std::string trim(std::string str)
    {
        return ltrim(rtrim(str));
    }

    inline void normalize_path(std::string & path)
    {
        constexpr char separator = get_platform_separator();
        replace_in_string(path, "//", "/");
        if (separator == '/') replace_in_string(path, "\\", "/");
        else if (separator == '\\') replace_in_string(path, "/", "\\");
        if (path.size() > 0 && path[path.size()-1] == separator) path.erase(path.begin()+(path.size()-1)); // the last char shouldn't be a separator 
    }

    inline bool starts_with(const std::string & str, const std::string & search)
    {
        return search.length() <= str.length() && std::equal(search.begin(), search.end(), str.begin());
    }

    inline std::vector<std::string> split(const std::string & s, const char delim) 
    {
        std::vector<std::string> list;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) 
            list.push_back(item);
        return list;
    }

    inline std::string replace_extension(const std::string & path, const std::string & extension)
    {
        std::string result = path;
        const size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) result.replace(pos, path.size() - pos, extension);
        else result += extension;
        return result;
    }

    // Does not include '.' -> "image.jpeg" returns "jpeg"
    inline std::string get_extension(const std::string & path)
    {
        auto found = path.find_last_of('.');
        if (found == std::string::npos) return {};
        else return path.substr(found + 1);
    }
    
    inline std::string get_filename_with_extension(const std::string & path)
    {
        std::string normalized_path = path;
        normalize_path(normalized_path);

        constexpr char separator = get_platform_separator();
        if (separator == '/') return normalized_path.substr(normalized_path.find_last_of("/") + 1);
        else if (separator == '\\') return normalized_path.substr(normalized_path.find_last_of("\\") + 1);
        return normalized_path;
    }
    
    inline std::string get_filename_without_extension(const std::string & path)
    {
        if (path.find_last_of(".") != std::string::npos && path.find_last_of("\\") != std::string::npos)
        {
            size_t end = path.find_last_of(".");
            size_t start = path.find_last_of("\\") + 1;
            return path.substr(start, end - start);
        }
        else if (path.find_last_of(".") != std::string::npos && path.find_last_of("/") != std::string::npos)
        {
            size_t end = path.find_last_of(".");
            size_t start = path.find_last_of("/") + 1;
            return path.substr(start, end - start);
        }
        return path;
    }
    
    inline std::string parent_directory_from_filepath(const std::string & path)
    {
        std::string normalized_path = path;
        normalize_path(normalized_path);

        if (path.find_last_of("\\") != std::string::npos)
        {
            size_t end = path.find_last_of("\\");
            return path.substr(0, end);
        }
        else if (path.find_last_of("/") != std::string::npos)
        {
            size_t end = path.find_last_of("/");
            return path.substr(0, end);
        }
        return path;
    }
    
}

#endif // polymer_string_utils_hpp

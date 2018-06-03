#pragma once

#ifndef string_utils_h
#define string_utils_h

#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <cctype>
#include <algorithm>

namespace polymer
{
    inline bool starts_with(const std::string & str, const std::string & search)
    {
        return search.length() <= str.length() && std::equal(search.begin(), search.end(), str.begin());
    }

    inline std::vector<std::string> split(const std::string & s, char delim) 
    {
        std::vector<std::string> list;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) 
            list.push_back(item);
        return list;
    }

    inline std::string get_extension(const std::string & path)
    {
        auto found = path.find_last_of('.');
        if (found == std::string::npos) return "";
        else return path.substr(found + 1);
    }
    
    inline std::string get_filename_with_extension(const std::string & path)
    {
        if (path.find_last_of("\\") != std::string::npos)
            return path.substr(path.find_last_of("\\") + 1);
        else if (path.find_last_of("/") != std::string::npos)
            return path.substr(path.find_last_of("/") + 1);
        return path;
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

#endif // string_utils_h

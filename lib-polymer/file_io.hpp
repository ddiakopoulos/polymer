#pragma once

#ifndef file_io_hpp
#define file_io_hpp

#include <exception>
#include <vector>
#include <string>
#include <fstream>
#include <streambuf>

namespace polymer
{
    
    inline std::vector<uint8_t> read_file_binary(const std::string & pathToFile)
    {
        std::ifstream file(pathToFile, std::ios::binary);
        std::vector<uint8_t> fileBufferBytes;

        if (file.is_open())
        {
            file.seekg(0, std::ios::end);
            size_t sizeBytes = file.tellg();
            file.seekg(0, std::ios::beg);
            fileBufferBytes.resize(sizeBytes);
            if (file.read((char*)fileBufferBytes.data(), sizeBytes)) return fileBufferBytes;
        }
        else throw std::runtime_error("could not open binary ifstream to path " + pathToFile);
        return fileBufferBytes;
    }

    inline void write_file_binary(const std::string & pathToFile, std::vector<uint8_t> & data)
    {
        std::ofstream file(pathToFile, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
        if (file.is_open())
        {
            file.write((char*)data.data(), data.size());
        }
        else throw std::runtime_error("could not open binary ofstream to path " + pathToFile);
    }
    
    inline std::string read_file_text(const std::string & pathToFile)
    {
        if (pathToFile.empty()) return {}; // no-op if path is empty

        std::ifstream file(pathToFile.c_str());
        std::string fileBufferAsString;
        if (file.is_open())
        {
            fileBufferAsString = { (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>() };
        }
        else throw std::runtime_error("could not open ascii ifstream to path " + pathToFile);
        return fileBufferAsString;
    }

    inline void write_file_text(const std::string & pathToFile, const std::string & output)
    {
        if (pathToFile.empty()) return; // no-op if path is empty

        std::ofstream file(pathToFile);
        if (file.is_open())
        {
            file << output;
        }
        else throw std::runtime_error("could not open ascii ofstream to path " + pathToFile);
    }

}

#endif // file_io_hpp

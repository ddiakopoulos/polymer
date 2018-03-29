#ifndef file_io_h
#define file_io_h

#include <exception>
#include <vector>
#include <string>
#include <fstream>
#include <streambuf>

namespace polymer
{
    
    inline std::vector<uint8_t> read_file_binary(const std::string pathToFile)
    {
        FILE * f = fopen(pathToFile.c_str(), "rb");
        
        if (!f) throw std::runtime_error("file not found");
        
        fseek(f, 0, SEEK_END);
        size_t lengthInBytes = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        std::vector<uint8_t> fileBuffer(lengthInBytes);
        
        size_t elementsRead = fread(fileBuffer.data(), 1, lengthInBytes, f);
        
        if (elementsRead == 0 || fileBuffer.size() < 4) throw std::runtime_error("error reading file or file too small");
        
        fclose(f);
        return fileBuffer;
    }
    
    inline std::string read_file_text(const std::string & pathToFile)
    {
        std::ifstream t(pathToFile.c_str());
        std::string str ((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        return str;
    }

    inline void write_file_text(const std::string & fileName, const std::string & output)
    {
        std::ofstream t(fileName);
        t << output;
        t.close();
    }

}

#endif // file_io_h

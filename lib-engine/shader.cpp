#include "shader.hpp"

std::string preprocess_includes(const std::string & source, const std::string & includeSearchPath, std::vector<std::string> & includes, int depth)
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

std::string preprocess_version(const std::string & source)
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
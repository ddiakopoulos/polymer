#include "shader.hpp"

using namespace polymer;

////////////////////////////////////////
//   Shader Preprocessing Functions   //
////////////////////////////////////////

std::string process_includes_recursive(const std::string & source, const std::string & includeSearchPath, std::vector<std::string> & includes, int depth)
{
    if (depth > 2) throw std::runtime_error("exceeded max include recursion depth");

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
                output << process_includes_recursive(includeString, includeSearchPath, includes, depth++) << std::endl;
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

gl_shader preprocess(const std::string & vertexShader,
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
        return gl_shader(
            preprocess_version(process_includes_recursive(vertex.str(), includeSearchPath, includes, 0)),
            preprocess_version(process_includes_recursive(fragment.str(), includeSearchPath, includes, 0)),
            preprocess_version(process_includes_recursive(geom.str(), includeSearchPath, includes, 0)));
    }
    else
    {
        return gl_shader(
            preprocess_version(process_includes_recursive(vertex.str(), includeSearchPath, includes, 0)),
            preprocess_version(process_includes_recursive(fragment.str(), includeSearchPath, includes, 0)));
    }
}

///////////////////////////////////////
//   gl_shader_asset implementation  //
///////////////////////////////////////

gl_shader_asset::gl_shader_asset(const std::string & n, const std::string & v, const std::string & f, const std::string & g, const std::string & inc) 
    : name(n), vertexPath(v), fragmentPath(f), geomPath(g), includePath(inc) {}

uint64_t gl_shader_asset::hash(const std::vector<std::string> & defines)
{
    uint64_t sumOfHashes = 0;
    for (auto & define : defines) sumOfHashes += poly_hash_fnv1a(define);
    return sumOfHashes;
}

std::shared_ptr<shader_variant> gl_shader_asset::get_variant(const std::vector<std::string> defines)
{
    // Hash the defines
    uint64_t theHash = hash(defines);

    // Lookup if exists
    auto itr = shaders.find(theHash);
    if (itr != shaders.end()) return itr->second;

    // Create if not
    auto newVariant = std::make_shared<shader_variant>();
    newVariant->shader = std::move(compile_variant(defines));
    newVariant->defines = defines;
    newVariant->hash = theHash;
    shaders[theHash] = newVariant;
    return newVariant;
}

gl_shader & gl_shader_asset::get()
{
    std::shared_ptr<shader_variant> theDefault;
    if (shaders.size() == 0) theDefault = get_variant();
    else theDefault = shaders[0];
    return theDefault->shader;
}

void gl_shader_asset::recompile_all()
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

gl_shader gl_shader_asset::compile_variant(const std::vector<std::string> defines)
{
    gl_shader variant;

    try
    {
        if (defines.size() > 0 || includePath.size() > 0)
        {
            variant = preprocess(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath), includePath, defines, includes);
        }
        else
        {
            variant = gl_shader(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath));
        }
    }
    catch (const std::exception & e)
    {
        //@todo use logger
        std::cout << "Shader recompilation error: " << e.what() << std::endl;
    }

    return std::move(variant);
}
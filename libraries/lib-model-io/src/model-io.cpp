#include <assert.h>
#include <fstream>
#include <ostream>

#include "polymer-model-io/model-io.hpp"
#include "polymer-model-io/model-io-util.hpp"

#include "polymer-core/util/file-io.hpp"
#include "polymer-core/util/string-utils.hpp"

#define TINYPLY_IMPLEMENTATION
#include "tinyply/tinyply.h"
#include "tinyobj/tiny_obj_loader.h"
#include "meshoptimizer/meshoptimizer.h"

using namespace polymer;
using namespace tinyply;

std::unordered_map<std::string, runtime_mesh> polymer::import_model(const std::string & path)
{
    std::unordered_map<std::string, runtime_mesh> models;

    std::string ext = get_extension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "obj")
    {
        auto asset = import_obj_model(path);
        for (auto & a : asset) models[a.first] = a.second;
    }
    else if (ext == "ply")
    {
        auto asset = import_ply_model(path);
        for (auto & a : asset) models[a.first] = a.second;
    }
    else if (ext == "mesh")
    {
        auto m = import_polymer_binary_model(path);
        models[get_filename_without_extension(path)] = m;
    }
    else
    {
        throw std::runtime_error("cannot import model format");
    }

    // Compute tangents and bitangents...
    for (auto & m : models)
    {
        compute_tangents(m.second);
    }

    return models;
}

std::unordered_map<std::string, runtime_mesh> polymer::import_obj_model(const std::string & path)
{
    std::unordered_map<std::string, runtime_mesh> meshes;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string parentDir = parent_directory_from_filepath(path) + "/";

    std::string err;
    bool status = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str(), parentDir.c_str());

    // if (status && !err.empty()) std::cerr << "tinyobj warning: " << err << std::endl;

    // Append `default` material
    materials.push_back(tinyobj::material_t());

    // std::cout << "# of shapes    : " << shapes.size() << std::endl;
    // std::cout << "# of materials : " << materials.size() << std::endl;

    // Parse tinyobj data into geometry struct
    for (unsigned int i = 0; i < shapes.size(); i++)
    {
        tinyobj::shape_t * shape = &shapes[i];
        tinyobj::mesh_t * mesh = &shapes[i].mesh;

        runtime_mesh & g = meshes[shape->name];

        // std::cout << "Submesh Name:  " << shape->name << std::endl;
        // std::cout << "Num Indices:   " << mesh->indices.size() << std::endl;
        // std::cout << "Num TexCoords: " << attrib.texcoords.size() << std::endl;

        size_t indexOffset = 0;

        // de-duplicate vertices
        unordered_map_generator<unique_vertex, uint32_t>::Type uniqueVertexMap;
        bool shouldGenerateNormals = false;

        for (size_t f = 0; f < mesh->num_face_vertices.size(); f++)
        {
            assert(mesh->num_face_vertices[f] == 3);

            uint3 indices;
            for (int v = 0; v < 3; v++)
            {
                const tinyobj::index_t idx = mesh->indices[indexOffset + v];

                unique_vertex vertex;
                vertex.position = { attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2] };
                if (attrib.normals.size()) vertex.normal = { attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2] };
                else shouldGenerateNormals = true;

                if (idx.texcoord_index != -1) vertex.texcoord = { attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1] };

                auto it = uniqueVertexMap.find(vertex);
                if (it != uniqueVertexMap.end())
                {
                    indices[v] = it->second; // found duplicated vertex
                }
                else
                {
                    // we haven't run into this vertex yet
                    uint32_t index = uint32_t(g.vertices.size());

                    uniqueVertexMap[vertex] = index;
                    indices[v] = index;

                    g.vertices.push_back(vertex.position);
                    if (attrib.normals.size()) g.normals.push_back(vertex.normal);
                    if (idx.texcoord_index != -1) g.texcoord0.push_back(vertex.texcoord);
                }
            }

            if (mesh->material_ids[f] > 0) g.material.push_back(mesh->material_ids[f]);
            g.faces.push_back(indices);
            indexOffset += 3;
        }

        // Optionally generate normals if the mesh was provided without them
        if (shouldGenerateNormals) compute_normals(g);
    }

    return meshes;
}

std::unordered_map<std::string, runtime_mesh> polymer::import_ply_model(const std::string & path)
{
    std::unordered_map<std::string, runtime_mesh> result;

    try
    {
        std::ifstream ss(path, std::ios::binary);
        if (ss.fail()) throw std::runtime_error("failed to open " + path);

        PlyFile file;
        file.parse_header(ss);

        std::shared_ptr<PlyData> vertices, normals, colors, faces, texcoords;

        try { vertices = file.request_properties_from_element("vertex", { "x", "y", "z" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { normals = file.request_properties_from_element("vertex", { "nx", "ny", "nz" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { colors = file.request_properties_from_element("vertex", { "red", "green", "blue", "alpha" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { texcoords = file.request_properties_from_element("vertex", { "u", "v" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { faces = file.request_properties_from_element("face", { "vertex_indices" }, 3); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        file.read(ss);

        const auto ply_file_name = get_filename_without_extension(path);
        runtime_mesh & g = result[ply_file_name];

        g.vertices.resize(vertices->count);
        std::memcpy(g.vertices.data(), vertices->buffer.get(), vertices->buffer.size_bytes());

        if (normals)
        {
            g.normals.resize(normals->count);
            std::memcpy(g.normals.data(), normals->buffer.get(), normals->buffer.size_bytes());
        }

        if (colors)
        {
            g.colors.resize(colors->count);
            std::memcpy(g.colors.data(), colors->buffer.get(), colors->buffer.size_bytes());
        }

        if (texcoords)
        {
            g.texcoord0.resize(texcoords->count);
            std::memcpy(g.texcoord0.data(), texcoords->buffer.get(), texcoords->buffer.size_bytes());
        }

        if (faces)
        {
            g.faces.resize(faces->count);
            std::memcpy(g.faces.data(), faces->buffer.get(), faces->buffer.size_bytes());
        }

    }
    catch (const std::exception & e)
    {
        std::cerr << "fatal tinyply exception: " << e.what() << std::endl;
    }

    return result;
}

void polymer::optimize_model(runtime_mesh & input)
{
    // @todo - implement mesh-optimizer workflow
}

runtime_mesh polymer::import_polymer_binary_model(const std::string & path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) throw std::runtime_error("couldn't open");

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    runtime_mesh_binary_header h;
    file.read((char*)&h, sizeof(runtime_mesh_binary_header));

    assert(h.headerVersion == runtime_mesh_binary_version);
    if (h.compressionVersion > 0) assert(h.compressionVersion == runtime_mesh_compression_version);

    runtime_mesh mesh;

    mesh.vertices.resize(h.verticesBytes / sizeof(float3));
    mesh.normals.resize(h.normalsBytes / sizeof(float3));
    mesh.colors.resize(h.colorsBytes / sizeof(float3));
    mesh.texcoord0.resize(h.texcoord0Bytes / sizeof(float2));
    mesh.texcoord1.resize(h.texcoord1Bytes / sizeof(float2));
    mesh.tangents.resize(h.tangentsBytes / sizeof(float3));
    mesh.bitangents.resize(h.bitangentsBytes / sizeof(float3));
    mesh.faces.resize(h.facesBytes / sizeof(uint3));
    mesh.material.resize(h.materialsBytes / sizeof(uint32_t));

    file.read((char*)mesh.vertices.data(), h.verticesBytes);
    file.read((char*)mesh.normals.data(), h.normalsBytes);
    file.read((char*)mesh.colors.data(), h.colorsBytes);
    file.read((char*)mesh.texcoord0.data(), h.texcoord0Bytes);
    file.read((char*)mesh.texcoord1.data(), h.texcoord1Bytes);
    file.read((char*)mesh.tangents.data(), h.tangentsBytes);
    file.read((char*)mesh.bitangents.data(), h.bitangentsBytes);
    file.read((char*)mesh.faces.data(), h.facesBytes);
    file.read((char*)mesh.material.data(), h.materialsBytes);

    return mesh;
}

void polymer::export_polymer_binary_model(const std::string & path, runtime_mesh & mesh, bool compressed)
{
    auto file = std::ofstream(path, std::ios::out | std::ios::binary);

    runtime_mesh_binary_header header;
    header.headerVersion = runtime_mesh_binary_version;
    header.compressionVersion = (compressed) ? runtime_mesh_compression_version : 0;
    header.verticesBytes = (uint32_t)mesh.vertices.size() * sizeof(float3);
    header.normalsBytes = (uint32_t)mesh.normals.size() * sizeof(float3);
    header.colorsBytes = (uint32_t)mesh.colors.size() * sizeof(float3);
    header.texcoord0Bytes = (uint32_t)mesh.texcoord0.size() * sizeof(float2);
    header.texcoord1Bytes = (uint32_t)mesh.texcoord1.size() * sizeof(float2);
    header.tangentsBytes = (uint32_t)mesh.tangents.size() * sizeof(float3);
    header.bitangentsBytes = (uint32_t)mesh.bitangents.size() * sizeof(float3);
    header.facesBytes = (uint32_t)mesh.faces.size() * sizeof(uint3);
    header.materialsBytes = (uint32_t)mesh.material.size() * sizeof(uint32_t);

    file.write(reinterpret_cast<char*>(&header), sizeof(runtime_mesh_binary_header));
    file.write(reinterpret_cast<char*>(mesh.vertices.data()), header.verticesBytes);
    file.write(reinterpret_cast<char*>(mesh.normals.data()), header.normalsBytes);
    file.write(reinterpret_cast<char*>(mesh.colors.data()), header.colorsBytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord0.data()), header.texcoord0Bytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord1.data()), header.texcoord1Bytes);
    file.write(reinterpret_cast<char*>(mesh.tangents.data()), header.tangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.bitangents.data()), header.bitangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.faces.data()), header.facesBytes);
    file.write(reinterpret_cast<char*>(mesh.material.data()), header.materialsBytes);

    file.close();
}

void export_obj_data(std::ofstream & file, runtime_mesh & mesh)
{
    file << "# vertices\n";
    for (auto & v : mesh.vertices) file << "v " << std::fixed << v.x << " " << std::fixed << v.y << " " << std::fixed << v.z << std::endl;

    float3 normalSum{ 0.f };
    float2 texcoordSum{ 0.f };
    for (auto v : mesh.normals) normalSum += v;
    for (auto v : mesh.texcoord0) texcoordSum += v;

    if (normalSum > float3(0.f)) for (auto & v : mesh.normals) file << "vn " << std::fixed << v.x << " " << std::fixed << v.y << " " << std::fixed << v.z << std::endl;
    if (texcoordSum > float2(0.f)) for (auto & v : mesh.texcoord0) file << "vt " << std::fixed << v.x << " " << std::fixed << v.y << std::endl;

    file << "# faces\n";
    for (auto & v : mesh.faces) file << "f " << v.x + 1 << " " << v.y + 1 << " " << v.z + 1 << std::endl;
}

bool polymer::export_obj_model(const std::string & name, const std::string & filename, runtime_mesh & mesh)
{
    std::ofstream file(filename);

    if (!file.is_open())  return false;

    file.precision(3);
    file << "# OBJ file created by Polymer\n";
    file << "o " << name << "\n";

    export_obj_data(file, mesh);

    file.close();

    return true;
}

bool polymer::export_obj_multi_model(const std::vector<std::string> & names, const std::string & filename, std::vector<runtime_mesh *> & meshes)
{
    assert(names.size() == meshes.size());

    std::ofstream file(filename);

    if (!file.is_open())  return false;

    file.precision(3);
    file << "# OBJ file created by Polymer\n";

    for (int i = 0; i < meshes.size(); ++i)
    {
        auto & mesh = meshes[i];
        auto & name = names[i];
        file << "o " << name << "\n";
        export_obj_data(file, *mesh);
    }

    file.close();

    return true;
}

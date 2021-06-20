#pragma once

#ifndef polymer_model_io_hpp
#define polymer_model_io_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/geometry.hpp"

#include <unordered_map>

namespace polymer
{

    struct animation_keyframe
    {
        uint32_t key = 0;
        float4 rotation = float4(0, 0, 0, 1);
        float3 translation = float3(0, 0, 0);
        float3 scale = float3(1, 1, 1);
    };

    struct animation_track
    {
        uint32_t boneIndex = 0;
        uint32_t keyframeCount = 0;
        std::vector<std::shared_ptr<animation_keyframe>> keyframes;
    };

    struct skeletal_animation
    {
        uint32_t total_frames() const { return endFrame - startFrame; }
        float total_time(float fps = 24.0f) const { return (endFrame - startFrame) / fps; }
        std::string name;
        uint32_t startFrame = 0xffffffff;
        uint32_t endFrame = 0;
        uint32_t trackCount = 0;
        std::vector<std::shared_ptr<animation_track>> tracks;
    };

    struct bone
    {
        std::string name;
        uint32_t parentIndex;
        float4x4 initialPose;
        float4x4 bindPose;
    };

    struct runtime_skinned_mesh : public runtime_mesh
    {
        std::vector<bone> bones;
        std::vector<int4> boneIndices;
        std::vector<float4> boneWeights;
    };

    #define runtime_mesh_binary_version 1
    #define runtime_mesh_compression_version 1

    #pragma pack(push, 1)
    struct runtime_mesh_binary_header
    {
        uint32_t headerVersion{ runtime_mesh_binary_version };
        uint32_t compressionVersion{ runtime_mesh_compression_version };
        uint32_t verticesBytes{ 0 };
        uint32_t normalsBytes{ 0 };
        uint32_t colorsBytes{ 0 };
        uint32_t texcoord0Bytes{ 0 };
        uint32_t texcoord1Bytes{ 0 };
        uint32_t tangentsBytes{ 0 };
        uint32_t bitangentsBytes{ 0 };
        uint32_t facesBytes{ 0 };
        uint32_t materialsBytes{ 0 };
    };
    #pragma pack(pop)

    ///////////////////////
    //   File Format IO  //
    ///////////////////////

    // Polymer's own runtime-optimized *.mesh file format
    runtime_mesh import_polymer_binary_model(const std::string & path);
    void export_polymer_binary_model(const std::string & path, runtime_mesh & mesh, bool compressed = false);

    // Load an FBX model, assuming the path points to a valid *.fbx
    std::unordered_map<std::string, runtime_mesh> import_fbx_model(const std::string & path);

    // Load an OBJ model, assuming the path points to a valid *.obj
    std::unordered_map<std::string, runtime_mesh> import_obj_model(const std::string & path);

    // Load a PLY model, assuming the path points to a valid *.ply
    std::unordered_map<std::string, runtime_mesh> import_ply_model(const std::string & path);

    // Convenience function that checks extension for *.fbx, *.obj, *.ply, or *.mesh
    std::unordered_map<std::string, runtime_mesh> import_model(const std::string & path);

    bool export_obj_model(const std::string & name, const std::string & filename, runtime_mesh & mesh);
    bool export_obj_multi_model(const std::vector<std::string> & names, const std::string & filename, std::vector<runtime_mesh *> & meshes);

    // Currently a no-op
    void optimize_model(runtime_mesh & input);

} // end namespace polymer

#endif // end polymer_model_io_hpp

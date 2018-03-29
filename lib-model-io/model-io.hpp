#pragma once

#ifndef runtime_mesh_hpp
#define runtime_mesh_hpp

#include "math-core.hpp"
#include "string_utils.hpp"
#include "util.hpp"

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>

using namespace polymer;

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

struct runtime_mesh
{
    std::vector<float3> vertices;
    std::vector<float3> normals;
    std::vector<float4> colors;
    std::vector<float2> texcoord0;
    std::vector<float2> texcoord1;
    std::vector<float3> tangents;
    std::vector<float3> bitangents;
    std::vector<uint3> faces;
    std::vector<uint32_t> material;
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

///////////////////////
//   File Format IO  //
///////////////////////

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

void optimize_model(runtime_mesh & input);
runtime_mesh import_mesh_binary(const std::string & path);
void export_mesh_binary(const std::string & path, runtime_mesh & mesh, bool compressed = false);

std::map<std::string, runtime_mesh> import_fbx_model(const std::string & path);
std::map<std::string, runtime_mesh> import_obj_model(const std::string & path);
std::map<std::string, runtime_mesh> import_model(const std::string & path);

#endif // end runtime_mesh_hpp

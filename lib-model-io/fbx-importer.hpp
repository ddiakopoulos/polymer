#pragma once

#ifdef SYSTEM_HAS_FBX_SDK
    #define USING_FBX 1
#else
    #define USING_FBX 0
#endif

#if (USING_FBX == 1)

#ifndef fbx_importer_hpp
#define fbx_importer_hpp

#ifdef _DEBUG
    #pragma comment(lib, "Debug/libfbxsdk.lib")
#else
    #pragma comment(lib, "Release/libfbxsdk.lib")
#endif

#include "model-io.hpp"
#include "model-io-util.hpp"
#include <vector>
#include <memory>

#include <fbxsdk/fbxsdk_nsbegin.h>
class FbxLayerElementNormal;
class FbxLayerElementVertexColor;
class FbxLayerElementUV;
class FbxMesh;
class FbxNode;
class FbxVector4;
class FbxScene;
class FbxAnimEvaluator;
#include <fbxsdk/fbxsdk_nsend.h>

struct fbx_container
{
    std::map<std::string, runtime_skinned_mesh> meshes;
    std::map<std::string, skeletal_animation> animations;
};

void gather_meshes(fbx_container & file, fbxsdk::FbxNode * node);

fbx_container import_fbx_file(const std::string & file);

#endif // end fbx_importer_hpp

#endif // end SYSTEM_HAS_FBX_SDK

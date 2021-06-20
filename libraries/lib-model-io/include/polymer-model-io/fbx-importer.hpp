#pragma once

#include "polymer-model-io/model-io.hpp"
#include "polymer-model-io/model-io-util.hpp"

#include <vector>
#include <memory>

#ifdef SYSTEM_HAS_FBX_SDK

#ifndef fbx_importer_hpp
#define fbx_importer_hpp

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
#include <map>

namespace polymer
{
    struct fbx_container
    {
        std::map<std::string, runtime_skinned_mesh> meshes;
        std::map<std::string, skeletal_animation> animations;
    };

    void gather_meshes(fbx_container & file, fbxsdk::FbxNode * node);

    fbx_container import_fbx_file(const std::string & file);
}

#endif // end fbx_importer_hpp

#endif // end SYSTEM_HAS_FBX_SDK

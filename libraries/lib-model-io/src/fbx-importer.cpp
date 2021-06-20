#include "polymer-model-io/fbx-importer.hpp"

using namespace polymer;

#ifdef SYSTEM_HAS_FBX_SDK

#include <fbxsdk.h>
#include <assert.h>
#include <map>
#include "util.hpp"

float2 to_linalg(const fbxsdk::FbxDouble2 & v) { return float2(double2(v[0], v[1])); }
float4 to_linalg(const fbxsdk::FbxDouble4 & v) { return float4(double4(v[0], v[1], v[2], v[3])); }

// Get the value of a geometry element for a triangle vertex
template <typename TGeometryElement, typename TValue>
inline TValue get_vertex_element(TGeometryElement * pElement, int iPoint, int iTriangle, int iVertex, TValue defaultValue) 
{
    if (!pElement || pElement->GetMappingMode() == fbxsdk::FbxGeometryElement::eNone) return defaultValue;
    int index = 0;
    if (pElement->GetMappingMode() == fbxsdk::FbxGeometryElement::eByControlPoint) index = iPoint;
    else if (pElement->GetMappingMode() == fbxsdk::FbxGeometryElement::eByPolygon) index = iTriangle;
    else if (pElement->GetMappingMode() == fbxsdk::FbxGeometryElement::eByPolygonVertex) index = iTriangle * 3 + iVertex;
    if (pElement->GetReferenceMode() != fbxsdk::FbxGeometryElement::eDirect) index = pElement->GetIndexArray().GetAt(index);
    return pElement->GetDirectArray().GetAt(index);
}

void polymer::gather_meshes(fbx_container & file, fbxsdk::FbxNode * node)
{
    auto * attrib = node->GetNodeAttribute();

    if (attrib)
    {
        std::vector<fbxsdk::FbxNodeAttribute::EType> acceptedAttributes = {
            fbxsdk::FbxNodeAttribute::eMesh,
            fbxsdk::FbxNodeAttribute::eNurbs,
            fbxsdk::FbxNodeAttribute::eNurbsSurface,
            fbxsdk::FbxNodeAttribute::eTrimNurbsSurface,
            fbxsdk::FbxNodeAttribute::ePatch };

        bool hasAcceptableAttribute = false;
        for (auto & att : acceptedAttributes)
        {
            if (attrib->GetAttributeType() == att)
            {
                hasAcceptableAttribute = true;
                break;
            }
        }

        if (hasAcceptableAttribute)
        {
            auto & mesh = file.meshes[node->GetName()];
            auto * fbxMesh = fbxsdk::FbxCast<fbxsdk::FbxMesh>(attrib);

            auto points = fbxMesh->GetControlPoints();
            if (!points) throw std::runtime_error("no control points");

            auto pUV1 = fbxMesh->GetElementUV(0);
            if (pUV1 && pUV1->GetMappingMode() != fbxsdk::FbxGeometryElement::eByPolygonVertex) throw std::runtime_error("UV1 set must be mapped by eByPolygonVertex");

            auto pUV2 = fbxMesh->GetElementUV(1);
            if (pUV2 && pUV2->GetMappingMode() != fbxsdk::FbxGeometryElement::eByPolygonVertex) throw std::runtime_error("UV2 set must be mapped by eByPolygonVertex");

            const fbxsdk::FbxGeometryElementVertexColor * pVertexColors = fbxMesh->GetElementVertexColor(0);
            const fbxsdk::FbxGeometryElementNormal * pNormals = fbxMesh->GetElementNormal(0);

            int materialCount = node->GetMaterialCount();
            std::function<uint32_t(uint32_t triangleIndex)> getMaterialIndex = [](uint32_t triangleIndex) { return 0; };

            // Load materials if available
            if (materialCount > 0)
            {
                const fbxsdk::FbxLayerElementMaterial * pPolygonMaterials = fbxMesh->GetElementMaterial();
                assert(pPolygonMaterials != nullptr);
                assert(pPolygonMaterials->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndex || pPolygonMaterials->GetReferenceMode() == fbxsdk::FbxGeometryElement::eIndexToDirect);

                fbxsdk::FbxGeometryElement::EMappingMode mappingMode = pPolygonMaterials->GetMappingMode();

                getMaterialIndex = [pPolygonMaterials, mappingMode, materialCount](uint32_t triangleIndex) 
                {
                    int lookupIndex = 0;
                    switch (mappingMode) 
                    {
                    case fbxsdk::FbxGeometryElement::eByPolygon: lookupIndex = triangleIndex; break;
                    case fbxsdk::FbxGeometryElement::eAllSame: lookupIndex = 0; break;
                    default: assert(false); break;
                    }

                    int materialIndex = pPolygonMaterials->mIndexArray->GetAt(lookupIndex);
                    assert(materialIndex >= 0 && materialIndex < materialCount);
                    return uint32_t(materialIndex);
                };
            }

            // de-duplicate vertices
            unordered_map_generator<unique_vertex, uint32_t>::Type uniqueVertexMap;

            uint32_t numTriangles = uint32_t(fbxMesh->GetPolygonCount());
            mesh.vertices.reserve(numTriangles * 3);
            mesh.faces.reserve(numTriangles);

            for (uint32_t t = 0; t < numTriangles; t++) 
            {
                uint3 indices;
                for (uint32_t v = 0; v < 3; v++) 
                {
                    int controlPointIndex = fbxMesh->GetPolygonVertex(t, v);

                    fbxsdk::FbxVector4 point = fbxMesh->GetControlPointAt(controlPointIndex);
                    fbxsdk::FbxVector4 normal = get_vertex_element(pNormals, controlPointIndex, t, v, fbxsdk::FbxVector4(0, 0, 0, 0));
                    fbxsdk::FbxVector2 texcoord1 = get_vertex_element(pUV1, controlPointIndex, t, v, fbxsdk::FbxVector2(0, 1));
                    fbxsdk::FbxVector2 texcoord2 = get_vertex_element(pUV2, controlPointIndex, t, v, fbxsdk::FbxVector2(0, 1));
                    fbxsdk::FbxColor vertexColor = get_vertex_element(pVertexColors, controlPointIndex, t, v, fbxsdk::FbxColor(0, 0, 0, 0));

                    unique_vertex vertex;
                    vertex.position = to_linalg(point).xyz;
                    vertex.normal = to_linalg(normal).xyz;
                    vertex.texcoord = to_linalg(texcoord1);

                    auto it = uniqueVertexMap.find(vertex);
                    if (it != uniqueVertexMap.end())
                    {
                        indices[v] = it->second; // found duplicated vertex
                    }
                    else
                    {
                        // we haven't run into this vertex yet
                        uint32_t index = uint32_t(mesh.vertices.size());

                        uniqueVertexMap[vertex] = index;
                        indices[v] = index;

                        mesh.vertices.push_back(vertex.position);
                        mesh.normals.push_back(vertex.normal);
                        mesh.texcoord0.push_back(vertex.texcoord);
                        mesh.texcoord1.push_back(to_linalg(texcoord2));
                        mesh.colors.push_back({ (float) vertexColor.mRed, (float) vertexColor.mGreen, (float) vertexColor.mBlue, (float) vertexColor.mAlpha });
                    }

                }

                mesh.material.push_back(getMaterialIndex(t));
                mesh.faces.push_back(indices);
            }
        }
    }

    // Recurse
    for (int i = 0; i < node->GetChildCount(); i++)
    {
        gather_meshes(file, node->GetChild(i));
    }
}

////////////////////////
//    FBX Importing   //
////////////////////////

fbxsdk::FbxScene * import_scene(const std::string & file, fbxsdk::FbxManager * mgr)
{
    auto filename = file.c_str();

    fbxsdk::FbxImporter * lImporter = fbxsdk::FbxImporter::Create(mgr, "");

    if (!lImporter->Initialize(filename, -1, mgr->GetIOSettings()))
    {
        std::cerr << "Call to FbxImporter::Initialize() failed... " << std::endl;
        std::cerr << "Please check the file path: " << filename << std::endl;
        return nullptr;
    }

    int lFileMajor, lFileMinor, lFileRevision;
    int lSDKMajor, lSDKMinor, lSDKRevision;

    fbxsdk::FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);
    lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

    //std::cout << "FBX SDK Version - Major: " << lSDKMajor << ", Minor: " << lSDKMinor << ", rev: " << lSDKRevision << std::endl;
    //std::cout << "File Version - Major: " << lFileMajor << ", Minor: " << lFileMinor << ", rev: " << lFileRevision << std::endl;

    fbxsdk::FbxScene * scene = fbxsdk::FbxScene::Create(mgr, "default");

    lImporter->Import(scene);

    if (scene->GetGlobalSettings().GetSystemUnit() != fbxsdk::FbxSystemUnit::m)
    {
        fbxsdk::FbxSystemUnit::m.ConvertScene(scene);
    }

    fbxsdk::FbxAxisSystem axisSystem(fbxsdk::FbxAxisSystem::eOpenGL);
    axisSystem.ConvertScene(scene);

    fbxsdk::FbxGeometryConverter geomConverter(mgr);
    geomConverter.Triangulate(scene, true);

    lImporter->Destroy();

    return scene;
}

fbx_container polymer::import_fbx_file(const std::string & path)
{
    fbx_container container;

    fbxsdk::FbxManager * manager = fbxsdk::FbxManager::Create();
    manager->SetIOSettings(fbxsdk::FbxIOSettings::Create(manager, IOSROOT));

    fbxsdk::FbxScene * scene = import_scene(path, manager);
    if (!scene) throw std::runtime_error("failed to import scene");

    fbxsdk::FbxNode * rootNode = scene->GetRootNode();

    // Recursively extract objects from the root node
    gather_meshes(container, rootNode);

    scene->Destroy();
    manager->Destroy();

    return container;
}

#endif // end SYSTEM_HAS_FBX_SDK
#pragma once

#include <Core/ResourceManager/Resource.h>
#include <Foundation/Containers/StaticArray.h>
#include <Foundation/Math/BoundingBoxSphere.h>
#include <KrautPlugin/KrautDeclarations.h>

using ezMeshResourceHandle = ezTypedResourceHandle<class ezMeshResource>;
using ezKrautTreeResourceHandle = ezTypedResourceHandle<class ezKrautTreeResource>;
using ezMaterialResourceHandle = ezTypedResourceHandle<class ezMaterialResource>;
using ezSurfaceResourceHandle = ezTypedResourceHandle<class ezSurfaceResource>;

struct EZ_KRAUTPLUGIN_DLL ezKrautTreeResourceDetails
{
  ezBoundingBoxSphere m_Bounds;
  ezVec3 m_vLeafCenter;
  float m_fStaticColliderRadius;
  ezString m_sSurfaceResource;
};

struct EZ_KRAUTPLUGIN_DLL ezKrautTreeResourceDescriptor
{
  void Save(ezStreamWriter& inout_stream) const;
  ezResult Load(ezStreamReader& inout_stream);

  struct VertexData
  {
    EZ_DECLARE_POD_TYPE();

    ezVec3 m_vPosition;
    ezVec3 m_vTexCoord; // U,V and Q
    float m_fAmbientOcclusion = 1.0f;
    ezVec3 m_vNormal;
    ezVec3 m_vTangent;
    ezUInt8 m_uiColorVariation = 0;

    // to compute wind
    ezUInt8 m_uiBranchLevel = 0;  // 0 = trunk, 1 = main branches, 2 = twigs, ...
    ezUInt8 m_uiFlutterPhase = 0; // phase shift for the flutter effect
    ezVec3 m_vBendAnchor;
    float m_fAnchorBendStrength = 0;
    float m_fBendAndFlutterStrength = 0;
  };

  struct TriangleData
  {
    EZ_DECLARE_POD_TYPE();

    ezUInt32 m_uiVertexIndex[3];
  };

  struct SubMeshData
  {
    ezUInt16 m_uiFirstTriangle = 0;
    ezUInt16 m_uiNumTriangles = 0;
    ezUInt8 m_uiMaterialIndex = 0xFF;
  };

  struct LodData
  {
    float m_fMinLodDistance = 0;
    float m_fMaxLodDistance = 0;
    ezKrautLodType m_LodType = ezKrautLodType::None;

    ezDynamicArray<VertexData> m_Vertices;
    ezDynamicArray<TriangleData> m_Triangles;
    ezDynamicArray<SubMeshData> m_SubMeshes;
  };

  struct MaterialData
  {
    ezKrautMaterialType m_MaterialType;
    ezString m_sMaterial;
    ezColorGammaUB m_VariationColor = ezColor::White; // currently done through the material
  };

  ezKrautTreeResourceDetails m_Details;
  ezStaticArray<LodData, 5> m_Lods;
  ezHybridArray<MaterialData, 8> m_Materials;
};

class EZ_KRAUTPLUGIN_DLL ezKrautTreeResource : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezKrautTreeResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezKrautTreeResource);
  EZ_RESOURCE_DECLARE_CREATEABLE(ezKrautTreeResource, ezKrautTreeResourceDescriptor);

public:
  ezKrautTreeResource();

  const ezKrautTreeResourceDetails& GetDetails() const { return m_Details; }

  struct TreeLod
  {
    ezMeshResourceHandle m_hMesh;
    float m_fMinLodDistance = 0;
    float m_fMaxLodDistance = 0;
    ezKrautLodType m_LodType = ezKrautLodType::None;
  };

  ezArrayPtr<const TreeLod> GetTreeLODs() const { return m_TreeLODs.GetArrayPtr(); }

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

  ezKrautTreeResourceDetails m_Details;
  ezStaticArray<TreeLod, 5> m_TreeLODs;
};

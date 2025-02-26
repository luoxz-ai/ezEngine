#pragma once

#include <Foundation/Reflection/Reflection.h>
#include <Foundation/Types/UniquePtr.h>
#include <RecastPlugin/RecastPluginDLL.h>
#include <RendererCore/Utils/WorldGeoExtractionUtil.h>

class ezRcBuildContext;
struct rcPolyMesh;
struct rcPolyMeshDetail;
class ezWorld;
class dtNavMesh;
struct ezRecastNavMeshResourceDescriptor;
class ezProgress;
class ezStreamWriter;
class ezStreamReader;

struct EZ_RECASTPLUGIN_DLL ezRecastConfig
{
  float m_fAgentHeight = 1.5f;
  float m_fAgentRadius = 0.3f;
  float m_fAgentClimbHeight = 0.4f;
  ezAngle m_WalkableSlope = ezAngle::MakeFromDegree(45);
  float m_fCellSize = 0.2f;
  float m_fCellHeight = 0.2f;
  float m_fMaxEdgeLength = 4.0f;
  float m_fMaxSimplificationError = 1.3f;
  float m_fMinRegionSize = 3.0f;
  float m_fRegionMergeSize = 20.0f;
  float m_fDetailMeshSampleDistanceFactor = 1.0f;
  float m_fDetailMeshSampleErrorFactor = 1.0f;

  ezResult Serialize(ezStreamWriter& inout_stream) const;
  ezResult Deserialize(ezStreamReader& inout_stream);
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_RECASTPLUGIN_DLL, ezRecastConfig);



class EZ_RECASTPLUGIN_DLL ezRecastNavMeshBuilder
{
public:
  ezRecastNavMeshBuilder();
  ~ezRecastNavMeshBuilder();

  static ezResult ExtractWorldGeometry(const ezWorld& world, ezWorldGeoExtractionUtil::MeshObjectList& out_worldGeo);

  ezResult Build(const ezRecastConfig& config, const ezWorldGeoExtractionUtil::MeshObjectList& worldGeo, ezRecastNavMeshResourceDescriptor& out_navMeshDesc,
    ezProgress& ref_progress);

private:
  static void FillOutConfig(struct rcConfig& cfg, const ezRecastConfig& config, const ezBoundingBox& bbox);

  void Clear();
  void GenerateTriangleMeshFromDescription(const ezWorldGeoExtractionUtil::MeshObjectList& objects);
  void ComputeBoundingBox();
  ezResult BuildRecastPolyMesh(const ezRecastConfig& config, rcPolyMesh& out_PolyMesh, ezProgress& progress);
  static ezResult BuildDetourNavMeshData(const ezRecastConfig& config, const rcPolyMesh& polyMesh, ezDataBuffer& NavmeshData);

  struct Triangle
  {
    EZ_DECLARE_POD_TYPE();

    Triangle() = default;
    Triangle(ezInt32 a, ezInt32 b, ezInt32 c)
    {
      m_VertexIdx[0] = a;
      m_VertexIdx[1] = b;
      m_VertexIdx[2] = c;
    }

    ezInt32 m_VertexIdx[3];
  };

  ezBoundingBox m_BoundingBox;
  ezDynamicArray<ezVec3> m_Vertices;
  ezDynamicArray<Triangle> m_Triangles;
  ezDynamicArray<ezUInt8> m_TriangleAreaIDs;
  ezRcBuildContext* m_pRecastContext = nullptr;
};

#pragma once

#include <Core/Messages/EventMessage.h>
#include <Core/ResourceManager/ResourceHandle.h>
#include <Core/World/WorldModule.h>
#include <Foundation/Communication/Message.h>

struct ezGameObjectHandle;
struct ezSkeletonResourceDescriptor;

using ezSurfaceResourceHandle = ezTypedResourceHandle<class ezSurfaceResource>;

/// \brief Classifies the facing of an individual raycast hit
enum class ezPhysicsHitType : int8_t
{
  Undefined = -1,        ///< Returned if the respective physics binding does not provide this information
  TriangleFrontFace = 0, ///< The raycast hit the front face of a triangle
  TriangleBackFace = 1,  ///< The raycast hit the back face of a triangle
};

/// \brief Used for raycast and seep tests
struct ezPhysicsCastResult
{
  ezVec3 m_vPosition;
  ezVec3 m_vNormal;
  float m_fDistance;

  ezGameObjectHandle m_hShapeObject;                        ///< The game object to which the hit physics shape is attached.
  ezGameObjectHandle m_hActorObject;                        ///< The game object to which the parent actor of the hit physics shape is attached.
  ezSurfaceResourceHandle m_hSurface;                       ///< The type of surface that was hit (if available)
  ezUInt32 m_uiObjectFilterID = ezInvalidIndex;             ///< An ID either per object (rigid-body / ragdoll) or per shape (implementation specific) that can be used to ignore this object during raycasts and shape queries.
  ezPhysicsHitType m_hitType = ezPhysicsHitType::Undefined; ///< Classification of the triangle face, see ezPhysicsHitType

  // Physics-engine specific information, may be available or not.
  void* m_pInternalPhysicsShape = nullptr;
  void* m_pInternalPhysicsActor = nullptr;
};

struct ezPhysicsCastResultArray
{
  ezHybridArray<ezPhysicsCastResult, 16> m_Results;
};

/// \brief Used to report overlap query results
struct ezPhysicsOverlapResult
{
  EZ_DECLARE_POD_TYPE();

  ezGameObjectHandle m_hShapeObject;            ///< The game object to which the hit physics shape is attached.
  ezGameObjectHandle m_hActorObject;            ///< The game object to which the parent actor of the hit physics shape is attached.
  ezUInt32 m_uiObjectFilterID = ezInvalidIndex; ///< The shape id of the hit physics shape
  ezVec3 m_vCenterPosition;                     ///< The center position of the reported object in world space.

  // Physics-engine specific information, may be available or not.
  void* m_pInternalPhysicsShape = nullptr;
  void* m_pInternalPhysicsActor = nullptr;
};

struct ezPhysicsOverlapResultArray
{
  ezHybridArray<ezPhysicsOverlapResult, 16> m_Results;
};

/// \brief Flags for selecting which types of physics shapes should be included in things like overlap queries and raycasts.
///
/// This is mainly for optimization purposes. It is up to the physics integration to support some or all of these flags.
///
/// Note: If this is modified, 'Physics.ts' also has to be updated.
EZ_DECLARE_FLAGS_WITH_DEFAULT(ezUInt32, ezPhysicsShapeType, 0xFFFFFFFF,
  Static,    ///< Static geometry
  Dynamic,   ///< Dynamic and kinematic objects
  Query,     ///< Query shapes are kinematic bodies that don't participate in the simulation and are only used for raycasts and other queries.
  Trigger,   ///< Trigger shapes
  Character, ///< Shapes associated with character controllers.
  Ragdoll,   ///< All shapes belonging to ragdolls.
  Rope       ///< All shapes belonging to ropes.
);

EZ_DECLARE_REFLECTABLE_TYPE(EZ_CORE_DLL, ezPhysicsShapeType);

struct ezPhysicsQueryParameters
{
  ezPhysicsQueryParameters() = default;
  explicit ezPhysicsQueryParameters(ezUInt32 uiCollisionLayer,
    ezBitflags<ezPhysicsShapeType> shapeTypes = ezPhysicsShapeType::Default, ezUInt32 uiIgnoreObjectFilterID = ezInvalidIndex)
    : m_uiCollisionLayer(uiCollisionLayer)
    , m_ShapeTypes(shapeTypes)
    , m_uiIgnoreObjectFilterID(uiIgnoreObjectFilterID)
  {
  }

  ezUInt32 m_uiCollisionLayer = 0;
  ezBitflags<ezPhysicsShapeType> m_ShapeTypes = ezPhysicsShapeType::Default;
  ezUInt32 m_uiIgnoreObjectFilterID = ezInvalidIndex;
  bool m_bIgnoreInitialOverlap = false;
};

enum class ezPhysicsHitCollection
{
  Closest,
  Any
};

class EZ_CORE_DLL ezPhysicsWorldModuleInterface : public ezWorldModule
{
  EZ_ADD_DYNAMIC_REFLECTION(ezPhysicsWorldModuleInterface, ezWorldModule);

protected:
  ezPhysicsWorldModuleInterface(ezWorld* pWorld)
    : ezWorldModule(pWorld)
  {
  }

public:
  virtual bool Raycast(ezPhysicsCastResult& out_result, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const = 0;

  virtual bool RaycastAll(ezPhysicsCastResultArray& out_results, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params) const = 0;

  virtual bool SweepTestSphere(ezPhysicsCastResult& out_result, float fSphereRadius, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const = 0;

  virtual bool SweepTestBox(ezPhysicsCastResult& out_result, ezVec3 vBoxExtends, const ezTransform& transform, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const = 0;

  virtual bool SweepTestCapsule(ezPhysicsCastResult& out_result, float fCapsuleRadius, float fCapsuleHeight, const ezTransform& transform, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const = 0;

  virtual bool OverlapTestSphere(float fSphereRadius, const ezVec3& vPosition, const ezPhysicsQueryParameters& params) const = 0;

  virtual bool OverlapTestCapsule(float fCapsuleRadius, float fCapsuleHeight, const ezTransform& transform, const ezPhysicsQueryParameters& params) const = 0;

  virtual void QueryShapesInSphere(ezPhysicsOverlapResultArray& out_results, float fSphereRadius, const ezVec3& vPosition, const ezPhysicsQueryParameters& params) const = 0;

  virtual ezVec3 GetGravity() const = 0;

  //////////////////////////////////////////////////////////////////////////
  // ABSTRACTION HELPERS
  //
  // These functions are used to be able to use certain physics functionality, without having a direct dependency on the exact implementation (Jolt / PhysX).
  // If no physics module is available, they simply do nothing.
  // Add functions on demand.

  /// \brief Adds a static actor with a box shape to pOwner.
  virtual void AddStaticCollisionBox(ezGameObject* pOwner, ezVec3 vBoxSize) {}

  struct JointConfig
  {
    ezGameObjectHandle m_hActorA;
    ezGameObjectHandle m_hActorB;
    ezTransform m_LocalFrameA = ezTransform::MakeIdentity();
    ezTransform m_LocalFrameB = ezTransform::MakeIdentity();
  };

  struct FixedJointConfig : JointConfig
  {
  };

  /// \brief Adds a fixed joint to pOwner.
  virtual void AddFixedJointComponent(ezGameObject* pOwner, const ezPhysicsWorldModuleInterface::FixedJointConfig& cfg) {}
};

/// \brief Used to apply a physical impulse on the object
struct EZ_CORE_DLL ezMsgPhysicsAddImpulse : public ezMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgPhysicsAddImpulse, ezMessage);

  ezVec3 m_vGlobalPosition;
  ezVec3 m_vImpulse;
  ezUInt32 m_uiObjectFilterID = ezInvalidIndex;

  // Physics-engine specific information, may be available or not.
  void* m_pInternalPhysicsShape = nullptr;
  void* m_pInternalPhysicsActor = nullptr;
};

/// \brief Used to apply a physical force on the object
struct EZ_CORE_DLL ezMsgPhysicsAddForce : public ezMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgPhysicsAddForce, ezMessage);

  ezVec3 m_vGlobalPosition;
  ezVec3 m_vForce;

  // Physics-engine specific information, may be available or not.
  void* m_pInternalPhysicsShape = nullptr;
  void* m_pInternalPhysicsActor = nullptr;
};

struct EZ_CORE_DLL ezMsgPhysicsJointBroke : public ezEventMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgPhysicsJointBroke, ezEventMessage);

  ezGameObjectHandle m_hJointObject;
};

/// \brief Sent by components such as ezJoltGrabObjectComponent to indicate that the object has been grabbed or released.
struct EZ_CORE_DLL ezMsgObjectGrabbed : public ezMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgObjectGrabbed, ezMessage);

  ezGameObjectHandle m_hGrabbedBy;
  bool m_bGotGrabbed = true;
};

/// \brief Send this to components such as ezJoltGrabObjectComponent to demand that m_hGrabbedObjectToRelease should no longer be grabbed.
struct EZ_CORE_DLL ezMsgReleaseObjectGrab : public ezMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgReleaseObjectGrab, ezMessage);

  ezGameObjectHandle m_hGrabbedObjectToRelease;
};

//////////////////////////////////////////////////////////////////////////

#include <Foundation/Communication/Message.h>

struct EZ_CORE_DLL ezSmcTriangle
{
  EZ_DECLARE_POD_TYPE();

  ezUInt32 m_uiVertexIndices[3];
};

struct EZ_CORE_DLL ezSmcSubMesh
{
  EZ_DECLARE_POD_TYPE();

  ezUInt32 m_uiFirstTriangle = 0;
  ezUInt32 m_uiNumTriangles = 0;
  ezUInt16 m_uiSurfaceIndex = 0;
};

struct EZ_CORE_DLL ezSmcDescription
{
  ezDeque<ezVec3> m_Vertices;
  ezDeque<ezSmcTriangle> m_Triangles;
  ezDeque<ezSmcSubMesh> m_SubMeshes;
  ezDeque<ezString> m_Surfaces;
};

struct EZ_CORE_DLL ezMsgBuildStaticMesh : public ezMessage
{
  EZ_DECLARE_MESSAGE_TYPE(ezMsgBuildStaticMesh, ezMessage);

  /// \brief Append data to this description to add meshes to the automatic static mesh generation
  ezSmcDescription* m_pStaticMeshDescription = nullptr;
};

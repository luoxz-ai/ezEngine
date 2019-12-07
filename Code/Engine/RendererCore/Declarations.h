#pragma once

#include <RendererCore/RendererCoreDLL.h>
#include <Foundation/Strings/HashedString.h>
#include <Core/ResourceManager/ResourceHandle.h>

class ezGALContext;
class ezShaderStageBinary;
struct ezVertexDeclarationInfo;

typedef ezTypedResourceHandle<class ezTexture2DResource> ezTexture2DResourceHandle;
typedef ezTypedResourceHandle<class ezRenderToTexture2DResource> ezRenderToTexture2DResourceHandle;
typedef ezTypedResourceHandle<class ezTextureCubeResource> ezTextureCubeResourceHandle;
typedef ezTypedResourceHandle<class ezMeshBufferResource> ezMeshBufferResourceHandle;
typedef ezTypedResourceHandle<class ezMeshResource> ezMeshResourceHandle;
typedef ezTypedResourceHandle<class ezMaterialResource> ezMaterialResourceHandle;
typedef ezTypedResourceHandle<class ezShaderResource> ezShaderResourceHandle;
typedef ezTypedResourceHandle<class ezShaderPermutationResource> ezShaderPermutationResourceHandle;
typedef ezTypedResourceHandle<class ezRenderPipelineResource> ezRenderPipelineResourceHandle;
typedef ezTypedResourceHandle<class ezDecalResource> ezDecalResourceHandle;
typedef ezTypedResourceHandle<class ezDecalAtlasResource> ezDecalAtlasResourceHandle;

struct EZ_RENDERERCORE_DLL ezPermutationVar
{
  EZ_DECLARE_MEM_RELOCATABLE_TYPE();

  ezHashedString m_sName;
  ezHashedString m_sValue;

  EZ_ALWAYS_INLINE bool operator==(const ezPermutationVar& other) const
  {
    return m_sName == other.m_sName && m_sValue == other.m_sValue;
  }
};


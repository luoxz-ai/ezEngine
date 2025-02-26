#pragma once

#include <Core/ResourceManager/Resource.h>
#include <ParticlePlugin/Effect/ParticleEffectDescriptor.h>
#include <RendererCore/Declarations.h>

using ezParticleEffectResourceHandle = ezTypedResourceHandle<class ezParticleEffectResource>;

struct EZ_PARTICLEPLUGIN_DLL ezParticleEffectResourceDescriptor
{
  virtual void Save(ezStreamWriter& inout_stream) const;
  virtual void Load(ezStreamReader& inout_stream);

  ezParticleEffectDescriptor m_Effect;
};

class EZ_PARTICLEPLUGIN_DLL ezParticleEffectResource final : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezParticleEffectResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezParticleEffectResource);
  EZ_RESOURCE_DECLARE_CREATEABLE(ezParticleEffectResource, ezParticleEffectResourceDescriptor);

public:
  ezParticleEffectResource();
  ~ezParticleEffectResource();

  const ezParticleEffectResourceDescriptor& GetDescriptor() { return m_Desc; }

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

private:
  ezParticleEffectResourceDescriptor m_Desc;
};

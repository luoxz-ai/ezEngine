#pragma once

#include <Core/Scripting/ScriptClassResource.h>
#include <TypeScriptPlugin/TypeScriptPluginDLL.h>

class ezComponent;
class ezTypeScriptBinding;

class EZ_TYPESCRIPTPLUGIN_DLL ezTypeScriptInstance : public ezScriptInstance
{
public:
  ezTypeScriptInstance(ezComponent& owner, ezTypeScriptBinding& binding);

  virtual void ApplyParameters(const ezArrayMap<ezHashedString, ezVariant>& parameters) override;

  ezTypeScriptBinding& GetBinding() { return m_Binding; }

  ezComponent& GetComponent() { return m_Component; }

private:
  ezTypeScriptBinding& m_Binding;
  ezComponent& m_Component;
};

class EZ_TYPESCRIPTPLUGIN_DLL ezTypeScriptClassResource : public ezScriptClassResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezTypeScriptClassResource, ezScriptClassResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezTypeScriptClassResource);

public:
  ezTypeScriptClassResource();
  ~ezTypeScriptClassResource();

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* pStream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

  virtual ezUniquePtr<ezScriptInstance> Instantiate(ezReflectedClass& owner, ezWorld* pWorld) const override;

private:
  ezUuid m_Guid;
};
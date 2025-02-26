#pragma once

#include <Core/ActorSystem/ActorPlugin.h>

#include <Core/GameApplication/WindowOutputTargetBase.h>
#include <Core/System/Window.h>

class ezActor;
class ezWindowOutputTargetBase;
class ezWindowBase;

class EZ_CORE_DLL ezActorPluginWindow : public ezActorPlugin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezActorPluginWindow, ezActorPlugin);

public:
  virtual ezWindowBase* GetWindow() const = 0;
  virtual ezWindowOutputTargetBase* GetOutputTarget() const = 0;

protected:
  virtual void Update() override;
};

class EZ_CORE_DLL ezActorPluginWindowOwner : public ezActorPluginWindow
{
  EZ_ADD_DYNAMIC_REFLECTION(ezActorPluginWindowOwner, ezActorPluginWindow);

public:
  virtual ~ezActorPluginWindowOwner();
  virtual ezWindowBase* GetWindow() const override;
  virtual ezWindowOutputTargetBase* GetOutputTarget() const override;

  ezUniquePtr<ezWindowBase> m_pWindow;
  ezUniquePtr<ezWindowOutputTargetBase> m_pWindowOutputTarget;
};

class EZ_CORE_DLL ezActorPluginWindowShared : public ezActorPluginWindow
{
  EZ_ADD_DYNAMIC_REFLECTION(ezActorPluginWindowShared, ezActorPluginWindow);

public:
  virtual ezWindowBase* GetWindow() const override;
  virtual ezWindowOutputTargetBase* GetOutputTarget() const override;

  ezWindowBase* m_pWindow = nullptr;
  ezWindowOutputTargetBase* m_pWindowOutputTarget = nullptr;
};

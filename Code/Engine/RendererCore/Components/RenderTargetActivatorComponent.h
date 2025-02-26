#pragma once

#include <Core/World/World.h>
#include <RendererCore/Components/RenderComponent.h>
#include <RendererCore/Textures/Texture2DResource.h>

struct ezMsgExtractRenderData;

using ezRenderTargetComponentManager = ezComponentManager<class ezRenderTargetActivatorComponent, ezBlockStorageType::Compact>;

class EZ_RENDERERCORE_DLL ezRenderTargetActivatorComponent : public ezRenderComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezRenderTargetActivatorComponent, ezRenderComponent, ezRenderTargetComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // ezComponent
public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;


  //////////////////////////////////////////////////////////////////////////
  // ezRenderComponent

public:
  virtual ezResult GetLocalBounds(ezBoundingBoxSphere& ref_bounds, bool& ref_bAlwaysVisible, ezMsgUpdateLocalBounds& ref_msg) override;


  //////////////////////////////////////////////////////////////////////////
  // ezRenderTargetActivatorComponent

public:
  ezRenderTargetActivatorComponent();
  ~ezRenderTargetActivatorComponent();

  void SetRenderTargetFile(const char* szFile); // [ property ]
  const char* GetRenderTargetFile() const;      // [ property ]

  void SetRenderTarget(const ezRenderToTexture2DResourceHandle& hResource);
  ezRenderToTexture2DResourceHandle GetRenderTarget() const { return m_hRenderTarget; }

private:
  void OnMsgExtractRenderData(ezMsgExtractRenderData& msg) const;

  ezRenderToTexture2DResourceHandle m_hRenderTarget;
};

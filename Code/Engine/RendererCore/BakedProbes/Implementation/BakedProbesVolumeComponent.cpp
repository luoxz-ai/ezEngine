#include <RendererCore/RendererCorePCH.h>

#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/BakedProbes/BakedProbesVolumeComponent.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezBakedProbesVolumeComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Extents", GetExtents, SetExtents)->AddAttributes(new ezDefaultValueAttribute(ezVec3(10.0f)), new ezClampValueAttribute(ezVec3(0), ezVariant())),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgUpdateLocalBounds, OnUpdateLocalBounds),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezInDevelopmentAttribute(ezInDevelopmentAttribute::Phase::Beta),
    new ezCategoryAttribute("Rendering/Baking"),
    new ezBoxManipulatorAttribute("Extents", 1.0f, true),
    new ezBoxVisualizerAttribute("Extents", 1.0f, ezColor::OrangeRed),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE
// clang-format on

ezBakedProbesVolumeComponent::ezBakedProbesVolumeComponent() = default;
ezBakedProbesVolumeComponent::~ezBakedProbesVolumeComponent() = default;

void ezBakedProbesVolumeComponent::OnActivated()
{
  GetOwner()->UpdateLocalBounds();
}

void ezBakedProbesVolumeComponent::OnDeactivated()
{
  GetOwner()->UpdateLocalBounds();
}

void ezBakedProbesVolumeComponent::SetExtents(const ezVec3& vExtents)
{
  if (m_vExtents != vExtents)
  {
    m_vExtents = vExtents;

    if (IsActiveAndInitialized())
    {
      GetOwner()->UpdateLocalBounds();
    }
  }
}

void ezBakedProbesVolumeComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  ezStreamWriter& s = inout_stream.GetStream();

  s << m_vExtents;
}

void ezBakedProbesVolumeComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  // const ezUInt32 uiVersion = stream.GetComponentTypeVersion(GetStaticRTTI());
  ezStreamReader& s = inout_stream.GetStream();

  s >> m_vExtents;
}

void ezBakedProbesVolumeComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const
{
  ref_msg.AddBounds(ezBoundingBoxSphere::MakeFromBox(ezBoundingBox::MakeFromMinMax(-m_vExtents * 0.5f, m_vExtents * 0.5f)), ezInvalidSpatialDataCategory);
}


EZ_STATICLINK_FILE(RendererCore, RendererCore_BakedProbes_Implementation_BakedProbesVolumeComponent);

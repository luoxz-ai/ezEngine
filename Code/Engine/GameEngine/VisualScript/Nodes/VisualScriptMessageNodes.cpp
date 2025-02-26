#include <GameEngine/GameEnginePCH.h>

#include <Core/Messages/CommonMessages.h>
#include <Core/World/World.h>
#include <GameEngine/Gameplay/InputComponent.h>
#include <GameEngine/VisualScript/Nodes/VisualScriptMessageNodes.h>
#include <GameEngine/VisualScript/VisualScriptInstance.h>

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_ScriptStartEvent, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_ScriptStartEvent>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Event Handler"),
    new ezTitleAttribute("OnScriptStart"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_PROPERTIES
  {
    EZ_OUTPUT_EXECUTION_PIN("OnStart", 0),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_ScriptStartEvent::ezVisualScriptNode_ScriptStartEvent()
{
  m_bStepNode = true;
}

ezVisualScriptNode_ScriptStartEvent::~ezVisualScriptNode_ScriptStartEvent() = default;

void ezVisualScriptNode_ScriptStartEvent::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  pInstance->ExecuteConnectedNodes(this, 0);
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_ScriptUpdateEvent, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_ScriptUpdateEvent>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Event Handler"),
    new ezTitleAttribute("OnScriptUpdate"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_PROPERTIES
  {
    EZ_OUTPUT_EXECUTION_PIN("OnUpdate", 0),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_ScriptUpdateEvent::ezVisualScriptNode_ScriptUpdateEvent()
{
  m_bStepNode = true;
}

ezVisualScriptNode_ScriptUpdateEvent::~ezVisualScriptNode_ScriptUpdateEvent() = default;

void ezVisualScriptNode_ScriptUpdateEvent::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  pInstance->ExecuteConnectedNodes(this, 0);

  // Make sure to be updated again next frame
  m_bStepNode = true;
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_GenericEvent, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_GenericEvent>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Event Handler"),
    new ezTitleAttribute("Generic Event '{Message}'"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Message", GetMessage, SetMessage),
    EZ_OUTPUT_EXECUTION_PIN("OnEvent", 0),
    EZ_OUTPUT_DATA_PIN("Value", 0, ezVisualScriptDataPinType::Variant),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_GenericEvent::ezVisualScriptNode_GenericEvent() = default;
ezVisualScriptNode_GenericEvent::~ezVisualScriptNode_GenericEvent() = default;

void ezVisualScriptNode_GenericEvent::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  pInstance->SetOutputPinValue(this, 0, &m_Value);
  pInstance->ExecuteConnectedNodes(this, 0);
}


ezInt32 ezVisualScriptNode_GenericEvent::HandlesMessagesWithID() const
{
  return ezMsgGenericEvent::GetTypeMsgId();
}

void ezVisualScriptNode_GenericEvent::HandleMessage(ezMessage* pMsg)
{
  ezMsgGenericEvent& msg = *static_cast<ezMsgGenericEvent*>(pMsg);

  if (msg.m_sMessage == m_sMessage)
  {
    m_bStepNode = true;

    m_Value = msg.m_Value;
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_PhysicsTriggerEvent, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_PhysicsTriggerEvent>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Physics/Events"),
    new ezTitleAttribute("Trigger Event '{TriggerMessage}'"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("TriggerMessage", GetTriggerMessage, SetTriggerMessage),
    EZ_OUTPUT_EXECUTION_PIN("OnActivated", 0),
    EZ_OUTPUT_EXECUTION_PIN("OnDeactivated", 2),
    EZ_OUTPUT_DATA_PIN("Object", 0, ezVisualScriptDataPinType::GameObjectHandle),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_PhysicsTriggerEvent::ezVisualScriptNode_PhysicsTriggerEvent() = default;
ezVisualScriptNode_PhysicsTriggerEvent::~ezVisualScriptNode_PhysicsTriggerEvent() = default;

void ezVisualScriptNode_PhysicsTriggerEvent::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  if (m_State == ezTriggerState::Activated)
  {
    pInstance->SetOutputPinValue(this, 0, &m_hObject);
    pInstance->ExecuteConnectedNodes(this, 0);
  }
  else if (m_State == ezTriggerState::Deactivated)
  {
    pInstance->SetOutputPinValue(this, 0, &m_hObject);
    pInstance->ExecuteConnectedNodes(this, 2);
  }
}


ezInt32 ezVisualScriptNode_PhysicsTriggerEvent::HandlesMessagesWithID() const
{
  return ezMsgTriggerTriggered::GetTypeMsgId();
}

void ezVisualScriptNode_PhysicsTriggerEvent::HandleMessage(ezMessage* pMsg)
{
  ezMsgTriggerTriggered& msg = *static_cast<ezMsgTriggerTriggered*>(pMsg);

  if (msg.m_sMessage == m_sTriggerMessage)
  {
    m_bStepNode = true;

    m_State = msg.m_TriggerState;
    m_hObject = msg.m_hTriggeringObject;
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_InputState, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_InputState>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Input"),
    new ezTitleAttribute("Input '{InputAction}'")
  }
    EZ_END_ATTRIBUTES;
    EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("InputAction", m_sInputAction),
    EZ_MEMBER_PROPERTY("OnlyKeyPressed", m_bOnlyKeyPressed),
    EZ_INPUT_DATA_PIN("Component", 0, ezVisualScriptDataPinType::ComponentHandle),
    EZ_OUTPUT_DATA_PIN("Value", 0, ezVisualScriptDataPinType::Number),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_InputState::ezVisualScriptNode_InputState() = default;
ezVisualScriptNode_InputState::~ezVisualScriptNode_InputState() = default;

void ezVisualScriptNode_InputState::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  if (m_hComponent.IsInvalidated())
    return;

  ezComponent* pComponent;
  if (!pInstance->GetWorld()->TryGetComponent(m_hComponent, pComponent))
  {
    m_hComponent.Invalidate();
    return;
  }

  ezInputComponent* pInput = ezDynamicCast<ezInputComponent*>(pComponent);
  if (pInput == nullptr)
  {
    m_hComponent.Invalidate();
    return;
  }

  const double fValue = pInput->GetCurrentInputState(m_sInputAction, m_bOnlyKeyPressed);
  pInstance->SetOutputPinValue(this, 0, &fValue);
}

void* ezVisualScriptNode_InputState::GetInputPinDataPointer(ezUInt8 uiPin)
{
  return &m_hComponent;
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVisualScriptNode_InputEvent, 1, ezRTTIDefaultAllocator<ezVisualScriptNode_InputEvent>)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Input"),
    new ezTitleAttribute("InputEvent '{InputAction}'"),
  }
    EZ_END_ATTRIBUTES;
    EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("InputAction", GetInputAction, SetInputAction),
    EZ_OUTPUT_EXECUTION_PIN("OnPressed", 0),
    EZ_OUTPUT_EXECUTION_PIN("OnDown", 1),
    EZ_OUTPUT_EXECUTION_PIN("OnReleased", 2),
    EZ_OUTPUT_DATA_PIN("Object", 0, ezVisualScriptDataPinType::GameObjectHandle),
    EZ_OUTPUT_DATA_PIN("Component", 1, ezVisualScriptDataPinType::ComponentHandle),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezVisualScriptNode_InputEvent::ezVisualScriptNode_InputEvent() = default;
ezVisualScriptNode_InputEvent::~ezVisualScriptNode_InputEvent() = default;

void ezVisualScriptNode_InputEvent::Execute(ezVisualScriptInstance* pInstance, ezUInt8 uiExecPin)
{
  if (m_State == ezTriggerState::Activated)
  {
    pInstance->SetOutputPinValue(this, 0, &m_hSenderObject);
    pInstance->ExecuteConnectedNodes(this, 0);
  }
  else if (m_State == ezTriggerState::Continuing)
  {
    pInstance->SetOutputPinValue(this, 0, &m_hSenderObject);
    pInstance->ExecuteConnectedNodes(this, 1);
  }
  else if (m_State == ezTriggerState::Deactivated)
  {
    pInstance->SetOutputPinValue(this, 0, &m_hSenderObject);
    pInstance->ExecuteConnectedNodes(this, 2);
  }
}

ezInt32 ezVisualScriptNode_InputEvent::HandlesMessagesWithID() const
{
  return ezMsgInputActionTriggered::GetTypeMsgId();
}

void ezVisualScriptNode_InputEvent::HandleMessage(ezMessage* pMsg)
{
  ezMsgInputActionTriggered& msg = *static_cast<ezMsgInputActionTriggered*>(pMsg);

  if (msg.m_sInputAction == m_sInputAction)
  {
    m_bStepNode = true;

    m_State = msg.m_TriggerState;
    m_hSenderObject = msg.m_hSenderObject;
    m_hSenderComponent = msg.m_hSenderComponent;
  }
}

//////////////////////////////////////////////////////////////////////////



EZ_STATICLINK_FILE(GameEngine, GameEngine_VisualScript_Nodes_VisualScriptMessageNodes);

#include <OpenXRPluginPCH.h>

#include <Foundation/Configuration/CVar.h>
#include <Foundation/Time/Stopwatch.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/Interfaces/SoundInterface.h>
#include <OpenXRPlugin/OpenXRIncludes.h>
#include <OpenXRPlugin/OpenXRSingleton.h>
#include <RendererCore/Components/CameraComponent.h>
#include <RendererCore/GPUResourcePool/GPUResourcePool.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererDX11/Resources/TextureDX11.h>
#include <RendererFoundation/Context/Context.h>
#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Profiling/Profiling.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>

#include <../../../Data/Base/Shaders/Pipeline/VRCompanionViewConstants.h>
#include <Core/World/World.h>
#include <GameEngine/VirtualReality/StageSpaceComponent.h>
#include <RendererCore/Shader/ShaderResource.h>
#include <algorithm>
#include <vector>

#pragma optimize("", off)

#if EZ_ENABLED(EZ_PLATFORM_WINDOWS)
#  include <RendererDX11/Context/ContextDX11.h>
#  include <RendererDX11/Device/DeviceDX11.h>
#endif
#include <GameEngine/ActorSystem/Actor.h>

EZ_IMPLEMENT_SINGLETON(ezOpenXR);

static ezOpenXR g_OpenXRSingleton;

#define XR_CHECK_LOG(exp)                                                                                              \
  {                                                                                                                    \
    if (exp != XR_SUCCESS)                                                                                             \
    {                                                                                                                  \
      ezLog::Error("OpenXR failure: %s:%d", __FILE__, __LINE__);                                                       \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define EZ_CHECK_XR(exp)                                                                                               \
  {                                                                                                                    \
    if (exp != XR_SUCCESS)                                                                                             \
    {                                                                                                                  \
      ezLog::Error("OpenXR failure: %s:%d", __FILE__, __LINE__);                                                       \
      return EZ_FAILURE;                                                                                               \
    }                                                                                                                  \
  }

#define XR_SUCCEED_OR_RETURN(code, cleanup)                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    auto s = (code);                                                                                                   \
    if (s != XR_SUCCESS)                                                                                               \
    {                                                                                                                  \
      cleanup();                                                                                                       \
      return s;                                                                                                        \
    }                                                                                                                  \
  } while (false)

#define XR_SUCCEED_OR_RETURN_LOG(code, cleanup)                                                                        \
  do                                                                                                                   \
  {                                                                                                                    \
    auto s = (code);                                                                                                   \
    if (s != XR_SUCCESS)                                                                                               \
    {                                                                                                                  \
      ezLog::Error("OpenXR call '{0}' failed with: {1}", EZ_STRINGIZE(code), s);                                       \
      cleanup();                                                                                                       \
      return s;                                                                                                        \
    }                                                                                                                  \
  } while (false)

static void voidFunction() {}

ezOpenXR::ezOpenXR()
  : m_SingletonRegistrar(this)
{
}

bool ezOpenXR::IsHmdPresent() const
{
  XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
  systemInfo.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  uint64_t systemId = XR_NULL_SYSTEM_ID;
  XrResult res = xrGetSystem(m_instance, &systemInfo, &systemId);

  return res == XrResult::XR_SUCCESS;
}

static ezResult SelectExtensions(ezHybridArray<const char*, 6>& extensions)
{
  // Fetch the list of extensions supported by the runtime.
  ezUInt32 extensionCount;
  EZ_CHECK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr));
  std::vector<XrExtensionProperties> extensionProperties(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
  EZ_CHECK_XR(
    xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()));

  // Add a specific extension to the list of extensions to be enabled, if it is supported.
  auto AddExtIfSupported = [&](const char* extensionName) -> ezResult {
    auto it = std::find_if(begin(extensionProperties), end(extensionProperties),
      [&](const XrExtensionProperties& prop) { return ezStringUtils::IsEqual(prop.extensionName, extensionName); });
    if (it != end(extensionProperties))
    {
      extensions.PushBack(extensionName);
      return EZ_SUCCESS;
    }
    return EZ_FAILURE;
  };

  // D3D11 extension is required so check that it was added.
  EZ_SUCCEED_OR_RETURN(AddExtIfSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME));

  // Additional optional extensions for enhanced functionality. Track whether enabled in m_optionalExtensions.
  // m_optionalExtensions.UnboundedRefSpaceSupported =
  // AddExtIfSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME); m_optionalExtensions.SpatialAnchorSupported =
  // AddExtIfSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);

  return EZ_SUCCESS;
}

ezResult ezOpenXR::Initialize()
{
  if (m_instance != nullptr)
    return EZ_FAILURE;

  // Build out the extensions to enable. Some extensions are required and some are optional.
  ezHybridArray<const char*, 6> enabledExtensions;
  EZ_SUCCEED_OR_RETURN(SelectExtensions(enabledExtensions));

  // Create the instance with desired extensions.
  XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
  createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.GetCount();
  createInfo.enabledExtensionNames = enabledExtensions.GetData();

  ezStringUtils::Copy(createInfo.applicationInfo.applicationName,
    EZ_ARRAY_SIZE(createInfo.applicationInfo.applicationName),
    ezApplication::GetApplicationInstance()->GetApplicationName());
  createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  createInfo.applicationInfo.applicationVersion = 1;
  EZ_CHECK_XR(xrCreateInstance(&createInfo, &m_instance));

  ezLog::Success("OpenXR initialized successfully.");
  return EZ_SUCCESS;
}

void ezOpenXR::Deinitialize()
{
  DeinitSwapChain();
  DeinitSession();
  DeinitSystem();

  if (m_instance)
  {
    xrDestroyInstance(m_instance);
    m_instance = nullptr;
  }
}

bool ezOpenXR::IsInitialized() const
{
  return m_instance != nullptr;
}

const ezHMDInfo& ezOpenXR::GetHmdInfo() const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  return m_Info;
}

void ezOpenXR::GetDeviceList(ezHybridArray<ezVRDeviceID, 64>& out_Devices) const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  out_Devices.PushBack(0);
  // TODO input devices
  /*for (ezVRDeviceID i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
  {
    if (m_DeviceState[i].m_bDeviceIsConnected)
    {
      out_Devices.PushBack(i);
    }
  }*/
}

ezVRDeviceID ezOpenXR::GetDeviceIDByType(ezVRDeviceType::Enum type) const
{
  ezVRDeviceID deviceID = -1;
  switch (type)
  {
    case ezVRDeviceType::HMD:
      deviceID = 0;
      break;
    case ezVRDeviceType::LeftController:
      deviceID = m_iLeftControllerDeviceID;
      break;
    case ezVRDeviceType::RightController:
      deviceID = m_iRightControllerDeviceID;
      break;
    default:
      deviceID = -1;
      break;
  }

  if (deviceID != -1 && !m_DeviceState[deviceID].m_bDeviceIsConnected)
  {
    deviceID = -1;
  }
  return deviceID;
}

const ezVRDeviceState& ezOpenXR::GetDeviceState(ezVRDeviceID uiDeviceID) const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  EZ_ASSERT_DEV(uiDeviceID < 3, "Invalid device ID.");
  EZ_ASSERT_DEV(m_DeviceState[uiDeviceID].m_bDeviceIsConnected, "Invalid device ID.");
  return m_DeviceState[uiDeviceID];
}

ezEvent<const ezVRDeviceEvent&>& ezOpenXR::DeviceEvents()
{
  return m_DeviceEvents;
}

ezUniquePtr<ezActor> ezOpenXR::CreateActor(
  ezView* pView, ezGALMSAASampleCount::Enum msaaCount)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  XrResult res = InitSystem();
  if (res != XrResult::XR_SUCCESS)
  {
    ezLog::Error("");
    return {};
  }
  res = InitSession();
  if (res != XrResult::XR_SUCCESS)
  {
    DeinitSystem();
    ezLog::Error("");
    return {};
  }

  res = InitSwapChain(msaaCount);
  if (res != XrResult::XR_SUCCESS)
  {
    DeinitSession();
    DeinitSystem();
    ezLog::Error("");
    return {};
  }

  {
    EZ_ASSERT_DEV(pView->GetCamera() != nullptr, "The provided view requires a camera to be set.");
    SetHMDCamera(pView->GetCamera());
  }

  ezUniquePtr<ezActor> pActor = EZ_DEFAULT_NEW(ezActor, "OpenXR", this);

  // TODO: Companion Window
  //ezUniquePtr<ezWindowVR> pWindow = EZ_DEFAULT_NEW(ezWindowVR, this, {});
  //ezUniquePtr<ezWindowOutputTargetVR> pOutput = EZ_DEFAULT_NEW(ezWindowOutputTargetVR, this, {});

  ezUniquePtr<ezActorPluginWindowVR> pActorPlugin = EZ_DEFAULT_NEW(ezActorPluginWindowVR, this, {}, {});
  pActor->AddPlugin(std::move(pActorPlugin));


  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();

  m_hView = pView->GetHandle();
  pView->SetCameraUsageHint(ezCameraUsageHint::MainView);

  /*{
    ezGALSwapChainCreationDescription desc;
    desc.m_BackBufferFormat = ConvertTextureFormat(m_colorSwapchain.format);
    desc.m_bAllowScreenshots = true;
    desc.m_bDoubleBuffered = true;
    desc.m_pWindow = nullptr;
    desc.m_SampleCount = ConvertMSAACount(0);
    ezGALOpenXRSwapChainDX11* pSwapChain = EZ_DEFAULT_NEW(ezGALOpenXRSwapChainDX11, desc);
    if (!pSwapChain->InitPlatform(pDevice).Succeeded())
    {
      EZ_DEFAULT_DELETE(pSwapChain);
      return nullptr;
    }
  }*/



  m_RenderTargetSetup.SetRenderTarget(0, pDevice->GetDefaultRenderTargetView(m_hColorRT));
  m_RenderTargetSetup.SetDepthStencilTarget(pDevice->GetDefaultRenderTargetView(m_hDepthRT));

  pView->SetRenderTargetSetup(m_RenderTargetSetup);
  pView->SetRenderPassProperty("ColorSource", "MSAA_Mode", (ezInt32)msaaCount);
  pView->SetRenderPassProperty("DepthStencil", "MSAA_Mode", (ezInt32)msaaCount);

  pView->SetViewport(
    ezRectFloat((float)m_Info.m_vEyeRenderTargetSize.width, (float)m_Info.m_vEyeRenderTargetSize.height));

  ezRenderWorld::AddMainView(m_hView);
  return std::move(pActor);
}

void ezOpenXR::OnActorDestroyed()
{
  if (m_hView.IsInvalidated())
    return;

  m_pWorld = nullptr;
  SetHMDCamera(nullptr);

  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();

  ezRenderWorld::RemoveMainView(m_hView);
  m_hView.Invalidate();
  m_RenderTargetSetup.DestroyAllAttachedViews();

  pDevice->DestroyTexture(m_hColorRT);
  m_hColorRT.Invalidate();
  pDevice->DestroyTexture(m_hDepthRT);
  m_hDepthRT.Invalidate();

  SetCompanionViewRenderTarget(ezGALTextureHandle());

  DeinitSwapChain();
  DeinitSession();
  DeinitSystem();
}

bool ezOpenXR::SupportsCompanionView()
{
  return true;
}

bool ezOpenXR::SetCompanionViewRenderTarget(ezGALTextureHandle hRenderTarget)
{
  if (!m_hCompanionRenderTarget.IsInvalidated() && !hRenderTarget.IsInvalidated())
  {
    // Maintain already created resources (just switch target).
  }
  else if (!m_hCompanionRenderTarget.IsInvalidated() && hRenderTarget.IsInvalidated())
  {
    // Delete companion resources.
    ezRenderContext::DeleteConstantBufferStorage(m_hCompanionConstantBuffer);
    m_hCompanionConstantBuffer.Invalidate();
  }
  else if (m_hCompanionRenderTarget.IsInvalidated() && !hRenderTarget.IsInvalidated())
  {
    // Create companion resources.
    m_hCompanionShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/Pipeline/VRCompanionView.ezShader");
    EZ_ASSERT_DEV(m_hCompanionShader.IsValid(), "Could not load VR companion view shader!");
    m_hCompanionConstantBuffer = ezRenderContext::CreateConstantBufferStorage<ezVRCompanionViewConstants>();
    m_hCompanionRenderTarget = hRenderTarget;
  }
  return true;
}

ezGALTextureHandle ezOpenXR::GetCompanionViewRenderTarget() const
{
  return m_hCompanionRenderTarget;
}

XrResult ezOpenXR::InitSystem()
{
  EZ_ASSERT_DEV(m_systemId == XR_NULL_SYSTEM_ID, "OpenXR actor already exists.");
  XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
  systemInfo.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XR_SUCCEED_OR_RETURN_LOG(xrGetSystem(m_instance, &systemInfo, &m_systemId), DeinitSystem);
  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSystem()
{
  m_systemId = XR_NULL_SYSTEM_ID;
}

XrResult ezOpenXR::InitSession()
{
  EZ_ASSERT_DEV(m_session == nullptr, "");
  XR_SUCCEED_OR_RETURN(InitGraphicsPlugin(), DeinitSession);

  XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
  sessionCreateInfo.next = &m_xrGraphicsBindingD3D11;
  sessionCreateInfo.systemId = m_systemId;
  XR_SUCCEED_OR_RETURN_LOG(xrCreateSession(m_instance, &sessionCreateInfo, &m_session), DeinitSession);

  XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  spaceCreateInfo.poseInReferenceSpace = ConvertTransform(ezTransform::IdentityTransform());
  XR_SUCCEED_OR_RETURN_LOG(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_sceneSpace), DeinitSession);

  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  XR_SUCCEED_OR_RETURN_LOG(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_localSpace), DeinitSession);


  m_executionEventsId = ezGameApplicationBase::GetGameApplicationBaseInstance()->m_ExecutionEvents.AddEventHandler(
    ezMakeDelegate(&ezOpenXR::GameApplicationEventHandler, this));
  m_beginRenderEventsId =
    ezRenderWorld::s_BeginRenderEvent.AddEventHandler(ezMakeDelegate(&ezOpenXR::OnBeginRender, this));
  m_GALdeviceEventsId =
    ezGALDevice::GetDefaultDevice()->m_Events.AddEventHandler(ezMakeDelegate(&ezOpenXR::GALDeviceEventHandler, this));

  ReadHMDInfo();
  SetStageSpace(ezVRStageSpace::Standing);

  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSession()
{
  if (m_executionEventsId != 0)
  {
    ezGameApplicationBase::GetGameApplicationBaseInstance()->m_ExecutionEvents.RemoveEventHandler(m_executionEventsId);
    ezRenderWorld::s_BeginRenderEvent.RemoveEventHandler(m_beginRenderEventsId);
    ezGALDevice::GetDefaultDevice()->m_Events.RemoveEventHandler(m_GALdeviceEventsId);
  }

  if (m_sceneSpace)
  {
    xrDestroySpace(m_sceneSpace);
    m_sceneSpace = nullptr;
  }

  if (m_localSpace)
  {
    xrDestroySpace(m_localSpace);
    m_localSpace = nullptr;
  }

  if (m_session)
  {
    xrDestroySession(m_session);
    m_session = nullptr;
  }

  DeinitGraphicsPlugin();
}

XrResult ezOpenXR::InitGraphicsPlugin()
{
  EZ_ASSERT_DEV(m_xrGraphicsBindingD3D11.device == nullptr, "");
  // Hard-coded to d3d
  XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  XR_SUCCEED_OR_RETURN_LOG(
    xrGetD3D11GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements), DeinitGraphicsPlugin);
  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
  ezGALDeviceDX11* pD3dDevice = static_cast<ezGALDeviceDX11*>(pDevice);

  m_xrGraphicsBindingD3D11.device = pD3dDevice->GetDXDevice();

  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitGraphicsPlugin()
{
  m_xrGraphicsBindingD3D11.device = nullptr;
}

XrResult ezOpenXR::SelectSwapchainFormat(int64_t& colorFormat, int64_t& depthFormat)
{
  uint32_t swapchainFormatCount;
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr), voidFunction);
  std::vector<int64_t> swapchainFormats(swapchainFormatCount);
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(),
                             &swapchainFormatCount, swapchainFormats.data()),
    voidFunction);

  // List of supported color swapchain formats, in priority order.
  constexpr DXGI_FORMAT SupportedColorSwapchainFormats[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
  };

  constexpr DXGI_FORMAT SupportedDepthSwapchainFormats[] = {
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
  };

  auto swapchainFormatIt = std::find_first_of(std::begin(SupportedColorSwapchainFormats),
    std::end(SupportedColorSwapchainFormats), swapchainFormats.begin(), swapchainFormats.end());
  if (swapchainFormatIt == std::end(SupportedColorSwapchainFormats))
  {
    return XrResult::XR_ERROR_INITIALIZATION_FAILED;
  }
  colorFormat = *swapchainFormatIt;

  if (m_supportsDepth)
  {
    auto depthSwapchainFormatIt = std::find_first_of(std::begin(SupportedDepthSwapchainFormats),
      std::end(SupportedDepthSwapchainFormats), swapchainFormats.begin(), swapchainFormats.end());
    if (depthSwapchainFormatIt == std::end(SupportedDepthSwapchainFormats))
    {
      return XrResult::XR_ERROR_INITIALIZATION_FAILED;
    }
    depthFormat = *depthSwapchainFormatIt;
  }
  return XrResult::XR_SUCCESS;
}

XrResult ezOpenXR::CreateSwapchainImages(Swapchain& swapchain, SwapchainType type)
{
  if (type == SwapchainType::Color)
  {
    m_colorSwapChainImagesD3D11.SetCount(swapchain.imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    swapchain.images = reinterpret_cast<XrSwapchainImageBaseHeader*>(m_colorSwapChainImagesD3D11.GetData());
  }
  else
  {
    m_depthSwapChainImagesD3D11.SetCount(swapchain.imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    swapchain.images = reinterpret_cast<XrSwapchainImageBaseHeader*>(m_depthSwapChainImagesD3D11.GetData());
  }
  XR_SUCCEED_OR_RETURN_LOG(
    xrEnumerateSwapchainImages(swapchain.handle, swapchain.imageCount, &swapchain.imageCount, swapchain.images),
    voidFunction);

  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
  // We only create a texture out of the first d3d11 texture
  // Before each rendered frame the native resource is replaced in the ezGALTexture.
  for (ezUInt32 i = 0; i < 1; i++)
  {
    ID3D11Texture2D* pTex = nullptr;
    if (type == SwapchainType::Color)
    {
      pTex = m_colorSwapChainImagesD3D11[i].texture;
    }
    else
    {
      pTex = m_depthSwapChainImagesD3D11[i].texture;
    }

    D3D11_TEXTURE2D_DESC backBufferDesc;
    pTex->GetDesc(&backBufferDesc);

    ezGALTextureCreationDescription textureDesc;
    textureDesc.SetAsRenderTarget(backBufferDesc.Width, backBufferDesc.Height, ConvertTextureFormat(swapchain.format),
      ConvertMSAACount(backBufferDesc.SampleDesc.Count));
    textureDesc.m_uiArraySize = backBufferDesc.ArraySize;
    textureDesc.m_pExisitingNativeObject = pTex;
    // Need to add a ref as the ez texture will always remove one on destruction.
    pTex->AddRef();
    if (type == SwapchainType::Color)
    {
      m_hColorRT = pDevice->CreateTexture(textureDesc);
    }
    else
    {
      m_hDepthRT = pDevice->CreateTexture(textureDesc);
    }
    
  }
  return XR_SUCCESS;
}

XrResult ezOpenXR::InitSwapChain(ezGALMSAASampleCount::Enum msaaCount)
{
  // Read graphics properties for preferred swapchain length and logging.
  XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
  XR_SUCCEED_OR_RETURN_LOG(xrGetSystemProperties(m_instance, m_systemId, &systemProperties), DeinitSwapChain);

  ezUInt32 viewCount = 0;
  XR_SUCCEED_OR_RETURN_LOG(
    xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewConfigurationType, 0, &viewCount, nullptr),
    DeinitSwapChain);
  if (viewCount != 2)
  {
    ezLog::Error("No stereo view configuration present, can't create swap chain");
    DeinitSwapChain();
    return XR_ERROR_INITIALIZATION_FAILED;
  }
  ezHybridArray<XrViewConfigurationView, 2> views;
  views.SetCount(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewConfigurationType,
                             viewCount, &viewCount, views.GetData()),
    DeinitSwapChain);

  // Create the swapchain and get the images.
  // Select a swapchain format.
  m_primaryConfigView = views[0];
  XR_SUCCEED_OR_RETURN_LOG(SelectSwapchainFormat(m_colorSwapchain.format, m_depthSwapchain.format), DeinitSwapChain);

  // Create the swapchain.
  XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchainCreateInfo.arraySize = 2;
  swapchainCreateInfo.format = m_colorSwapchain.format;
  swapchainCreateInfo.width = m_primaryConfigView.recommendedImageRectWidth;
  swapchainCreateInfo.height = m_primaryConfigView.recommendedImageRectHeight;
  swapchainCreateInfo.mipCount = 1;
  swapchainCreateInfo.faceCount = 1;
  swapchainCreateInfo.sampleCount = (int)msaaCount;
  swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

  m_Info.m_vEyeRenderTargetSize = {swapchainCreateInfo.width, swapchainCreateInfo.height};

  auto CreateSwapChain = [this](const XrSwapchainCreateInfo& swapchainCreateInfo, Swapchain& swapchain,
                           SwapchainType type) -> XrResult {
    XR_SUCCEED_OR_RETURN_LOG(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle), voidFunction);
    XR_SUCCEED_OR_RETURN_LOG(
      xrEnumerateSwapchainImages(swapchain.handle, 0, &swapchain.imageCount, nullptr), voidFunction);
    CreateSwapchainImages(swapchain, type);

    return XrResult::XR_SUCCESS;
  };
  XR_SUCCEED_OR_RETURN(CreateSwapChain(swapchainCreateInfo, m_colorSwapchain, SwapchainType::Color), DeinitSwapChain);

  if (m_supportsDepth)
  {
    swapchainCreateInfo.format = m_depthSwapchain.format;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    XR_SUCCEED_OR_RETURN(CreateSwapChain(swapchainCreateInfo, m_depthSwapchain, SwapchainType::Depth), DeinitSwapChain);
  }
  else
  {
    // Create depth buffer in case the API does not support it
    ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
    ezGALTextureCreationDescription tcd;
    tcd.SetAsRenderTarget(m_Info.m_vEyeRenderTargetSize.width, m_Info.m_vEyeRenderTargetSize.height,
      ezGALResourceFormat::DFloat, msaaCount);
    tcd.m_uiArraySize = 2;
    m_hDepthRT = pDevice->CreateTexture(tcd);
  }

  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSwapChain()
{
  auto DeleteSwapchain = [](Swapchain& swapchain) {
    if (swapchain.handle != nullptr)
    {
      xrDestroySwapchain(swapchain.handle);
      swapchain.handle = nullptr;
    }
    swapchain.format = 0;
    swapchain.imageCount = 0;
    swapchain.images = nullptr;
    swapchain.imageIndex = 0;
  };
  m_primaryConfigView = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
  DeleteSwapchain(m_colorSwapchain);
  DeleteSwapchain(m_depthSwapchain);

  m_colorSwapChainImagesD3D11.Clear();
  m_depthSwapChainImagesD3D11.Clear();
  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
  pDevice->DestroyTexture(m_hColorRT);
  pDevice->DestroyTexture(m_hDepthRT);
  m_hColorRT.Invalidate();
  m_hDepthRT.Invalidate();
}

void ezOpenXR::GALDeviceEventHandler(const ezGALDeviceEvent& e)
{
  if (e.m_Type == ezGALDeviceEvent::Type::BeforeBeginFrame)
  {
    if (m_hView.IsInvalidated() || !m_sessionRunning)
      return;

    m_frameWaitInfo = XrFrameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    m_frameState = XrFrameState{XR_TYPE_FRAME_STATE};
    XrResult result = xrWaitFrame(m_session, &m_frameWaitInfo, &m_frameState);
    if (result != XR_SUCCESS)
    {
      // TODO?
    }
    m_frameBeginInfo = XrFrameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    result = xrBeginFrame(m_session, &m_frameBeginInfo);
    if (result != XR_SUCCESS)
    {
      // TODO?
    }
    UpdatePoses();

    auto AquireAndWait = [](Swapchain& swapchain) {
      XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      XR_CHECK_LOG(xrAcquireSwapchainImage(swapchain.handle, &acquireInfo, &swapchain.imageIndex));

      XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      waitInfo.timeout = XR_INFINITE_DURATION;
      XR_CHECK_LOG(xrWaitSwapchainImage(swapchain.handle, &waitInfo));
    };
    AquireAndWait(m_colorSwapchain);
    if (m_supportsDepth)
      AquireAndWait(m_depthSwapchain);

    // This will update the extracted view from last frame with the new data we got
    // this frame just before starting to render.
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      // Update render target setup with current OpenXR swapchain texture.
      ID3D11Texture2D* pTex = m_colorSwapChainImagesD3D11[m_colorSwapchain.imageIndex].texture;
      pTex->AddRef();
      ezGALDevice::GetDefaultDevice()->ReplaceExisitingNativeObject(m_hColorRT, pTex);
      pView->UpdateViewData(ezRenderWorld::GetDataIndexForRendering());
    }
    m_renderInProgress = true;
  }
}

void ezOpenXR::GameApplicationEventHandler(const ezGameApplicationExecutionEvent& e)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforeUpdatePlugins)
  {
    // Make sure the main camera component is set to stereo mode.
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      if (ezWorld* pWorld = pView->GetWorld())
      {
        EZ_LOCK(pWorld->GetWriteMarker());
        ezCameraComponent* pCameraComponent =
          pWorld->GetComponentManager<ezCameraComponentManager>()->GetCameraByUsageHint(ezCameraUsageHint::MainView);
        EZ_ASSERT_DEV(pCameraComponent != nullptr, "The world must have a main camera component.");
        pCameraComponent->SetCameraMode(ezCameraMode::Stereo);
      }
    }

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER, nullptr};
    XrEventDataBaseHeader* header = reinterpret_cast<XrEventDataBaseHeader*>(&event);

    while (xrPollEvent(m_instance, &event) == XR_SUCCESS)
    {
      switch (event.type)
      {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
          const XrEventDataSessionStateChanged& session_state_changed_event =
            *reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
          m_sessionState = session_state_changed_event.state;
          switch (m_sessionState)
          {
            case XR_SESSION_STATE_READY:
            {
              XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
              sessionBeginInfo.primaryViewConfigurationType = m_primaryViewConfigurationType;
              if (xrBeginSession(m_session, &sessionBeginInfo) == XR_SUCCESS)
              {
                m_sessionRunning = true;
              }
              break;
            }
            case XR_SESSION_STATE_STOPPING:
            {
              m_sessionRunning = false;
              if (xrEndSession(m_session) != XR_SUCCESS)
              {
                // TODO log
              }
              break;
            }
            case XR_SESSION_STATE_EXITING:
            {
              // Do not attempt to restart because user closed this session.
              m_exitRenderLoop = true;
              m_requestRestart = false;
              break;
            }
            case XR_SESSION_STATE_LOSS_PENDING:
            {
              // Poll for a new systemId
              m_exitRenderLoop = true;
              m_requestRestart = true;
              break;
            }
          }
          break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        {
          const XrEventDataInstanceLossPending& instance_loss_pending_event =
            *reinterpret_cast<XrEventDataInstanceLossPending*>(&event);
          m_exitRenderLoop = true;
          m_requestRestart = false;
          break;
        }
      }
      event = {XR_TYPE_EVENT_DATA_BUFFER, nullptr};
    }

    if (m_exitRenderLoop)
    {
      DeinitSession();
      DeinitSystem();
      if (m_requestRestart)
      {
        // Try to re-init session
        XrResult res = InitSystem();
        if (res != XR_SUCCESS)
        {
          return;
        }
        res = InitSession();
        if (res != XR_SUCCESS)
        {
          DeinitSystem();
          return;
        }
      }
    }
    //#TODO
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforePresent)
  {
    if (!m_renderInProgress)
      return;

    for (uint32_t i = 0; i < 2; i++)
    {
      m_projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
      m_projectionLayerViews[i].pose = m_views[i].pose;
      m_projectionLayerViews[i].fov = m_views[i].fov;
      m_projectionLayerViews[i].subImage.swapchain = m_colorSwapchain.handle;
      m_projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
      m_projectionLayerViews[i].subImage.imageRect.extent = {
        (ezInt32)m_Info.m_vEyeRenderTargetSize.width, (ezInt32)m_Info.m_vEyeRenderTargetSize.height};
      m_projectionLayerViews[i].subImage.imageArrayIndex = i;

      if (m_supportsDepth)
      {
        m_depthLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
        m_depthLayerViews[i].minDepth = 0;
        m_depthLayerViews[i].maxDepth = 1;
        m_depthLayerViews[i].nearZ = 0; // TODO
        m_depthLayerViews[i].farZ = 1;  // TODO
        m_depthLayerViews[i].subImage.swapchain = m_depthSwapchain.handle;
        m_depthLayerViews[i].subImage.imageRect.offset = {0, 0};
        m_depthLayerViews[i].subImage.imageRect.extent = {
          (ezInt32)m_Info.m_vEyeRenderTargetSize.width, (ezInt32)m_Info.m_vEyeRenderTargetSize.height};
        m_depthLayerViews[i].subImage.imageArrayIndex = i;

        m_projectionLayerViews[i].next = &m_depthLayerViews[i];
      }
    }
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    XR_CHECK_LOG(xrReleaseSwapchainImage(m_colorSwapchain.handle, &releaseInfo));
    if (m_supportsDepth)
    {
      XR_CHECK_LOG(xrReleaseSwapchainImage(m_depthSwapchain.handle, &releaseInfo));
    }

    m_layer.space = m_StageSpace == ezVRStageSpace::Standing ? m_sceneSpace : m_localSpace;
    m_layer.viewCount = 2;
    m_layer.views = m_projectionLayerViews;

    ezHybridArray<XrCompositionLayerBaseHeader*, 1> layers;
    layers.PushBack(reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layer));

    // Submit the composition layers for the predicted display time.
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_blendMode;
    frameEndInfo.layerCount = layers.GetCapacity();
    frameEndInfo.layers = layers.GetData();

    XrResult result = xrEndFrame(m_session, &frameEndInfo);
    if (result != XR_SUCCESS)
    {
      // TODO?
    }

  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeginAppTick)
  {
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::AfterPresent)
  {
    ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
    ezGALContext* pGALContext = pDevice->GetPrimaryContext();
    ezRenderContext* m_pRenderContext = ezRenderContext::GetDefaultInstance();

    if (const ezGALTexture* tex = pDevice->GetTexture(m_hCompanionRenderTarget))
    {
      // We are rendering the companion window at the very start of the frame, using the content
      // of the last frame. That way we do not add additional delay before submitting the frames.
      EZ_PROFILE_AND_MARKER(pGALContext, "VR CompanionView");

      m_pRenderContext->BindMeshBuffer(
        ezGALBufferHandle(), ezGALBufferHandle(), nullptr, ezGALPrimitiveTopology::Triangles, 1);
      m_pRenderContext->BindConstantBuffer("ezVRCompanionViewConstants", m_hCompanionConstantBuffer);
      m_pRenderContext->BindShader(m_hCompanionShader);

      auto hRenderTargetView = ezGALDevice::GetDefaultDevice()->GetDefaultRenderTargetView(m_hCompanionRenderTarget);
      ezVec2 targetSize = ezVec2((float)tex->GetDescription().m_uiWidth, (float)tex->GetDescription().m_uiHeight);

      ezGALRenderTargetSetup renderTargetSetup;
      renderTargetSetup.SetRenderTarget(0, hRenderTargetView);
      pGALContext->SetRenderTargetSetup(renderTargetSetup);
      pGALContext->SetViewport(ezRectFloat(targetSize.x, targetSize.y));

      auto* constants = ezRenderContext::GetConstantBufferData<ezVRCompanionViewConstants>(m_hCompanionConstantBuffer);
      constants->TargetSize = targetSize;

      ezGALResourceViewHandle hInputView = pDevice->GetDefaultResourceView(m_hColorRT);
      m_pRenderContext->BindTexture2D("VRTexture", hInputView);
      m_pRenderContext->DrawMeshBuffer();
    }
  }
}

void ezOpenXR::OnBeginRender(ezUInt64)
{
  // TODO: Ideally we would like to call UpdatePoses() here and block and in BeforeBeginFrame
  // we would predict the pose in two frames.
}

void ezOpenXR::ReadHMDInfo()
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  // m_Info.m_sDeviceName = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
  // m_Info.m_sDeviceDriver = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
  // m_pHMD->GetRecommendedRenderTargetSize(&m_Info.m_vEyeRenderTargetSize.x, &m_Info.m_vEyeRenderTargetSize.y);
  // m_Info.m_mat4eyePosLeft = GetHMDEyePose(vr::Eye_Left);
  // m_Info.m_mat4eyePosRight = GetHMDEyePose(vr::Eye_Right);
}


void ezOpenXR::OnDeviceActivated(ezVRDeviceID uiDeviceID)
{
  /*m_DeviceState[uiDeviceID].m_bDeviceIsConnected = true;
  switch (m_pHMD->GetTrackedDeviceClass(uiDeviceID))
  {
    case vr::TrackedDeviceClass_HMD:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::HMD;
      break;
    case vr::TrackedDeviceClass_Controller:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Controller;

      break;
    case vr::TrackedDeviceClass_GenericTracker:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Tracker;
      break;
    case vr::TrackedDeviceClass_TrackingReference:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Reference;
      break;
    default:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Unknown;
      break;
  }*/

  UpdateHands();

  {
    ezVRDeviceEvent e;
    e.m_Type = ezVRDeviceEvent::Type::DeviceAdded;
    e.uiDeviceID = uiDeviceID;
    m_DeviceEvents.Broadcast(e);
  }
}


void ezOpenXR::OnDeviceDeactivated(ezVRDeviceID uiDeviceID)
{
  /*m_DeviceState[uiDeviceID].m_bDeviceIsConnected = false;
  m_DeviceState[uiDeviceID].m_bPoseIsValid = false;
  UpdateHands();
  {
    ezVRDeviceEvent e;
    e.m_Type = ezVRDeviceEvent::Type::DeviceRemoved;
    e.uiDeviceID = uiDeviceID;
    m_DeviceEvents.Broadcast(e);
  }*/
}

void ezOpenXR::UpdatePoses()
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  ezStopwatch sw;

  UpdateHands();

  XrViewState viewState{XR_TYPE_VIEW_STATE};
  ezUInt32 viewCapacityInput = 2;
  ezUInt32 viewCountOutput;

  XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
  viewLocateInfo.viewConfigurationType = m_primaryViewConfigurationType;
  viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
  viewLocateInfo.space = ezVRStageSpace::Standing ? m_sceneSpace : m_localSpace;
  m_views[0].type = XR_TYPE_VIEW;
  m_views[1].type = XR_TYPE_VIEW;
  XrResult res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views);
  if (res != XR_SUCCESS)
  {
    m_DeviceState[0].m_bPoseIsValid = false;
    return;
  }

  UpdateCamera();
}

void ezOpenXR::UpdateHands()
{


  // m_iLeftControllerDeviceID = m_pHMD->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
  // m_iRightControllerDeviceID = m_pHMD->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
}

void ezOpenXR::SetStageSpace(ezVRStageSpace::Enum space)
{
  m_StageSpace = space;
  // switch (space)
  //{
  //  case ezVRStageSpace::Seated:
  //    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseOrigin::TrackingUniverseSeated);
  //    break;
  //  case ezVRStageSpace::Standing:
  //    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseOrigin::TrackingUniverseStanding);
  //    break;
  //}
}

void ezOpenXR::SetHMDCamera(ezCamera* pCamera)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  if (m_pCameraToSynchronize == pCamera)
    return;

  m_pCameraToSynchronize = pCamera;
  if (m_pCameraToSynchronize)
  {
    m_uiSettingsModificationCounter = m_pCameraToSynchronize->GetSettingsModificationCounter() + 1;
    m_pCameraToSynchronize->SetCameraMode(ezCameraMode::Stereo, m_pCameraToSynchronize->GetFovOrDim(),
      m_pCameraToSynchronize->GetNearPlane(), m_pCameraToSynchronize->GetFarPlane());
  }

  if (ezSoundInterface* pSoundInterface = ezSingletonRegistry::GetSingletonInstance<ezSoundInterface>())
  {
    pSoundInterface->SetListenerOverrideMode(m_pCameraToSynchronize != nullptr);
  }
}


void ezOpenXR::UpdateCamera()
{
  if (!m_pCameraToSynchronize)
  {
    m_DeviceState[0].m_bPoseIsValid = false;
    m_DeviceState[0].m_bDeviceIsConnected = false;
    return;
  }
  // Update camera projection
  if (m_uiSettingsModificationCounter != m_pCameraToSynchronize->GetSettingsModificationCounter())
  {
    const float fAspectRatio = (float)m_Info.m_vEyeRenderTargetSize.width / (float)m_Info.m_vEyeRenderTargetSize.height;
    auto CreateProjection = [](const XrView& view, ezCamera* cam) {
      return ezGraphicsUtils::CreatePerspectiveProjectionMatrix(
        ezMath::Tan(ezAngle::Radian(view.fov.angleLeft)) * cam->GetNearPlane(),
        ezMath::Tan(ezAngle::Radian(view.fov.angleRight)) * cam->GetNearPlane(),
        ezMath::Tan(ezAngle::Radian(view.fov.angleDown)) * cam->GetNearPlane(),
        ezMath::Tan(ezAngle::Radian(view.fov.angleUp)) * cam->GetNearPlane(), cam->GetNearPlane(), cam->GetFarPlane());
    };

    // Update projection with newest near/ far values. If not sync camera is set, just use the last value from VR
    // camera.
    const ezMat4 projLeft = CreateProjection(m_views[0], m_pCameraToSynchronize);
    const ezMat4 projRight = CreateProjection(m_views[1], m_pCameraToSynchronize);
    m_pCameraToSynchronize->SetStereoProjection(projLeft, projRight, fAspectRatio);
    m_uiSettingsModificationCounter = m_pCameraToSynchronize->GetSettingsModificationCounter();
  }

  // Update camera view
  {
    ezTransform add;
    add.SetIdentity();
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      if (const ezWorld* pWorld = pView->GetWorld())
      {
        EZ_LOCK(pWorld->GetReadMarker());
        if (const ezStageSpaceComponentManager* pStageMan = pWorld->GetComponentManager<ezStageSpaceComponentManager>())
        {
          if (const ezStageSpaceComponent* pStage = pStageMan->GetSingletonComponent())
          {
            ezEnum<ezVRStageSpace> stageSpace = pStage->GetStageSpace();
            if (m_StageSpace != stageSpace)
              SetStageSpace(pStage->GetStageSpace());
            add = pStage->GetOwner()->GetGlobalTransform();
          }
        }
      }
    }

    {
      // Update device state (average of both eyes).
      ezQuat rot;
      rot.SetIdentity();
      rot.SetSlerp(
        ConvertOrientation(m_views[0].pose.orientation), ConvertOrientation(m_views[1].pose.orientation), 0.5f);
      const ezVec3 pos =
        ezMath::Lerp(ConvertPosition(m_views[0].pose.position), ConvertPosition(m_views[1].pose.position), 0.5f);

      m_DeviceState[0].m_mPose.SetTransformationMatrix(rot.GetAsMat3(), pos);
      m_DeviceState[0].m_vPosition = pos;
      m_DeviceState[0].m_qRotation = rot;
      m_DeviceState[0].m_Type = ezVRDeviceState::Type::HMD;
      m_DeviceState[0].m_bPoseIsValid = true;
      m_DeviceState[0].m_bDeviceIsConnected = true;
    }

    // Set view matrix
    {
      const ezMat4 mStageTransform = add.GetAsMat4();
      const ezMat4 poseLeft = mStageTransform * ConvertPoseToMatrix(m_views[0].pose);
      const ezMat4 poseRight = mStageTransform * ConvertPoseToMatrix(m_views[1].pose);

      // EZ Forward is +X, need to add this to align the forward projection
      const ezMat4 viewMatrix =
        ezGraphicsUtils::CreateLookAtViewMatrix(ezVec3::ZeroVector(), ezVec3(1, 0, 0), ezVec3(0, 0, 1));
      const ezMat4 mViewTransformLeft = viewMatrix * poseLeft.GetInverse();
      const ezMat4 mViewTransformRight = viewMatrix * poseRight.GetInverse();

      m_pCameraToSynchronize->SetViewMatrix(mViewTransformLeft, ezCameraEye::Left);
      m_pCameraToSynchronize->SetViewMatrix(mViewTransformRight, ezCameraEye::Right);
    }
  }

  {
    // put the camera orientation into the sound listener and enable the listener override mode
    if (ezSoundInterface* pSoundInterface = ezSingletonRegistry::GetSingletonInstance<ezSoundInterface>())
    {
      pSoundInterface->SetListener(-1, m_pCameraToSynchronize->GetCenterPosition(),
        m_pCameraToSynchronize->GetCenterDirForwards(), m_pCameraToSynchronize->GetCenterDirUp(), ezVec3::ZeroVector());
    }
  }
}

XrPosef ezOpenXR::ConvertTransform(const ezTransform& tr)
{
  XrPosef pose;
  pose.orientation = ConvertOrientation(tr.m_qRotation);
  pose.position = ConvertPosition(tr.m_vPosition);
  return pose;
}

XrQuaternionf ezOpenXR::ConvertOrientation(const ezQuat& q)
{
  return {q.v.y, q.v.z, -q.v.x, -q.w};
}

XrVector3f ezOpenXR::ConvertPosition(const ezVec3& pos)
{
  return {pos.y, pos.z, -pos.x};
}

ezQuat ezOpenXR::ConvertOrientation(const XrQuaternionf& q)
{
  return {-q.z, q.x, q.y, -q.w};
}

ezVec3 ezOpenXR::ConvertPosition(const XrVector3f& pos)
{
  return {-pos.z, pos.x, pos.y};
}

ezMat4 ezOpenXR::ConvertPoseToMatrix(const XrPosef& pose)
{
  ezMat4 m;
  ezMat3 rot = ConvertOrientation(pose.orientation).GetAsMat3();
  ezVec3 pos = ConvertPosition(pose.position);
  m.SetTransformationMatrix(rot, pos);
  return m;
}

ezGALResourceFormat::Enum ezOpenXR::ConvertTextureFormat(int64_t format)
{
  switch (format)
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return ezGALResourceFormat::RGBAUByteNormalizedsRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return ezGALResourceFormat::BGRAUByteNormalizedsRGB;
    case DXGI_FORMAT_D32_FLOAT:
      return ezGALResourceFormat::DFloat;
    case DXGI_FORMAT_D16_UNORM:
      return ezGALResourceFormat::D16;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
      return ezGALResourceFormat::D24S8;
    default:
      EZ_REPORT_FAILURE("Unknown format {}", format);
      break;
  }
  return ezGALResourceFormat::Invalid;
}

ezGALMSAASampleCount::Enum ezOpenXR::ConvertMSAACount(ezUInt32 count)
{
  switch (count)
  {
    case 1:
      return ezGALMSAASampleCount::None;
    case 2:
      return ezGALMSAASampleCount::TwoSamples;
    case 4:
      return ezGALMSAASampleCount::FourSamples;
    case 8:
      return ezGALMSAASampleCount::EightSamples;
    default:
      EZ_REPORT_FAILURE("Unknown sample count {}", count);
      break;
  }
  return ezGALMSAASampleCount::None;
}

//////////////////////////////////////////////////////////////////////////

ezGALOpenXRSwapChainDX11::ezGALOpenXRSwapChainDX11(const ezGALSwapChainCreationDescription& Description)
  : ezGALSwapChain(Description)
{
}

ezGALOpenXRSwapChainDX11::~ezGALOpenXRSwapChainDX11() {}

ezResult ezGALOpenXRSwapChainDX11::InitPlatform(ezGALDevice* pDevice)
{
  return EZ_SUCCESS;
}

ezResult ezGALOpenXRSwapChainDX11::DeInitPlatform(ezGALDevice* pDevice)
{
  return EZ_SUCCESS;
}

void ezGALOpenXRSwapChainDX11::SetSwapchainTexture(ezGALTextureHandle hBackBufferTexture)
{
  m_hBackBufferTexture = hBackBufferTexture;
}

EZ_STATICLINK_FILE(OpenXRPlugin, OpenXRPlugin_OpenXRSingleton);

#include <RendererVulkan/RendererVulkanPCH.h>

#if EZ_ENABLED(EZ_PLATFORM_WINDOWS)
#  include <Foundation/Basics/Platform/Win/IncludeWindows.h>
#endif

#include <Core/System/Window.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <Foundation/System/PlatformFeatures.h>
#include <RendererFoundation/CommandEncoder/RenderCommandEncoder.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <RendererFoundation/Device/SwapChain.h>
#include <RendererFoundation/Profiling/Profiling.h>
#include <RendererVulkan/Cache/ResourceCacheVulkan.h>
#include <RendererVulkan/CommandEncoder/CommandEncoderImplVulkan.h>
#include <RendererVulkan/Device/DeviceVulkan.h>
#include <RendererVulkan/Device/InitContext.h>
#include <RendererVulkan/Device/PassVulkan.h>
#include <RendererVulkan/Device/SwapChainVulkan.h>
#include <RendererVulkan/Pools/CommandBufferPoolVulkan.h>
#include <RendererVulkan/Pools/DescriptorSetPoolVulkan.h>
#include <RendererVulkan/Pools/FencePoolVulkan.h>
#include <RendererVulkan/Pools/QueryPoolVulkan.h>
#include <RendererVulkan/Pools/SemaphorePoolVulkan.h>
#include <RendererVulkan/Pools/StagingBufferPoolVulkan.h>
#include <RendererVulkan/Resources/BufferVulkan.h>
#include <RendererVulkan/Resources/FallbackResourcesVulkan.h>
#include <RendererVulkan/Resources/QueryVulkan.h>
#include <RendererVulkan/Resources/RenderTargetViewVulkan.h>
#include <RendererVulkan/Resources/ResourceViewVulkan.h>
#include <RendererVulkan/Resources/TextureVulkan.h>
#include <RendererVulkan/Resources/UnorderedAccessViewVulkan.h>
#include <RendererVulkan/Shader/ShaderVulkan.h>
#include <RendererVulkan/Shader/VertexDeclarationVulkan.h>
#include <RendererVulkan/State/StateVulkan.h>
#include <RendererVulkan/Utils/ImageCopyVulkan.h>
#include <RendererVulkan/Utils/PipelineBarrierVulkan.h>

#if EZ_ENABLED(EZ_SUPPORTS_GLFW)
#  include <GLFW/glfw3.h>
#endif

EZ_DEFINE_AS_POD_TYPE(VkLayerProperties);

namespace
{
  VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
  {
    switch (messageSeverity)
    {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        ezLog::Debug("VK: {}", pCallbackData->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        ezLog::Info("VK: {}", pCallbackData->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        ezLog::Warning("VK: {}", pCallbackData->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        ezLog::Error("VK: {}", pCallbackData->pMessage);
        break;
      default:
        break;
    }
    // Only layers are allowed to return true here.
    return VK_FALSE;
  }

  bool isInstanceLayerPresent(const char* layerName)
  {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    ezDynamicArray<VkLayerProperties> availableLayers;
    availableLayers.SetCountUninitialized(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.GetData());

    for (const auto& layerProperties : availableLayers)
    {
      if (strcmp(layerName, layerProperties.layerName) == 0)
      {
        return true;
      }
    }

    return false;
  }
} // namespace

// Need to implement these extension functions so vulkan hpp can call them.
// They're basically just adapters calling the function pointer retrieved previously.

PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXTFunc;
PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXTFunc;
PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXTFunc;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXTFunc;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXTFunc;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXTFunc;


VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pObjectName)
{
  return vkSetDebugUtilsObjectNameEXTFunc(device, pObjectName);
}

void vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
{
  return vkQueueBeginDebugUtilsLabelEXTFunc(queue, pLabelInfo);
}

void vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
{
  return vkQueueEndDebugUtilsLabelEXTFunc(queue);
}
//
// void vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
//{
//  return vkQueueInsertDebugUtilsLabelEXTFunc(queue, pLabelInfo);
//}

void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
  return vkCmdBeginDebugUtilsLabelEXTFunc(commandBuffer, pLabelInfo);
}

void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
  return vkCmdEndDebugUtilsLabelEXTFunc(commandBuffer);
}

void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
  return vkCmdInsertDebugUtilsLabelEXTFunc(commandBuffer, pLabelInfo);
}

ezInternal::NewInstance<ezGALDevice> CreateVulkanDevice(ezAllocatorBase* pAllocator, const ezGALDeviceCreationDescription& Description)
{
  return EZ_NEW(pAllocator, ezGALDeviceVulkan, Description);
}

// clang-format off
EZ_BEGIN_SUBSYSTEM_DECLARATION(RendererVulkan, DeviceFactory)

ON_CORESYSTEMS_STARTUP
{
  ezGALDeviceFactory::RegisterCreatorFunc("Vulkan", &CreateVulkanDevice, "VULKAN", "ezShaderCompilerDXC");
}

ON_CORESYSTEMS_SHUTDOWN
{
  ezGALDeviceFactory::UnregisterCreatorFunc("Vulkan");
}

EZ_END_SUBSYSTEM_DECLARATION;
// clang-format on

ezGALDeviceVulkan::ezGALDeviceVulkan(const ezGALDeviceCreationDescription& Description)
  : ezGALDevice(Description)
{
}

ezGALDeviceVulkan::~ezGALDeviceVulkan() = default;

// Init & shutdown functions


vk::Result ezGALDeviceVulkan::SelectInstanceExtensions(ezHybridArray<const char*, 6>& extensions)
{
  // Fetch the list of extensions supported by the runtime.
  ezUInt32 extensionCount;
  VK_SUCCEED_OR_RETURN_LOG(vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
  ezDynamicArray<vk::ExtensionProperties> extensionProperties;
  extensionProperties.SetCount(extensionCount);
  VK_SUCCEED_OR_RETURN_LOG(vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.GetData()));

  EZ_LOG_BLOCK("InstanceExtensions");
  for (auto& ext : extensionProperties)
  {
    ezLog::Info("{}", ext.extensionName.data());
  }

  // Add a specific extension to the list of extensions to be enabled, if it is supported.
  auto AddExtIfSupported = [&](const char* extensionName, bool& enableFlag) -> vk::Result {
    auto it = std::find_if(begin(extensionProperties), end(extensionProperties), [&](const vk::ExtensionProperties& prop) { return ezStringUtils::IsEqual(prop.extensionName.data(), extensionName); });
    if (it != end(extensionProperties))
    {
      extensions.PushBack(extensionName);
      enableFlag = true;
      return vk::Result::eSuccess;
    }
    enableFlag = false;
    return vk::Result::eErrorExtensionNotPresent;
  };

  VK_SUCCEED_OR_RETURN_LOG(AddExtIfSupported(VK_KHR_SURFACE_EXTENSION_NAME, m_extensions.m_bSurface));
#if EZ_ENABLED(EZ_SUPPORTS_GLFW)
  uint32_t iNumGlfwExtensions = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&iNumGlfwExtensions);
  bool dummy = false;
  for (uint32_t i = 0; i < iNumGlfwExtensions; ++i)
  {
    VK_SUCCEED_OR_RETURN_LOG(AddExtIfSupported(glfwExtensions[i], dummy));
  }
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
  VK_SUCCEED_OR_RETURN_LOG(AddExtIfSupported(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, m_extensions.m_bWin32Surface));
#else
#  error "Vulkan platform not supported"
#endif
  VK_SUCCEED_OR_RETURN_LOG(AddExtIfSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, m_extensions.m_bDebugUtils));

  return vk::Result::eSuccess;
}


vk::Result ezGALDeviceVulkan::SelectDeviceExtensions(vk::DeviceCreateInfo& deviceCreateInfo, ezHybridArray<const char*, 6>& extensions)
{
  // Fetch the list of extensions supported by the runtime.
  ezUInt32 extensionCount;
  VK_SUCCEED_OR_RETURN_LOG(m_physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr));
  ezDynamicArray<vk::ExtensionProperties> extensionProperties;
  extensionProperties.SetCount(extensionCount);
  VK_SUCCEED_OR_RETURN_LOG(m_physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, extensionProperties.GetData()));

  EZ_LOG_BLOCK("DeviceExtensions");
  for (auto& ext : extensionProperties)
  {
    ezLog::Info("{}", ext.extensionName.data());
  }

  // Add a specific extension to the list of extensions to be enabled, if it is supported.
  auto AddExtIfSupported = [&](const char* extensionName, bool& enableFlag) -> vk::Result {
    auto it = std::find_if(begin(extensionProperties), end(extensionProperties), [&](const vk::ExtensionProperties& prop) { return ezStringUtils::IsEqual(prop.extensionName.data(), extensionName); });
    if (it != end(extensionProperties))
    {
      extensions.PushBack(extensionName);
      enableFlag = true;
      return vk::Result::eSuccess;
    }
    enableFlag = false;
    ezLog::Warning("Extension '{}' not supported", extensionName);
    return vk::Result::eErrorExtensionNotPresent;
  };

  VK_SUCCEED_OR_RETURN_LOG(AddExtIfSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME, m_extensions.m_bDeviceSwapChain));
  AddExtIfSupported(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME, m_extensions.m_bShaderViewportIndexLayer);

  vk::PhysicalDeviceFeatures2 features;
  features.pNext = &m_extensions.m_borderColorEXT;
  m_physicalDevice.getFeatures2(&features);

  m_supportedStages = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader;
  if (features.features.geometryShader)
  {
    m_supportedStages |= vk::PipelineStageFlagBits::eGeometryShader;
  }
  else
  {
    ezLog::Warning("Geometry shaders are not supported.");
  }

  if (features.features.tessellationShader)
  {
    m_supportedStages |= vk::PipelineStageFlagBits::eTessellationControlShader | vk::PipelineStageFlagBits::eTessellationEvaluationShader;
  }
  else
  {
    ezLog::Warning("Tessellation shaders are not supported.");
  }

  // Only use the extension if it allows us to not specify a format or we would need to create different samplers for every texture.
  if (m_extensions.m_borderColorEXT.customBorderColors && m_extensions.m_borderColorEXT.customBorderColorWithoutFormat)
  {
    AddExtIfSupported(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, m_extensions.m_bBorderColorFloat);
    if (m_extensions.m_bBorderColorFloat)
    {
      deviceCreateInfo.pNext = &m_extensions.m_borderColorEXT;
    }
  }
  AddExtIfSupported(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, m_extensions.m_bImageFormatList);

  return vk::Result::eSuccess;
}

#define EZ_GET_INSTANCE_PROC_ADDR(name) m_extensions.pfn_##name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(m_instance, #name));

ezResult ezGALDeviceVulkan::InitPlatform()
{
  EZ_LOG_BLOCK("ezGALDeviceVulkan::InitPlatform");

  const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
  {
    // Create instance
    // We use Vulkan 1.1 because of two features:
    // 1. Descriptor set pools return vk::Result::eErrorOutOfPoolMemory if exhaused. Removing the requirement to count usage yourself.
    // 2. Viewport height can be negative which performs y-inversion of the clip-space to framebuffer-space transform.
    vk::ApplicationInfo applicationInfo = {};
    applicationInfo.apiVersion = VK_API_VERSION_1_1;
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // TODO put ezEngine version here
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);      // TODO put ezEngine version here
    applicationInfo.pApplicationName = "ezEngine";
    applicationInfo.pEngineName = "ezEngine";

    ezHybridArray<const char*, 6> instanceExtensions;
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(SelectInstanceExtensions(instanceExtensions));

    vk::InstanceCreateInfo instanceCreateInfo;
    // enabling support for win32 surfaces
    instanceCreateInfo.pApplicationInfo = &applicationInfo;

    instanceCreateInfo.enabledExtensionCount = instanceExtensions.GetCount();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.GetData();

    instanceCreateInfo.enabledLayerCount = 0;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_Description.m_bDebugDevice)
    {
      debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugCreateInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debugCreateInfo.pfnUserCallback = debugCallback;
      debugCreateInfo.pUserData = nullptr;

      if (isInstanceLayerPresent(layers[0]))
      {
        instanceCreateInfo.enabledLayerCount = EZ_ARRAY_SIZE(layers);
        instanceCreateInfo.ppEnabledLayerNames = layers;
      }
      else
      {
        ezLog::Warning("The khronos validation layer is not supported on this device. Will run without validation layer.");
      }
      instanceCreateInfo.pNext = &debugCreateInfo;
    }

    m_instance = vk::createInstance(instanceCreateInfo);

#if EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
    if (m_Description.m_bDebugDevice)
    {
      EZ_GET_INSTANCE_PROC_ADDR(vkCreateDebugUtilsMessengerEXT);
      EZ_GET_INSTANCE_PROC_ADDR(vkDestroyDebugUtilsMessengerEXT);
      EZ_GET_INSTANCE_PROC_ADDR(vkSetDebugUtilsObjectNameEXT);
      VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_extensions.pfn_vkCreateDebugUtilsMessengerEXT(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger));
    }
#endif

    if (!m_instance)
    {
      ezLog::Error("Failed to create Vulkan instance!");
      return EZ_FAILURE;
    }
  }

  {
    // physical device
    ezUInt32 physicalDeviceCount = 0;
    ezHybridArray<vk::PhysicalDevice, 2> physicalDevices;
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_instance.enumeratePhysicalDevices(&physicalDeviceCount, nullptr));
    if (physicalDeviceCount == 0)
    {
      ezLog::Error("No available physical device to create a Vulkan device on!");
      return EZ_FAILURE;
    }

    physicalDevices.SetCount(physicalDeviceCount);
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_instance.enumeratePhysicalDevices(&physicalDeviceCount, physicalDevices.GetData()));

    // TODO choosable physical device?
    // TODO making sure we have a hardware device?
    m_physicalDevice = physicalDevices[0];
    m_properties = m_physicalDevice.getProperties();
    ezLog::Info("Selected physical device \"{}\" for device creation.", m_properties.deviceName);
  }

  ezHybridArray<vk::QueueFamilyProperties, 4> queueFamilyProperties;
  {
    // Device
    ezUInt32 queueFamilyPropertyCount = 0;
    m_physicalDevice.getQueueFamilyProperties(&queueFamilyPropertyCount, nullptr);
    if (queueFamilyPropertyCount == 0)
    {
      ezLog::Error("No available device queues on physical device!");
      return EZ_FAILURE;
    }
    queueFamilyProperties.SetCount(queueFamilyPropertyCount);
    m_physicalDevice.getQueueFamilyProperties(&queueFamilyPropertyCount, queueFamilyProperties.GetData());

    {
      EZ_LOG_BLOCK("Queue Families");
      for (ezUInt32 i = 0; i < queueFamilyProperties.GetCount(); ++i)
      {
        const vk::QueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[i];
        ezLog::Info("Queue count: {}, flags: {}", queueFamilyProperty.queueCount, vk::to_string(queueFamilyProperty.queueFlags).data());
      }
    }

    // Select best queue family for graphics and transfers.
    for (ezUInt32 i = 0; i < queueFamilyProperties.GetCount(); ++i)
    {
      const vk::QueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[i];
      if (queueFamilyProperty.queueCount == 0)
        continue;
      constexpr auto graphicsFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
      if ((queueFamilyProperty.queueFlags & graphicsFlags) == graphicsFlags)
      {
        m_graphicsQueue.m_uiQueueFamily = i;
      }
      if (queueFamilyProperty.queueFlags & vk::QueueFlagBits::eTransfer)
      {
        if (m_transferQueue.m_uiQueueFamily == -1)
        {
          m_transferQueue.m_uiQueueFamily = i;
        }
        else if ((queueFamilyProperty.queueFlags & graphicsFlags) == vk::QueueFlagBits())
        {
          // Prefer a queue that can't be used for graphics.
          m_transferQueue.m_uiQueueFamily = i;
        }
      }
    }
    if (m_graphicsQueue.m_uiQueueFamily == -1)
    {
      ezLog::Error("No graphics queue found.");
      return EZ_FAILURE;
    }
    if (m_transferQueue.m_uiQueueFamily == -1)
    {
      ezLog::Error("No transfer queue found.");
      return EZ_FAILURE;
    }

    constexpr float queuePriority = 0.f;

    ezHybridArray<vk::DeviceQueueCreateInfo, 2> queues;

    vk::DeviceQueueCreateInfo& graphicsQueueCreateInfo = queues.ExpandAndGetRef();
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.queueFamilyIndex = m_graphicsQueue.m_uiQueueFamily;
    if (m_graphicsQueue.m_uiQueueFamily != m_transferQueue.m_uiQueueFamily)
    {
      vk::DeviceQueueCreateInfo& transferQueueCreateInfo = queues.ExpandAndGetRef();
      transferQueueCreateInfo.pQueuePriorities = &queuePriority;
      transferQueueCreateInfo.queueCount = 1;
      transferQueueCreateInfo.queueFamilyIndex = m_transferQueue.m_uiQueueFamily;
    }

    // #TODO_VULKAN test that this returns the same as 'layers' passed into the instance.
    ezUInt32 uiLayers;
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_physicalDevice.enumerateDeviceLayerProperties(&uiLayers, nullptr));
    ezDynamicArray<vk::LayerProperties> deviceLayers;
    deviceLayers.SetCount(uiLayers);
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_physicalDevice.enumerateDeviceLayerProperties(&uiLayers, deviceLayers.GetData()));

    vk::DeviceCreateInfo deviceCreateInfo = {};
    ezHybridArray<const char*, 6> deviceExtensions;
    VK_SUCCEED_OR_RETURN_EZ_FAILURE(SelectDeviceExtensions(deviceCreateInfo, deviceExtensions));

    deviceCreateInfo.enabledExtensionCount = deviceExtensions.GetCount();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.GetData();
    // Device layers are deprecated but provided (same as in instance) for backwards compatibility.
    deviceCreateInfo.enabledLayerCount = EZ_ARRAY_SIZE(layers);
    deviceCreateInfo.ppEnabledLayerNames = layers;

    vk::PhysicalDeviceFeatures physicalDeviceFeatures = m_physicalDevice.getFeatures();
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures; // Enabling all available features for now
    deviceCreateInfo.queueCreateInfoCount = queues.GetCount();
    deviceCreateInfo.pQueueCreateInfos = queues.GetData();

    VK_SUCCEED_OR_RETURN_EZ_FAILURE(m_physicalDevice.createDevice(&deviceCreateInfo, nullptr, &m_device));
    m_device.getQueue(m_graphicsQueue.m_uiQueueFamily, m_graphicsQueue.m_uiQueueIndex, &m_graphicsQueue.m_queue);
    m_device.getQueue(m_transferQueue.m_uiQueueFamily, m_transferQueue.m_uiQueueIndex, &m_transferQueue.m_queue);
  }

  VK_SUCCEED_OR_RETURN_EZ_FAILURE(ezMemoryAllocatorVulkan::Initialize(m_physicalDevice, m_device, m_instance));

  vkSetDebugUtilsObjectNameEXTFunc = (PFN_vkSetDebugUtilsObjectNameEXT)m_device.getProcAddr("vkSetDebugUtilsObjectNameEXT");
  vkQueueBeginDebugUtilsLabelEXTFunc = (PFN_vkQueueBeginDebugUtilsLabelEXT)m_device.getProcAddr("vkQueueBeginDebugUtilsLabelEXT");
  vkQueueEndDebugUtilsLabelEXTFunc = (PFN_vkQueueEndDebugUtilsLabelEXT)m_device.getProcAddr("vkQueueEndDebugUtilsLabelEXT");
  vkCmdBeginDebugUtilsLabelEXTFunc = (PFN_vkCmdBeginDebugUtilsLabelEXT)m_device.getProcAddr("vkCmdBeginDebugUtilsLabelEXT");
  vkCmdEndDebugUtilsLabelEXTFunc = (PFN_vkCmdEndDebugUtilsLabelEXT)m_device.getProcAddr("vkCmdEndDebugUtilsLabelEXT");
  vkCmdInsertDebugUtilsLabelEXTFunc = (PFN_vkCmdInsertDebugUtilsLabelEXT)m_device.getProcAddr("vkCmdInsertDebugUtilsLabelEXT");


  m_memoryProperties = m_physicalDevice.getMemoryProperties();

  // Fill lookup table
  FillFormatLookupTable();

  ezClipSpaceDepthRange::Default = ezClipSpaceDepthRange::ZeroToOne;
  // We use ezClipSpaceYMode::Regular and rely in the Vulkan 1.1 feature that a negative height performs y-inversion of the clip-space to framebuffer-space transform.
  // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_maintenance1.html
  ezClipSpaceYMode::RenderToTextureDefault = ezClipSpaceYMode::Regular;

  m_pPipelineBarrier = EZ_NEW(&m_Allocator, ezPipelineBarrierVulkan);
  m_pCommandBufferPool = EZ_NEW(&m_Allocator, ezCommandBufferPoolVulkan);
  m_pCommandBufferPool->Initialize(m_device, m_graphicsQueue.m_uiQueueFamily);
  m_pStagingBufferPool = EZ_NEW(&m_Allocator, ezStagingBufferPoolVulkan);
  m_pStagingBufferPool->Initialize(this);
  m_pQueryPool = EZ_NEW(&m_Allocator, ezQueryPoolVulkan);
  m_pQueryPool->Initialize(this, queueFamilyProperties[m_graphicsQueue.m_uiQueueFamily].timestampValidBits);
  m_pInitContext = EZ_NEW(&m_Allocator, ezInitContextVulkan, this);

  ezSemaphorePoolVulkan::Initialize(m_device);
  ezFencePoolVulkan::Initialize(m_device);
  ezResourceCacheVulkan::Initialize(this, m_device);
  ezDescriptorSetPoolVulkan::Initialize(m_device);
  ezFallbackResourcesVulkan::Initialize(this);
  ezImageCopyVulkan::Initialize(*this);

  m_pDefaultPass = EZ_NEW(&m_Allocator, ezGALPassVulkan, *this);

  ezGALWindowSwapChain::SetFactoryMethod([this](const ezGALWindowSwapChainCreationDescription& desc) -> ezGALSwapChainHandle { return CreateSwapChain([this, &desc](ezAllocatorBase* pAllocator) -> ezGALSwapChain* { return EZ_NEW(pAllocator, ezGALSwapChainVulkan, desc); }); });

  return EZ_SUCCESS;
}

void ezGALDeviceVulkan::SetDebugName(const vk::DebugUtilsObjectNameInfoEXT& info, ezVulkanAllocation allocation)
{
#if EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
  m_device.setDebugUtilsObjectNameEXT(info);
  if (allocation)
    ezMemoryAllocatorVulkan::SetAllocationUserData(allocation, info.pObjectName);
#endif
}

void ezGALDeviceVulkan::ReportLiveGpuObjects()
{
  // This is automatically done in the validation layer and can't be easily done manually.
}

void ezGALDeviceVulkan::UploadBufferStaging(ezStagingBufferPoolVulkan* pStagingBufferPool, ezPipelineBarrierVulkan* pPipelineBarrier, vk::CommandBuffer commandBuffer, const ezGALBufferVulkan* pBuffer, ezArrayPtr<const ezUInt8> pInitialData, vk::DeviceSize dstOffset)
{
  void* pData = nullptr;

  // #TODO_VULKAN Use transfer queue
  ezStagingBufferVulkan stagingBuffer = pStagingBufferPool->AllocateBuffer(0, pInitialData.GetCount());
  // ezMemoryUtils::Copy(reinterpret_cast<ezUInt8*>(stagingBuffer.m_allocInfo.m_pMappedData), pInitialData.GetPtr(), pInitialData.GetCount());
  ezMemoryAllocatorVulkan::MapMemory(stagingBuffer.m_alloc, &pData);
  ezMemoryUtils::Copy(reinterpret_cast<ezUInt8*>(pData), pInitialData.GetPtr(), pInitialData.GetCount());
  ezMemoryAllocatorVulkan::UnmapMemory(stagingBuffer.m_alloc);

  vk::BufferCopy region;
  region.srcOffset = 0;
  region.dstOffset = dstOffset;
  region.size = pInitialData.GetCount();

  // #TODO_VULKAN atomic min size violation?
  commandBuffer.copyBuffer(stagingBuffer.m_buffer, pBuffer->GetVkBuffer(), 1, &region);

  pPipelineBarrier->AccessBuffer(pBuffer, region.dstOffset, region.size, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, pBuffer->GetUsedByPipelineStage(), pBuffer->GetAccessMask());

  // #TODO_VULKAN Custom delete later / return to ezStagingBufferPoolVulkan once this is on the transfer queue and runs async to graphics queue.
  pStagingBufferPool->ReclaimBuffer(stagingBuffer);
}

void ezGALDeviceVulkan::UploadTextureStaging(ezStagingBufferPoolVulkan* pStagingBufferPool, ezPipelineBarrierVulkan* pPipelineBarrier, vk::CommandBuffer commandBuffer, const ezGALTextureVulkan* pTexture, const vk::ImageSubresourceLayers& subResource, const ezGALSystemMemoryDescription& data)
{
  const vk::Offset3D imageOffset = {0, 0, 0};
  const vk::Extent3D imageExtent = pTexture->GetMipLevelSize(subResource.mipLevel);

  auto getRange = [](const vk::ImageSubresourceLayers& layers) -> vk::ImageSubresourceRange {
    vk::ImageSubresourceRange range;
    range.aspectMask = layers.aspectMask;
    range.baseMipLevel = layers.mipLevel;
    range.levelCount = 1;
    range.baseArrayLayer = layers.baseArrayLayer;
    range.layerCount = layers.layerCount;
    return range;
  };

  pPipelineBarrier->EnsureImageLayout(pTexture, getRange(subResource), vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite);
  pPipelineBarrier->Flush();

  for (ezUInt32 i = 0; i < subResource.layerCount; i++)
  {
    auto pLayerData = reinterpret_cast<const ezUInt8*>(data.m_pData) + i * data.m_uiSlicePitch;
    const vk::Format format = pTexture->GetImageFormat();
    const ezUInt8 uiBlockSize = vk::blockSize(format);
    const auto blockExtent = vk::blockExtent(format);
    const VkExtent3D blockCount = {
      (imageExtent.width + blockExtent[0] - 1) / blockExtent[0],
      (imageExtent.height + blockExtent[1] - 1) / blockExtent[1],
      (imageExtent.depth + blockExtent[2] - 1) / blockExtent[2]};

    const vk::DeviceSize uiTotalSize = uiBlockSize * blockCount.width * blockCount.height * blockCount.depth;
    ezStagingBufferVulkan stagingBuffer = pStagingBufferPool->AllocateBuffer(0, uiTotalSize);

    const ezUInt32 uiBufferRowPitch = uiBlockSize * blockCount.width;
    const ezUInt32 uiBufferSlicePitch = uiBufferRowPitch * blockCount.height;
    EZ_ASSERT_DEV(uiBufferRowPitch == data.m_uiRowPitch, "Row pitch with padding is not implemented yet.");
    EZ_ASSERT_DEV(uiBufferSlicePitch == data.m_uiSlicePitch, "Row pitch with padding is not implemented yet.");

    void* pData = nullptr;
    ezMemoryAllocatorVulkan::MapMemory(stagingBuffer.m_alloc, &pData);
    ezMemoryUtils::Copy(reinterpret_cast<ezUInt8*>(pData), pLayerData, uiTotalSize);
    ezMemoryAllocatorVulkan::UnmapMemory(stagingBuffer.m_alloc);

    vk::BufferImageCopy region = {};
    region.imageSubresource = subResource;
    region.imageOffset = imageOffset;
    region.imageExtent = imageExtent;

    region.bufferOffset = 0;
    region.bufferRowLength = blockExtent[0] * uiBufferRowPitch / uiBlockSize;
    region.bufferImageHeight = blockExtent[1] * uiBufferSlicePitch / uiBufferRowPitch;

    // #TODO_VULKAN atomic min size violation?
    commandBuffer.copyBufferToImage(stagingBuffer.m_buffer, pTexture->GetImage(), pTexture->GetPreferredLayout(vk::ImageLayout::eTransferDstOptimal), 1, &region);
    pStagingBufferPool->ReclaimBuffer(stagingBuffer);
  }

  pPipelineBarrier->EnsureImageLayout(pTexture, getRange(subResource), pTexture->GetPreferredLayout(), pTexture->GetUsedByPipelineStage(), pTexture->GetAccessMask());
}

ezResult ezGALDeviceVulkan::ShutdownPlatform()
{
  ezImageCopyVulkan::DeInitialize(*this);
  DestroyDeadObjects(); // ezImageCopyVulkan might add dead objects, so make sure the list is cleared again

  ezFallbackResourcesVulkan::DeInitialize();

  ezGALWindowSwapChain::SetFactoryMethod({});
  if (m_lastCommandBufferFinished)
    ReclaimLater(m_lastCommandBufferFinished, m_pCommandBufferPool.Borrow());
  auto& pCommandEncoder = m_pDefaultPass->m_pCommandEncoderImpl;

  // We couldn't create a device in the first place, so early out of shutdown
  if (!m_device)
  {
    return EZ_SUCCESS;
  }

  WaitIdlePlatform();

  m_pDefaultPass = nullptr;
  m_pPipelineBarrier = nullptr;
  m_pCommandBufferPool->DeInitialize();
  m_pCommandBufferPool = nullptr;
  m_pStagingBufferPool->DeInitialize();
  m_pStagingBufferPool = nullptr;
  m_pQueryPool->DeInitialize();
  m_pQueryPool = nullptr;
  m_pInitContext = nullptr;

  ezSemaphorePoolVulkan::DeInitialize();
  ezFencePoolVulkan::DeInitialize();
  ezResourceCacheVulkan::DeInitialize();
  ezDescriptorSetPoolVulkan::DeInitialize();

  ezMemoryAllocatorVulkan::DeInitialize();

  m_device.waitIdle();
  m_device.destroy();

#if EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
  if (m_extensions.pfn_vkDestroyDebugUtilsMessengerEXT != nullptr)
  {
    m_extensions.pfn_vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
  }
#endif

  m_instance.destroy();
  ReportLiveGpuObjects();

  return EZ_SUCCESS;
}

// Pipeline & Pass functions

vk::CommandBuffer& ezGALDeviceVulkan::GetCurrentCommandBuffer()
{
  vk::CommandBuffer& commandBuffer = m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer;
  if (!commandBuffer)
  {
    // Restart new command buffer if none is active already.
    commandBuffer = m_pCommandBufferPool->RequestCommandBuffer();
    vk::CommandBufferBeginInfo beginInfo;
    VK_ASSERT_DEBUG(commandBuffer.begin(&beginInfo));
    GetCurrentPipelineBarrier().SetCommandBuffer(&commandBuffer);

    m_pDefaultPass->SetCurrentCommandBuffer(&commandBuffer, m_pPipelineBarrier.Borrow());
    // We can't carry state across individual command buffers.
    m_pDefaultPass->MarkDirty();
  }
  return commandBuffer;
}

ezPipelineBarrierVulkan& ezGALDeviceVulkan::GetCurrentPipelineBarrier()
{
  vk::CommandBuffer& commandBuffer = m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer;
  if (!commandBuffer)
  {
    GetCurrentCommandBuffer();
  }
  return *m_pPipelineBarrier.Borrow();
}

ezQueryPoolVulkan& ezGALDeviceVulkan::GetQueryPool() const
{
  return *m_pQueryPool.Borrow();
}

ezStagingBufferPoolVulkan& ezGALDeviceVulkan::GetStagingBufferPool() const
{
  return *m_pStagingBufferPool.Borrow();
}

ezInitContextVulkan& ezGALDeviceVulkan::GetInitContext() const
{
  return *m_pInitContext.Borrow();
}

ezProxyAllocator& ezGALDeviceVulkan::GetAllocator()
{
  return m_Allocator;
}

ezGALTextureHandle ezGALDeviceVulkan::CreateTextureInternal(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData, vk::Format OverrideFormat, bool bStaging)
{
  ezGALTextureVulkan* pTexture = EZ_NEW(&m_Allocator, ezGALTextureVulkan, Description, OverrideFormat, bStaging);

  if (!pTexture->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return ezGALTextureHandle();
  }

  return FinalizeTextureInternal(Description, pTexture);
}

ezGALBufferHandle ezGALDeviceVulkan::CreateBufferInternal(const ezGALBufferCreationDescription& Description, ezArrayPtr<const ezUInt8> pInitialData, bool bCPU)
{
  ezGALBufferVulkan* pBuffer = EZ_NEW(&m_Allocator, ezGALBufferVulkan, Description, bCPU);

  if (!pBuffer->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pBuffer);
    return ezGALBufferHandle();
  }

  return FinalizeBufferInternal(Description, pBuffer);
}

void ezGALDeviceVulkan::BeginPipelinePlatform(const char* szName, ezGALSwapChain* pSwapChain)
{
  EZ_PROFILE_SCOPE("BeginPipelinePlatform");

  GetCurrentCommandBuffer();
#if EZ_ENABLED(EZ_USE_PROFILING)
  m_pPipelineTimingScope = ezProfilingScopeAndMarker::Start(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), szName);
#endif

  if (pSwapChain)
  {
    pSwapChain->AcquireNextRenderTarget(this);
  }
}

void ezGALDeviceVulkan::EndPipelinePlatform(ezGALSwapChain* pSwapChain)
{
  EZ_PROFILE_SCOPE("EndPipelinePlatform");

#if EZ_ENABLED(EZ_USE_PROFILING)
  ezProfilingScopeAndMarker::Stop(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), m_pPipelineTimingScope);
#endif
  if (pSwapChain)
  {
    pSwapChain->PresentRenderTarget(this);
  }

  // Render context is reset on every end pipeline so it will re-submit all state change for the next render pass. Thus it is safe at this point to do a full reset.
  // Technically don't have to reset here, MarkDirty would also be fine but we do need to do a Reset at the end of the frame as pointers held by the ezGALCommandEncoderImplVulkan may not be valid in the next frame.
  m_pDefaultPass->Reset();
}

vk::Fence ezGALDeviceVulkan::Submit(vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitStage, vk::Semaphore signalSemaphore)
{
  m_pDefaultPass->SetCurrentCommandBuffer(nullptr, nullptr);

  vk::CommandBuffer initCommandBuffer = m_pInitContext->GetFinishedCommandBuffer();
  bool bHasCmdBuffer = initCommandBuffer || m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer;

  ezHybridArray<vk::CommandBuffer, 2> buffers;
  vk::SubmitInfo submitInfo = {};
  if (bHasCmdBuffer)
  {
    vk::CommandBuffer mainCommandBuffer = m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer;
    if (initCommandBuffer)
    {
      // Any background loading that happened up to this point needs to be submitted first.
      // The main render command buffer assumes that all new resources are in their default state which is made sure by subitting this command buffer.
      buffers.PushBack(initCommandBuffer);
    }
    if (mainCommandBuffer)
    {
      GetCurrentPipelineBarrier().Submit();
      mainCommandBuffer.end();
      buffers.PushBack(mainCommandBuffer);
    }
    submitInfo.commandBufferCount = buffers.GetCount();
    submitInfo.pCommandBuffers = buffers.GetData();
  }

  vk::Fence renderFence = ezFencePoolVulkan::RequestFence();

  ezHybridArray<vk::Semaphore, 2> waitSemaphores;
  ezHybridArray<vk::PipelineStageFlags, 2> waitStages;
  ezHybridArray<vk::Semaphore, 2> signalSemaphores;

  if (m_lastCommandBufferFinished)
  {
    waitSemaphores.PushBack(m_lastCommandBufferFinished);
    waitStages.PushBack(vk::PipelineStageFlagBits::eAllCommands);
    ReclaimLater(m_lastCommandBufferFinished);
  }

  if (waitSemaphore)
  {
    waitSemaphores.PushBack(waitSemaphore);
    waitStages.PushBack(waitStage);
  }

  if (signalSemaphore)
  {
    signalSemaphores.PushBack(signalSemaphore);
  }
  m_lastCommandBufferFinished = ezSemaphorePoolVulkan::RequestSemaphore();
  signalSemaphores.PushBack(m_lastCommandBufferFinished);

  submitInfo.waitSemaphoreCount = waitSemaphores.GetCount();
  submitInfo.pWaitSemaphores = waitSemaphores.GetData();
  submitInfo.pWaitDstStageMask = waitStages.GetData();
  submitInfo.signalSemaphoreCount = signalSemaphores.GetCount();
  submitInfo.pSignalSemaphores = signalSemaphores.GetData();

  {
    m_PerFrameData[m_uiCurrentPerFrameData].m_CommandBufferFences.PushBack(renderFence);
    m_graphicsQueue.m_queue.submit(1, &submitInfo, renderFence);
  }

  auto res = renderFence;
  ReclaimLater(renderFence);
  ReclaimLater(m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer, m_pCommandBufferPool.Borrow());

  return res;
}

ezGALPass* ezGALDeviceVulkan::BeginPassPlatform(const char* szName)
{
  GetCurrentCommandBuffer();
#if EZ_ENABLED(EZ_USE_PROFILING)
  m_pPassTimingScope = ezProfilingScopeAndMarker::Start(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), szName);
#endif
  return m_pDefaultPass.Borrow();
}

void ezGALDeviceVulkan::EndPassPlatform(ezGALPass* pPass)
{
#if EZ_ENABLED(EZ_USE_PROFILING)
  ezProfilingScopeAndMarker::Stop(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), m_pPassTimingScope);
#endif
}

// State creation functions

ezGALBlendState* ezGALDeviceVulkan::CreateBlendStatePlatform(const ezGALBlendStateCreationDescription& Description)
{
  ezGALBlendStateVulkan* pState = EZ_NEW(&m_Allocator, ezGALBlendStateVulkan, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
}

void ezGALDeviceVulkan::DestroyBlendStatePlatform(ezGALBlendState* pBlendState)
{
  ezGALBlendStateVulkan* pState = static_cast<ezGALBlendStateVulkan*>(pBlendState);
  ezResourceCacheVulkan::ResourceDeleted(pState);
  pState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pState);
}

ezGALDepthStencilState* ezGALDeviceVulkan::CreateDepthStencilStatePlatform(const ezGALDepthStencilStateCreationDescription& Description)
{
  ezGALDepthStencilStateVulkan* pVulkanDepthStencilState = EZ_NEW(&m_Allocator, ezGALDepthStencilStateVulkan, Description);

  if (pVulkanDepthStencilState->InitPlatform(this).Succeeded())
  {
    return pVulkanDepthStencilState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVulkanDepthStencilState);
    return nullptr;
  }
}

void ezGALDeviceVulkan::DestroyDepthStencilStatePlatform(ezGALDepthStencilState* pDepthStencilState)
{
  ezGALDepthStencilStateVulkan* pVulkanDepthStencilState = static_cast<ezGALDepthStencilStateVulkan*>(pDepthStencilState);
  ezResourceCacheVulkan::ResourceDeleted(pVulkanDepthStencilState);
  pVulkanDepthStencilState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanDepthStencilState);
}

ezGALRasterizerState* ezGALDeviceVulkan::CreateRasterizerStatePlatform(const ezGALRasterizerStateCreationDescription& Description)
{
  ezGALRasterizerStateVulkan* pVulkanRasterizerState = EZ_NEW(&m_Allocator, ezGALRasterizerStateVulkan, Description);

  if (pVulkanRasterizerState->InitPlatform(this).Succeeded())
  {
    return pVulkanRasterizerState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVulkanRasterizerState);
    return nullptr;
  }
}

void ezGALDeviceVulkan::DestroyRasterizerStatePlatform(ezGALRasterizerState* pRasterizerState)
{
  ezGALRasterizerStateVulkan* pVulkanRasterizerState = static_cast<ezGALRasterizerStateVulkan*>(pRasterizerState);
  ezResourceCacheVulkan::ResourceDeleted(pVulkanRasterizerState);
  pVulkanRasterizerState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanRasterizerState);
}

ezGALSamplerState* ezGALDeviceVulkan::CreateSamplerStatePlatform(const ezGALSamplerStateCreationDescription& Description)
{
  ezGALSamplerStateVulkan* pVulkanSamplerState = EZ_NEW(&m_Allocator, ezGALSamplerStateVulkan, Description);

  if (pVulkanSamplerState->InitPlatform(this).Succeeded())
  {
    return pVulkanSamplerState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVulkanSamplerState);
    return nullptr;
  }
}

void ezGALDeviceVulkan::DestroySamplerStatePlatform(ezGALSamplerState* pSamplerState)
{
  ezGALSamplerStateVulkan* pVulkanSamplerState = static_cast<ezGALSamplerStateVulkan*>(pSamplerState);
  pVulkanSamplerState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanSamplerState);
}


// Resource creation functions

ezGALShader* ezGALDeviceVulkan::CreateShaderPlatform(const ezGALShaderCreationDescription& Description)
{
  ezGALShaderVulkan* pShader = EZ_NEW(&m_Allocator, ezGALShaderVulkan, Description);

  if (!pShader->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pShader);
    return nullptr;
  }

  return pShader;
}

void ezGALDeviceVulkan::DestroyShaderPlatform(ezGALShader* pShader)
{
  ezGALShaderVulkan* pVulkanShader = static_cast<ezGALShaderVulkan*>(pShader);
  ezResourceCacheVulkan::ShaderDeleted(pVulkanShader);
  pVulkanShader->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanShader);
}

ezGALBuffer* ezGALDeviceVulkan::CreateBufferPlatform(
  const ezGALBufferCreationDescription& Description, ezArrayPtr<const ezUInt8> pInitialData)
{
  ezGALBufferVulkan* pBuffer = EZ_NEW(&m_Allocator, ezGALBufferVulkan, Description);

  if (!pBuffer->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pBuffer);
    return nullptr;
  }

  return pBuffer;
}

void ezGALDeviceVulkan::DestroyBufferPlatform(ezGALBuffer* pBuffer)
{
  ezGALBufferVulkan* pVulkanBuffer = static_cast<ezGALBufferVulkan*>(pBuffer);
  GetCurrentPipelineBarrier().BufferDestroyed(pVulkanBuffer);
  pVulkanBuffer->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanBuffer);
}

ezGALTexture* ezGALDeviceVulkan::CreateTexturePlatform(
  const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  ezGALTextureVulkan* pTexture = EZ_NEW(&m_Allocator, ezGALTextureVulkan, Description);

  if (!pTexture->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return nullptr;
  }

  return pTexture;
}


void ezGALDeviceVulkan::DestroyTexturePlatform(ezGALTexture* pTexture)
{
  ezGALTextureVulkan* pVulkanTexture = static_cast<ezGALTextureVulkan*>(pTexture);
  GetCurrentPipelineBarrier().TextureDestroyed(pVulkanTexture);
  m_pInitContext->TextureDestroyed(pVulkanTexture);

  pVulkanTexture->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanTexture);
}

ezGALResourceView* ezGALDeviceVulkan::CreateResourceViewPlatform(
  ezGALResourceBase* pResource, const ezGALResourceViewCreationDescription& Description)
{
  ezGALResourceViewVulkan* pResourceView = EZ_NEW(&m_Allocator, ezGALResourceViewVulkan, pResource, Description);

  if (!pResourceView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pResourceView);
    return nullptr;
  }

  return pResourceView;
}

void ezGALDeviceVulkan::DestroyResourceViewPlatform(ezGALResourceView* pResourceView)
{
  ezGALResourceViewVulkan* pVulkanResourceView = static_cast<ezGALResourceViewVulkan*>(pResourceView);
  pVulkanResourceView->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanResourceView);
}

ezGALRenderTargetView* ezGALDeviceVulkan::CreateRenderTargetViewPlatform(
  ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& Description)
{
  ezGALRenderTargetViewVulkan* pRTView = EZ_NEW(&m_Allocator, ezGALRenderTargetViewVulkan, pTexture, Description);

  if (!pRTView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pRTView);
    return nullptr;
  }

  return pRTView;
}

void ezGALDeviceVulkan::DestroyRenderTargetViewPlatform(ezGALRenderTargetView* pRenderTargetView)
{
  ezGALRenderTargetViewVulkan* pVulkanRenderTargetView = static_cast<ezGALRenderTargetViewVulkan*>(pRenderTargetView);
  pVulkanRenderTargetView->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVulkanRenderTargetView);
}

ezGALUnorderedAccessView* ezGALDeviceVulkan::CreateUnorderedAccessViewPlatform(
  ezGALResourceBase* pTextureOfBuffer, const ezGALUnorderedAccessViewCreationDescription& Description)
{
  ezGALUnorderedAccessViewVulkan* pUnorderedAccessView = EZ_NEW(&m_Allocator, ezGALUnorderedAccessViewVulkan, pTextureOfBuffer, Description);

  if (!pUnorderedAccessView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pUnorderedAccessView);
    return nullptr;
  }

  return pUnorderedAccessView;
}

void ezGALDeviceVulkan::DestroyUnorderedAccessViewPlatform(ezGALUnorderedAccessView* pUnorderedAccessView)
{
  ezGALUnorderedAccessViewVulkan* pUnorderedAccessViewVulkan = static_cast<ezGALUnorderedAccessViewVulkan*>(pUnorderedAccessView);
  pUnorderedAccessViewVulkan->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pUnorderedAccessViewVulkan);
}



// Other rendering creation functions
ezGALQuery* ezGALDeviceVulkan::CreateQueryPlatform(const ezGALQueryCreationDescription& Description)
{
  ezGALQueryVulkan* pQuery = EZ_NEW(&m_Allocator, ezGALQueryVulkan, Description);

  if (!pQuery->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pQuery);
    return nullptr;
  }

  return pQuery;
}

void ezGALDeviceVulkan::DestroyQueryPlatform(ezGALQuery* pQuery)
{
  ezGALQueryVulkan* pQueryVulkan = static_cast<ezGALQueryVulkan*>(pQuery);
  pQueryVulkan->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pQueryVulkan);
}

ezGALVertexDeclaration* ezGALDeviceVulkan::CreateVertexDeclarationPlatform(const ezGALVertexDeclarationCreationDescription& Description)
{
  ezGALVertexDeclarationVulkan* pVertexDeclaration = EZ_NEW(&m_Allocator, ezGALVertexDeclarationVulkan, Description);

  if (pVertexDeclaration->InitPlatform(this).Succeeded())
  {
    return pVertexDeclaration;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVertexDeclaration);
    return nullptr;
  }
}

void ezGALDeviceVulkan::DestroyVertexDeclarationPlatform(ezGALVertexDeclaration* pVertexDeclaration)
{
  ezGALVertexDeclarationVulkan* pVertexDeclarationVulkan = static_cast<ezGALVertexDeclarationVulkan*>(pVertexDeclaration);
  ezResourceCacheVulkan::ResourceDeleted(pVertexDeclarationVulkan);
  pVertexDeclarationVulkan->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pVertexDeclarationVulkan);
}

ezGALTimestampHandle ezGALDeviceVulkan::GetTimestampPlatform()
{
  return m_pQueryPool->GetTimestamp();
}

ezResult ezGALDeviceVulkan::GetTimestampResultPlatform(ezGALTimestampHandle hTimestamp, ezTime& result)
{
  return m_pQueryPool->GetTimestampResult(hTimestamp, result);
}

// Misc functions

void ezGALDeviceVulkan::BeginFramePlatform(const ezUInt64 uiRenderFrame)
{
  auto& pCommandEncoder = m_pDefaultPass->m_pCommandEncoderImpl;

  // check if fence is reached
  if (m_PerFrameData[m_uiCurrentPerFrameData].m_uiFrame != ((ezUInt64)-1))
  {
    auto& perFrameData = m_PerFrameData[m_uiCurrentPerFrameData];
    for (vk::Fence fence : perFrameData.m_CommandBufferFences)
    {
      vk::Result fenceStatus = m_device.getFenceStatus(fence);
      if (fenceStatus == vk::Result::eNotReady)
      {
        m_device.waitForFences(1, &fence, true, 1000000000);
      }
    }
    perFrameData.m_CommandBufferFences.Clear();

    {
      EZ_LOCK(m_PerFrameData[m_uiCurrentPerFrameData].m_pendingDeletionsMutex);
      DeletePendingResources(m_PerFrameData[m_uiCurrentPerFrameData].m_pendingDeletionsPrevious);
    }
    {
      EZ_LOCK(m_PerFrameData[m_uiCurrentPerFrameData].m_reclaimResourcesMutex);
      ReclaimResources(m_PerFrameData[m_uiCurrentPerFrameData].m_reclaimResourcesPrevious);
    }
    m_uiSafeFrame = m_PerFrameData[m_uiCurrentPerFrameData].m_uiFrame;
  }
  {
    auto& perFrameData = m_PerFrameData[m_uiNextPerFrameData];
    perFrameData.m_fInvTicksPerSecond = -1.0f;
  }

  m_PerFrameData[m_uiCurrentPerFrameData].m_uiFrame = m_uiFrameCounter;

  m_pQueryPool->BeginFrame(GetCurrentCommandBuffer());
  GetCurrentCommandBuffer();

#if EZ_ENABLED(EZ_USE_PROFILING)
  ezStringBuilder sb;
  sb.Format("Frame {}", uiRenderFrame);
  m_pFrameTimingScope = ezProfilingScopeAndMarker::Start(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), sb);
#endif
}

void ezGALDeviceVulkan::EndFramePlatform()
{
#if EZ_ENABLED(EZ_USE_PROFILING)
  {
    // #TODO_VULKAN This is very wasteful, in normal cases the last endPipeline will have submitted the command buffer via the swapchain. Thus, we start and submit a command buffer here with only the timestamp in it.
    GetCurrentCommandBuffer();
    ezProfilingScopeAndMarker::Stop(m_pDefaultPass->m_pRenderCommandEncoder.Borrow(), m_pFrameTimingScope);
  }
#endif

  if (m_PerFrameData[m_uiCurrentPerFrameData].m_currentCommandBuffer)
  {
    Submit({}, {}, {});
  }

  {
    // Resources can be added to deletion / reclaim outside of the render frame. These will not be covered by the fences. To handle this, we swap the resources arrays so for any newly added resources we know they are not part of the batch that is deleted / reclaimed with the frame.
    auto& currentFrameData = m_PerFrameData[m_uiCurrentPerFrameData];
    {
      EZ_LOCK(currentFrameData.m_pendingDeletionsMutex);
      currentFrameData.m_pendingDeletionsPrevious.Swap(currentFrameData.m_pendingDeletions);
    }
    {
      EZ_LOCK(currentFrameData.m_reclaimResourcesMutex);
      currentFrameData.m_reclaimResourcesPrevious.Swap(currentFrameData.m_reclaimResources);
    }
  }
  m_uiCurrentPerFrameData = (m_uiCurrentPerFrameData + 1) % EZ_ARRAY_SIZE(m_PerFrameData);
  m_uiNextPerFrameData = (m_uiCurrentPerFrameData + 1) % EZ_ARRAY_SIZE(m_PerFrameData);
  ++m_uiFrameCounter;
}

void ezGALDeviceVulkan::FillCapabilitiesPlatform()
{
  vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();
  vk::PhysicalDeviceFeatures features = m_physicalDevice.getFeatures();

  ezUInt64 dedicatedMemory = 0;
  ezUInt64 systemMemory = 0;
  for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i)
  {
    if (memProperties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
    {
      dedicatedMemory += memProperties.memoryHeaps[i].size;
    }
    else
    {
      systemMemory += memProperties.memoryHeaps[i].size;
    }
  }

  {
    m_Capabilities.m_sAdapterName = ezStringUtf8(m_properties.deviceName).GetData();
    m_Capabilities.m_uiDedicatedVRAM = static_cast<ezUInt64>(dedicatedMemory);
    m_Capabilities.m_uiDedicatedSystemRAM = static_cast<ezUInt64>(systemMemory);
    m_Capabilities.m_uiSharedSystemRAM = static_cast<ezUInt64>(0); // TODO
    m_Capabilities.m_bHardwareAccelerated = m_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
  }

  m_Capabilities.m_bMultithreadedResourceCreation = true;

  m_Capabilities.m_bB5G6R5Textures = true;          // TODO how to check
  m_Capabilities.m_bNoOverwriteBufferUpdate = true; // TODO how to check

  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::VertexShader] = true;
  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::HullShader] = features.tessellationShader;
  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::DomainShader] = features.tessellationShader;
  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::GeometryShader] = features.geometryShader;
  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::PixelShader] = true;
  m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::ComputeShader] = true; // we check this when creating the queue, always has to be supported
  m_Capabilities.m_bInstancing = true;
  m_Capabilities.m_b32BitIndices = true;
  m_Capabilities.m_bIndirectDraw = true;
  m_Capabilities.m_bStreamOut = true;
  m_Capabilities.m_uiMaxConstantBuffers = m_properties.limits.maxDescriptorSetUniformBuffers;
  m_Capabilities.m_bTextureArrays = true;
  m_Capabilities.m_bCubemapArrays = true;
  m_Capabilities.m_uiMaxTextureDimension = m_properties.limits.maxImageDimension1D;
  m_Capabilities.m_uiMaxCubemapDimension = m_properties.limits.maxImageDimensionCube;
  m_Capabilities.m_uiMax3DTextureDimension = m_properties.limits.maxImageDimension3D;
  m_Capabilities.m_uiMaxAnisotropy = static_cast<ezUInt16>(m_properties.limits.maxSamplerAnisotropy);
  m_Capabilities.m_uiMaxRendertargets = m_properties.limits.maxColorAttachments;
  m_Capabilities.m_uiUAVCount = ezMath::Min(m_properties.limits.maxDescriptorSetStorageBuffers, m_properties.limits.maxDescriptorSetStorageImages);
  m_Capabilities.m_bAlphaToCoverage = true;
  m_Capabilities.m_bVertexShaderRenderTargetArrayIndex = m_extensions.m_bShaderViewportIndexLayer;

  m_Capabilities.m_bConservativeRasterization = false; // need to query for VK_EXT_CONSERVATIVE_RASTERIZATION
}

void ezGALDeviceVulkan::WaitIdlePlatform()
{
  m_device.waitIdle();
  DestroyDeadObjects();
  for (ezUInt32 i = 0; i < EZ_ARRAY_SIZE(m_PerFrameData); ++i)
  {
    // First, we wait for all fences for all submit calls. This is necessary to make sure no resources of the frame are still in use by the GPU.
    auto& perFrameData = m_PerFrameData[i];
    for (vk::Fence fence : perFrameData.m_CommandBufferFences)
    {
      vk::Result fenceStatus = m_device.getFenceStatus(fence);
      if (fenceStatus == vk::Result::eNotReady)
      {
        m_device.waitForFences(1, &fence, true, 1000000000);
      }
    }
    perFrameData.m_CommandBufferFences.Clear();
  }

  for (ezUInt32 i = 0; i < EZ_ARRAY_SIZE(m_PerFrameData); ++i)
  {
    {
      EZ_LOCK(m_PerFrameData[i].m_pendingDeletionsMutex);
      DeletePendingResources(m_PerFrameData[i].m_pendingDeletionsPrevious);
      DeletePendingResources(m_PerFrameData[i].m_pendingDeletions);
    }
    {
      EZ_LOCK(m_PerFrameData[i].m_reclaimResourcesMutex);
      ReclaimResources(m_PerFrameData[i].m_reclaimResourcesPrevious);
      ReclaimResources(m_PerFrameData[i].m_reclaimResources);
    }
  }
}

vk::PipelineStageFlags ezGALDeviceVulkan::GetSupportedStages() const
{
  return m_supportedStages;
}

ezInt32 ezGALDeviceVulkan::GetMemoryIndex(vk::MemoryPropertyFlags properties, const vk::MemoryRequirements& requirements) const
{

  for (ezUInt32 i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
  {
    const vk::MemoryType& type = m_memoryProperties.memoryTypes[i];
    if (requirements.memoryTypeBits & (1 << i) && (type.propertyFlags & properties))
    {
      return i;
    }
  }

  return -1;
}

void ezGALDeviceVulkan::DeleteLater(const PendingDeletion& deletion)
{
  EZ_LOCK(m_PerFrameData[m_uiCurrentPerFrameData].m_pendingDeletionsMutex);
  m_PerFrameData[m_uiCurrentPerFrameData].m_pendingDeletions.PushBack(deletion);
}

void ezGALDeviceVulkan::ReclaimLater(const ReclaimResource& reclaim)
{
  EZ_LOCK(m_PerFrameData[m_uiCurrentPerFrameData].m_reclaimResourcesMutex);
  m_PerFrameData[m_uiCurrentPerFrameData].m_reclaimResources.PushBack(reclaim);
}

void ezGALDeviceVulkan::DeletePendingResources(ezDeque<PendingDeletion>& pendingDeletions)
{
  for (PendingDeletion& deletion : pendingDeletions)
  {
    switch (deletion.m_type)
    {
      case vk::ObjectType::eImageView:
        m_device.destroyImageView(reinterpret_cast<vk::ImageView&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eImage:
      {
        auto& image = reinterpret_cast<vk::Image&>(deletion.m_pObject);
        OnBeforeImageDestroyed.Broadcast(OnBeforeImageDestroyedData{image, *this});
        ezMemoryAllocatorVulkan::DestroyImage(image, deletion.m_allocation);
      }
      break;
      case vk::ObjectType::eBuffer:
        ezMemoryAllocatorVulkan::DestroyBuffer(reinterpret_cast<vk::Buffer&>(deletion.m_pObject), deletion.m_allocation);
        break;
      case vk::ObjectType::eBufferView:
        m_device.destroyBufferView(reinterpret_cast<vk::BufferView&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eFramebuffer:
        m_device.destroyFramebuffer(reinterpret_cast<vk::Framebuffer&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eRenderPass:
        m_device.destroyRenderPass(reinterpret_cast<vk::RenderPass&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eSampler:
        m_device.destroySampler(reinterpret_cast<vk::Sampler&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eSwapchainKHR:
        m_device.destroySwapchainKHR(reinterpret_cast<vk::SwapchainKHR&>(deletion.m_pObject));
        break;
      case vk::ObjectType::eSurfaceKHR:
        m_instance.destroySurfaceKHR(reinterpret_cast<vk::SurfaceKHR&>(deletion.m_pObject));
        if (ezWindowBase* pWindow = reinterpret_cast<ezWindowBase*>(deletion.m_pContext))
        {
          pWindow->RemoveReference();
        }
        break;
      case vk::ObjectType::eShaderModule:
        m_device.destroyShaderModule(reinterpret_cast<vk::ShaderModule&>(deletion.m_pObject));
        break;
      case vk::ObjectType::ePipeline:
        m_device.destroyPipeline(reinterpret_cast<vk::Pipeline&>(deletion.m_pObject));
        break;
      default:
        EZ_REPORT_FAILURE("This object type is not implemented");
        break;
    }
  }
  pendingDeletions.Clear();
}

void ezGALDeviceVulkan::ReclaimResources(ezDeque<ReclaimResource>& resources)
{
  for (ReclaimResource& resource : resources)
  {
    switch (resource.m_type)
    {
      case vk::ObjectType::eSemaphore:
        ezSemaphorePoolVulkan::ReclaimSemaphore(reinterpret_cast<vk::Semaphore&>(resource.m_pObject));
        break;
      case vk::ObjectType::eFence:
        ezFencePoolVulkan::ReclaimFence(reinterpret_cast<vk::Fence&>(resource.m_pObject));
        break;
      case vk::ObjectType::eCommandBuffer:
        static_cast<ezCommandBufferPoolVulkan*>(resource.m_pContext)->ReclaimCommandBuffer(reinterpret_cast<vk::CommandBuffer&>(resource.m_pObject));
        break;
      case vk::ObjectType::eDescriptorPool:
        ezDescriptorSetPoolVulkan::ReclaimPool(reinterpret_cast<vk::DescriptorPool&>(resource.m_pObject));
        break;
      default:
        EZ_REPORT_FAILURE("This object type is not implemented");
        break;
    }
  }
  resources.Clear();
}

void ezGALDeviceVulkan::FillFormatLookupTable()
{
  /// The list below is in the same order as the ezGALResourceFormat enum. No format should be missing except the ones that are just different names for the same enum value.
  vk::Format R32G32B32A32_Formats[] = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Uint, vk::Format::eR32G32B32A32Sint};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAFloat, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32A32Sfloat, R32G32B32A32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32A32Uint, R32G32B32A32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32A32Sint, R32G32B32A32_Formats));

  vk::Format R32G32B32_Formats[] = {vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32Uint, vk::Format::eR32G32B32Sint};
  // TODO 3-channel formats are not really supported under vulkan judging by experience
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBFloat, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32Sfloat, R32G32B32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBUInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32Uint, R32G32B32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32B32Sint, R32G32B32_Formats));

  // TODO dunno if these are actually supported for the respective Vulkan device
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::B5G6R5UNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR5G6B5UnormPack16));

  vk::Format B8G8R8A8_Formats[] = {vk::Format::eB8G8R8A8Unorm, vk::Format::eB8G8R8A8Srgb};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BGRAUByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eB8G8R8A8Unorm, B8G8R8A8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BGRAUByteNormalizedsRGB, ezGALFormatLookupEntryVulkan(vk::Format::eB8G8R8A8Srgb, B8G8R8A8_Formats));

  vk::Format R16G16B16A16_Formats[] = {vk::Format::eR16G16B16A16Sfloat, vk::Format::eR16G16B16A16Uint, vk::Format::eR16G16B16A16Unorm, vk::Format::eR16G16B16A16Sint, vk::Format::eR16G16B16A16Snorm};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAHalf, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16B16A16Sfloat, R16G16B16A16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16B16A16Uint, R16G16B16A16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16B16A16Unorm, R16G16B16A16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16B16A16Sint, R16G16B16A16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16B16A16Snorm, R16G16B16A16_Formats));

  vk::Format R32G32_Formats[] = {vk::Format::eR32G32Sfloat, vk::Format::eR32G32Uint, vk::Format::eR32G32Sint};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGFloat, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32Sfloat, R32G32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGUInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32Uint, R32G32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32G32Sint, R32G32_Formats));

  vk::Format R10G10B10A2_Formats[] = {vk::Format::eA2B10G10R10UintPack32, vk::Format::eA2B10G10R10UnormPack32};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGB10A2UInt, ezGALFormatLookupEntryVulkan(vk::Format::eA2B10G10R10UintPack32, R10G10B10A2_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGB10A2UIntNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eA2B10G10R10UnormPack32, R10G10B10A2_Formats));

  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RG11B10Float, ezGALFormatLookupEntryVulkan(vk::Format::eB10G11R11UfloatPack32));

  vk::Format R8G8B8A8_Formats[] = {vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Uint, vk::Format::eR8G8B8A8Snorm, vk::Format::eR8G8B8A8Sint};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8B8A8Unorm, R8G8B8A8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUByteNormalizedsRGB, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8B8A8Srgb, R8G8B8A8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAUByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8B8A8Uint, R8G8B8A8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8B8A8Snorm, R8G8B8A8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGBAByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8B8A8Sint, R8G8B8A8_Formats));

  vk::Format R16G16_Formats[] = {vk::Format::eR16G16Sfloat, vk::Format::eR16G16Uint, vk::Format::eR16G16Unorm, vk::Format::eR16G16Sint, vk::Format::eR16G16Snorm};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGHalf, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16Sfloat, R16G16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGUShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16Uint, R16G16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGUShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16Unorm, R16G16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16Sint, R16G16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16G16Snorm, R16G16_Formats));

  vk::Format R8G8_Formats[] = {vk::Format::eR8G8Uint, vk::Format::eR8G8Unorm, vk::Format::eR8G8Sint, vk::Format::eR8G8Snorm};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGUByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8Uint, R8G8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGUByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8Unorm, R8G8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8Sint, R8G8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RGByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8G8Snorm, R8G8_Formats));

  vk::Format R32_Formats[] = {vk::Format::eR32Sfloat, vk::Format::eR32Uint, vk::Format::eR32Sint};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RFloat, ezGALFormatLookupEntryVulkan(vk::Format::eR32Sfloat, R32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RUInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32Uint, R32_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RInt, ezGALFormatLookupEntryVulkan(vk::Format::eR32Sint, R32_Formats));

  vk::Format R16_Formats[] = {vk::Format::eR16Sfloat, vk::Format::eR16Uint, vk::Format::eR16Unorm, vk::Format::eR16Sint, vk::Format::eR16Snorm};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RHalf, ezGALFormatLookupEntryVulkan(vk::Format::eR16Sfloat, R16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RUShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16Uint, R16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RUShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16Unorm, R16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RShort, ezGALFormatLookupEntryVulkan(vk::Format::eR16Sint, R16_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RShortNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR16Snorm, R16_Formats));

  vk::Format R8_Formats[] = {vk::Format::eR8Uint, vk::Format::eR8Unorm, vk::Format::eR8Sint, vk::Format::eR8Snorm};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RUByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8Uint, R8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RUByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8Unorm, R8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RByte, ezGALFormatLookupEntryVulkan(vk::Format::eR8Sint, R8_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::RByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8Snorm, R8_Formats));

  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::AUByteNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eR8Unorm));

  auto SelectDepthFormat = [&](const std::vector<vk::Format>& list) -> vk::Format {
    for (auto& format : list)
    {
      vk::FormatProperties formatProperties;
      m_physicalDevice.getFormatProperties(format, &formatProperties);
      if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        return format;
    }
    return vk::Format::eUndefined;
  };

  auto SelectStorageFormat = [](vk::Format depthFormat) -> vk::Format {
    switch (depthFormat)
    {
      case vk::Format::eD16Unorm:
        return vk::Format::eR16Unorm;
      case vk::Format::eD16UnormS8Uint:
        return vk::Format::eUndefined;
      case vk::Format::eD24UnormS8Uint:
        return vk::Format::eUndefined;
      case vk::Format::eD32Sfloat:
        return vk::Format::eR32Sfloat;
      case vk::Format::eD32SfloatS8Uint:
        return vk::Format::eR32Sfloat;
      default:
        return vk::Format::eUndefined;
    }
  };

  // Select smallest available depth format.  #TODO_VULKAN support packed eX8D24UnormPack32?
  vk::Format depthFormat = SelectDepthFormat({vk::Format::eD16Unorm, vk::Format::eD24UnormS8Uint, vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint});
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::D16, ezGALFormatLookupEntryVulkan(depthFormat).R(SelectStorageFormat(depthFormat)));

  // Select closest depth stencil format.
  depthFormat = SelectDepthFormat({vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint, vk::Format::eD16UnormS8Uint});
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::D24S8, ezGALFormatLookupEntryVulkan(depthFormat).R(SelectStorageFormat(depthFormat)));

  // Select biggest depth format.
  depthFormat = SelectDepthFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD16Unorm});
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::DFloat, ezGALFormatLookupEntryVulkan(depthFormat).R(SelectStorageFormat(depthFormat)));

  vk::Format BC1_Formats[] = {vk::Format::eBc1RgbaUnormBlock, vk::Format::eBc1RgbaSrgbBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC1, ezGALFormatLookupEntryVulkan(vk::Format::eBc1RgbaUnormBlock, BC1_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC1sRGB, ezGALFormatLookupEntryVulkan(vk::Format::eBc1RgbaSrgbBlock, BC1_Formats));

  vk::Format BC2_Formats[] = {vk::Format::eBc2UnormBlock, vk::Format::eBc2SrgbBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC2, ezGALFormatLookupEntryVulkan(vk::Format::eBc2UnormBlock, BC2_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC2sRGB, ezGALFormatLookupEntryVulkan(vk::Format::eBc2SrgbBlock, BC2_Formats));

  vk::Format BC3_Formats[] = {vk::Format::eBc3UnormBlock, vk::Format::eBc3SrgbBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC3, ezGALFormatLookupEntryVulkan(vk::Format::eBc3UnormBlock, BC3_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC3sRGB, ezGALFormatLookupEntryVulkan(vk::Format::eBc3SrgbBlock, BC3_Formats));

  vk::Format BC4_Formats[] = {vk::Format::eBc4UnormBlock, vk::Format::eBc4SnormBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC4UNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eBc4UnormBlock, BC4_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC4Normalized, ezGALFormatLookupEntryVulkan(vk::Format::eBc4SnormBlock, BC4_Formats));

  vk::Format BC5_Formats[] = {vk::Format::eBc5UnormBlock, vk::Format::eBc5SnormBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC5UNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eBc5UnormBlock, BC5_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC5Normalized, ezGALFormatLookupEntryVulkan(vk::Format::eBc5SnormBlock, BC5_Formats));

  vk::Format BC6_Formats[] = {vk::Format::eBc6HUfloatBlock, vk::Format::eBc6HSfloatBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC6UFloat, ezGALFormatLookupEntryVulkan(vk::Format::eBc6HUfloatBlock, BC6_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC6Float, ezGALFormatLookupEntryVulkan(vk::Format::eBc6HSfloatBlock, BC6_Formats));

  vk::Format BC7_Formats[] = {vk::Format::eBc7UnormBlock, vk::Format::eBc7SrgbBlock};
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC7UNormalized, ezGALFormatLookupEntryVulkan(vk::Format::eBc7UnormBlock, BC7_Formats));
  m_FormatLookupTable.SetFormatInfo(ezGALResourceFormat::BC7UNormalizedsRGB, ezGALFormatLookupEntryVulkan(vk::Format::eBc7SrgbBlock, BC7_Formats));

  if (false)
  {
    EZ_LOG_BLOCK("GAL Resource Formats");
    for (ezUInt32 i = 1; i < ezGALResourceFormat::ENUM_COUNT; i++)
    {
      const ezGALFormatLookupEntryVulkan& entry = m_FormatLookupTable.GetFormatInfo((ezGALResourceFormat::Enum)i);

      vk::FormatProperties formatProperties;
      m_physicalDevice.getFormatProperties(entry.m_format, &formatProperties);

      const bool bSampled = static_cast<bool>(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);
      const bool bColorAttachment = static_cast<bool>(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment);
      const bool bDepthAttachment = static_cast<bool>(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment);

      const bool bTexel = static_cast<bool>(formatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eUniformTexelBuffer);
      const bool bStorageTexel = static_cast<bool>(formatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eStorageTexelBuffer);
      const bool bVertex = static_cast<bool>(formatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer);

      ezStringBuilder sTemp;
      ezReflectionUtils::EnumerationToString(ezGetStaticRTTI<ezGALResourceFormat>(), i, sTemp, ezReflectionUtils::EnumConversionMode::ValueNameOnly);

      ezLog::Info("OptTiling S: {}, CA: {}, DA: {}. Buffer: T: {}, ST: {}, V: {}, Format {} -> {}", bSampled ? 1 : 0, bColorAttachment ? 1 : 0, bDepthAttachment ? 1 : 0, bTexel ? 1 : 0, bStorageTexel ? 1 : 0, bVertex ? 1 : 0, sTemp, vk::to_string(entry.m_format).c_str());
    }
  }
}

EZ_STATICLINK_FILE(RendererVulkan, RendererVulkan_Device_Implementation_DeviceVulkan);

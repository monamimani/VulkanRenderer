#include "VkRenderer.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vulkan/vulkan.hpp>

#include "VkHal/VkMesh.h"
#include "VkHal/Vulkan/VulkanUtils.h"

using namespace std::literals::string_literals;

namespace VkHal
{
constexpr std::array<const char*, 2> g_instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
constexpr std::array<const char*, 1> g_validationLayers = {"VK_LAYER_LUNARG_standard_validation"};

std::vector<Vertex> vertices;
std::vector<uint32_t> indices;

VkRenderer::VkRenderer(bool isHeadless, bool enableValidation, const std::string& appName)
    : m_isHeadless(isHeadless)
    , m_enableValidation(enableValidation)
{
  char exePathStr[MAX_PATH]{};
  auto result = GetModuleFileNameA(nullptr, &exePathStr[0], MAX_PATH);
  auto exePath = std::filesystem::path(exePathStr).parent_path();

  m_dataPath = std::filesystem::canonical(exePath / ".." / ".." / ".." / "data");

  vk::ApplicationInfo appInfo{};
  appInfo.pApplicationName = appName.c_str();
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, VK_HEADER_VERSION);

  vk::InstanceCreateInfo instanceCreateInfo{};
  instanceCreateInfo.pApplicationInfo = &appInfo;

  std::vector<const char*> instanceExtensions;
  std::vector<const char*> validationLayers;

  if (!m_isHeadless)
  {
    instanceExtensions.insert(end(instanceExtensions), cbegin(g_instanceExtensions), cend(g_instanceExtensions));
  }

  if (m_enableValidation)
  {
    instanceExtensions.push_back(VkDebugReport::m_debugExtensionName);
    instanceExtensions.push_back(DebugUtils::m_debugExtensionName);
    validationLayers.insert(end(validationLayers), cbegin(g_validationLayers), cend(g_validationLayers));
    verifyValidationLayersAvailability({begin(validationLayers), end(validationLayers)});
  }

  verifyInstanceExtensionAvailability(std::set<std::string>{cbegin(instanceExtensions), cend(instanceExtensions)});
  instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
  instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  instanceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
  instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();

  m_instance = vk::createInstanceUnique(instanceCreateInfo);

  printAvailablePhysicalDeviceProperties(*m_instance);

  if (m_enableValidation)
  {
    //m_vkDebugReport = std::make_unique<VkDebugReport>(m_instance.get());
    m_debugUtils = std::make_unique<DebugUtils>(m_instance.get());
  }
}

VkRenderer::~VkRenderer()
{
  m_device->waitIdle();

  m_debugGui.reset();
}

void VkRenderer::initialize(HINSTANCE appInstance, HWND windowHandle)
{
  m_appInstance = appInstance;
  m_windowHandle = windowHandle;
  if (!m_isHeadless)
  {
    createSurface(appInstance, windowHandle);
  }

  auto physicalDevice = selectPhysicalDevice();
  m_physicalDevice = physicalDevice[0];
  createDeviceAndQueues(m_physicalDevice);

  m_debugGui = std::make_unique<DevGuiRenderer>(&m_instance.get(), m_vulkanDevice.get(), m_queueFamilyIndices.graphics, m_graphicsQueue);
}

VKHAL_API void VkRenderer::recreateSwapchain()
{
  m_device->waitIdle();

  RECT clientRect = {};
  ::GetClientRect(m_windowHandle, &clientRect);
  m_windowWidth = clientRect.right - clientRect.left;
  m_windowHeight = clientRect.bottom - clientRect.top;

  if (!m_isHeadless)
  {
    m_vulkanSwapchain = m_vulkanDevice->recreateSwapchain({m_windowWidth, m_windowHeight}, VkRenderer::m_frameResourcesCount, m_surface.get(), &m_vulkanSwapchain->getSwapchain());
    m_frameResourcesCount = std::min(m_frameResourcesCount, m_vulkanSwapchain->getSwapchainImageCount());
  }

  createFrameResources();

  createGBuffer();
  createRenderPass();
  createFramebuffers();

  createGraphicsPipeline();

  createCommandBuffers();
}

void VkRenderer::prepare(uint32_t windowWidth, uint32_t windowHeight)
{
  m_windowWidth = windowWidth;
  m_windowHeight = windowHeight;

  MeshLoader meshLoader;
  meshLoader.loadModel(m_dataPath / "models" / "chalet.obj");

  vertices = meshLoader.getVertices();
  indices = meshLoader.getIndices();

  if (!m_isHeadless)
  {
    m_vulkanSwapchain = m_vulkanDevice->createSwapchain({m_windowWidth, m_windowHeight}, VkRenderer::m_frameResourcesCount, m_surface.get());
    m_frameResourcesCount = std::min(m_frameResourcesCount, m_vulkanSwapchain->getSwapchainImageCount());
  }

  createCommandPools();
  createCommandBuffers();
  createFrameResources();

  createGBuffer();
  createRenderPass();
  createFramebuffers();

  createUniformBuffer();
  createTextureImage();
  createTextureSampler(m_vulkanTextureImage->getMipCount());
  createDescriptorSetLayout();
  createDescriptorPool();
  createDescriptorSets();

  createGraphicsPipeline();

  createVertexBuffer();
  createIndexBuffer();

  m_debugGui->prepare(m_windowHandle, m_vulkanSwapchain.get());
}

void VkRenderer::createSurface(HINSTANCE appInstance, HWND windowHandle)
{
  vk::Win32SurfaceCreateInfoKHR createInfo;
  createInfo.hinstance = appInstance;
  createInfo.hwnd = windowHandle;

  m_surface = m_instance->createWin32SurfaceKHRUnique(createInfo);
}

uint32_t getPhysicalDeviceScore(const vk::PhysicalDevice& physDevice)
{
  uint32_t score{};
  auto physDeviceProperties = physDevice.getProperties();
  auto physDeviceFeatures = physDevice.getFeatures();

  if (physDeviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
  {
    score += 1000;
  }

  score += physDeviceProperties.limits.maxImageDimension2D;

  return score;
}

int32_t getQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties, vk::QueueFlagBits queueFlags)
{
  // Find a dedicated queue for the queue flags.
  for (uint32_t i = 0; i < queueFamilyProperties.size(); i++)
  {
    const auto& queueFamilyProperty = queueFamilyProperties[i];
    if (queueFamilyProperty.queueCount > 0 && (queueFamilyProperty.queueFlags == queueFlags))
    {
      return i;
      break;
    }
  }

  // If we can't find a dedicated queue family find one that at least support it.
  for (int32_t i = 0; i < queueFamilyProperties.size(); i++)
  {
    const auto& queueFamilyProperty = queueFamilyProperties[i];
    if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & queueFlags)
    {
      return i;
      break;
    }
  }

  // Could not find a queue of that family.
  return -1;
}

std::vector<vk::PhysicalDevice> VkRenderer::selectPhysicalDevice()
{
  auto physicalDevices = m_instance->enumeratePhysicalDevices();

  if (physicalDevices.size() == 0)
  {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  vk::PhysicalDevice selectedPhysicalDevice;

  using Score_t = uint32_t;
  std::multimap<Score_t, vk::PhysicalDevice> physDeviceCandidates;

  for (auto physDevice : physicalDevices)
  {
    auto score = getPhysicalDeviceScore(physDevice);
    physDeviceCandidates.insert(std::make_pair(score, physDevice));
  }

  auto itCandidate = physDeviceCandidates.rbegin();

  while (itCandidate != physDeviceCandidates.rend())
  {
    auto physicalDevice = itCandidate->second;
    auto score = itCandidate->first;
    itCandidate++;
    if (score <= 0)
    {
      continue;
    }

    auto deviceFeatures = physicalDevice.getFeatures();
    if (!deviceFeatures.samplerAnisotropy)
    {
      continue;
    }

    auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    int32_t graphicQueueIdx = getQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
    int32_t computeQueueIdx = getQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute);
    int32_t transferQueueIdx = getQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eTransfer);

    int32_t presentQueueIdx = -1;
    if (!m_isHeadless)
    {
      for (int i = 0; i < queueFamilyProperties.size(); i++)
      {
        const auto& queueFamilyProperty = queueFamilyProperties[i];
        const auto supportSurfaceKHR = physicalDevice.getSurfaceSupportKHR(i, *m_surface);
        const auto supportWin32PresentKHR = physicalDevice.getWin32PresentationSupportKHR(i);
        if (queueFamilyProperty.queueCount > 0 && supportSurfaceKHR && supportWin32PresentKHR)
        {
          presentQueueIdx = i;
          break;
        }
      }
    }

    if (graphicQueueIdx >= 0 && computeQueueIdx >= 0 && transferQueueIdx >= 0 && !m_isHeadless ? presentQueueIdx >= 0 : true)
    {
      m_queueFamilyIndices.graphics = graphicQueueIdx;
      m_queueFamilyIndices.compute = computeQueueIdx;
      m_queueFamilyIndices.transfer = transferQueueIdx;
      m_queueFamilyIndices.present = presentQueueIdx;

      selectedPhysicalDevice = physicalDevice;
      break;
    }
  }

  if (!selectedPhysicalDevice)
  {
    throw std::runtime_error("failed to find a suitable physical GPU");
  }

  return std::vector<vk::PhysicalDevice>{selectedPhysicalDevice};
}

void VkRenderer::createDeviceAndQueues(const vk::PhysicalDevice& physicalDevice)
{
  vk::PhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.textureCompressionBC = true;
  deviceFeatures.imageCubeArray = true;
  deviceFeatures.depthClamp = true;
  deviceFeatures.depthBiasClamp = true;
  deviceFeatures.depthBounds = true;
  deviceFeatures.fillModeNonSolid = true;
  deviceFeatures.samplerAnisotropy = true;

  m_vulkanDevice = std::make_unique<VulkanDevice>(physicalDevice, deviceFeatures, m_isHeadless, m_queueFamilyIndices);

  if (m_enableValidation)
  {
    m_vulkanDevice->initDebugExtention();
  }

  m_device = &m_vulkanDevice->getDevice();
  auto queues = m_vulkanDevice->getQueues();
  m_graphicsQueue = queues[(size_t)QueueFamilyType::Graphics];
  m_computeQueue = queues[(size_t)QueueFamilyType::Compute];
  m_transferQueue = queues[(size_t)QueueFamilyType::Transfer];
  m_presentQueue = queues[(size_t)QueueFamilyType::Present];
}

void VkRenderer::createFrameResources()
{
  m_frameResources.resize(VkRenderer::m_frameResourcesCount);

  for (uint32_t i = 0; i < m_frameResources.size(); i++)
  {
    auto iStr = std::to_string(i);
    auto& frameResource = m_frameResources[i];
    frameResource.m_imageAcquiredSemaphores = m_vulkanDevice->createSemaphore();
    frameResource.m_renderCompletedSemaphores = m_vulkanDevice->createSemaphore();
    frameResource.m_frameFence = m_vulkanDevice->createFence(true);

    frameResource.m_graphicsCmdPool = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.graphics, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
    m_vulkanDevice->setObjectName(frameResource.m_graphicsCmdPool.get(), vk::ObjectType::eCommandPool, ("Frameresources:GfxCmdPool_"s + iStr).c_str());
    frameResource.m_graphicsCmdBuffers = m_vulkanDevice->allocateCommandBuffer(*frameResource.m_graphicsCmdPool, 1, true);
    m_vulkanDevice->setObjectName(frameResource.m_graphicsCmdBuffers[0].get(), vk::ObjectType::eCommandBuffer, ("Frameresources:GfxCmdBuffer_"s + iStr).c_str());

    frameResource.m_computeCmdPool = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.compute, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
    m_vulkanDevice->setObjectName(frameResource.m_computeCmdPool.get(), vk::ObjectType::eCommandPool, ("FrameresourcesComputeCmdPool_"s + iStr).c_str());
    frameResource.m_computeCmdBuffers = m_vulkanDevice->allocateCommandBuffer(*frameResource.m_computeCmdPool, 1, true);
    m_vulkanDevice->setObjectName(frameResource.m_computeCmdBuffers[0].get(), vk::ObjectType::eCommandBuffer, ("Frameresources:ComputeCmdBuffer_"s + iStr).c_str());

    frameResource.m_transferCmdPool = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.transfer, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
    m_vulkanDevice->setObjectName(frameResource.m_transferCmdPool.get(), vk::ObjectType::eCommandPool, ("FrameresourcesTransferCmdPool_"s + iStr).c_str());
    frameResource.m_transferCmdBuffers = m_vulkanDevice->allocateCommandBuffer(*frameResource.m_transferCmdPool, 1, true);
    m_vulkanDevice->setObjectName(frameResource.m_transferCmdBuffers[0].get(), vk::ObjectType::eCommandBuffer, ("Frameresources:TransferCmdBuffer_"s + iStr).c_str());

    frameResource.m_presentCmdPool = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.present, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
    m_vulkanDevice->setObjectName(frameResource.m_presentCmdPool.get(), vk::ObjectType::eCommandPool, ("Frameresources:PresentCmdPool_"s + iStr).c_str());
    frameResource.m_presentCmdBuffers = m_vulkanDevice->allocateCommandBuffer(*frameResource.m_presentCmdPool, 1, true);
    m_vulkanDevice->setObjectName(frameResource.m_presentCmdBuffers[0].get(), vk::ObjectType::eCommandBuffer, ("Frameresources:PresentCmdBuffer_"s + iStr).c_str());
  }
}

void VkRenderer::createCommandPools()
{
  m_graphicsCmdPoolTmp = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.graphics, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
  m_vulkanDevice->setObjectName(m_graphicsCmdPoolTmp.get(), vk::ObjectType::eCommandPool, "GfxCmdPoolTmp");

  m_transferCmdPool = m_vulkanDevice->createCommandPool(m_queueFamilyIndices.transfer, vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
}

void VkRenderer::createCommandBuffers()
{
  m_graphicsCmdBuffersTmp = m_vulkanDevice->allocateCommandBuffer(*m_graphicsCmdPoolTmp, 1, true);
  m_vulkanDevice->setObjectName(m_graphicsCmdBuffersTmp[0].get(), vk::ObjectType::eCommandBuffer, "GfxCmdBufferTmp");

  m_transferCmdBuffers = m_vulkanDevice->allocateCommandBuffer(*m_transferCmdPool, 1, true);
}

void VkRenderer::generateMipmaps(vk::CommandBuffer& cmdBuffer, vk::Queue queue, vk::Image image, vk::Format format, int32_t width, int32_t height, uint32_t mipLevels)
{
  auto formatProperties = m_physicalDevice.getFormatProperties(format);

  if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
  {
    throw std::runtime_error("Texture image format does not support linear blitting.");
  }

  m_debugUtils->beginLabel(cmdBuffer, "GenerateMipMap");

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  cmdBuffer.begin(beginInfo);

  vk::ImageMemoryBarrier barrier{};
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  auto mipWidth = width;
  auto mipHeight = height;

  for (uint32_t i = 1; i < mipLevels; i++)
  {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, nullptr, nullptr, barrier);

    vk::ImageBlit imgBlit{};
    imgBlit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    imgBlit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
    imgBlit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    imgBlit.srcSubresource.mipLevel = i - 1;
    imgBlit.srcSubresource.baseArrayLayer = 0;
    imgBlit.srcSubresource.layerCount = 1;
    imgBlit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    imgBlit.dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    imgBlit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    imgBlit.dstSubresource.mipLevel = i;
    imgBlit.dstSubresource.baseArrayLayer = 0;
    imgBlit.dstSubresource.layerCount = 1;

    cmdBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, imgBlit, vk::Filter::eLinear);

    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags{}, nullptr, nullptr, barrier);

    if (mipWidth > 1)
    {
      mipWidth /= 2;
    }

    if (mipHeight > 1)
    {
      mipHeight /= 2;
    }
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags{}, nullptr, nullptr, barrier);

  cmdBuffer.end();

  m_debugUtils->endLabel(cmdBuffer);

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuffer;

  m_debugUtils->beginLabel(queue, "Submit GenerateMipmap.");
  queue.submit(submitInfo, nullptr);
  m_debugUtils->endLabel(queue);
  queue.waitIdle();

  cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

vk::Format VkRenderer::selectSupportedFormat(const std::vector<vk::Format>& formats, vk ::ImageTiling desiredTilling, vk::FormatFeatureFlags featuresDesired)
{
  for (const auto& format : formats)
  {
    auto formatProperties = m_physicalDevice.getFormatProperties(format);

    if (desiredTilling == vk::ImageTiling::eLinear && (formatProperties.linearTilingFeatures & featuresDesired) == featuresDesired)
    {
      return format;
    }
    else if (desiredTilling == vk::ImageTiling::eOptimal && (formatProperties.optimalTilingFeatures & featuresDesired) == featuresDesired)
    {
      return format;
    }
  }

  throw std::runtime_error("Failed to find a supported format.");

  return vk::Format::eUndefined;
}

void VkRenderer::createGraphicsPipeline()
{
  // Setup programmable pipeline stages
  auto shaderPath = std::filesystem::canonical(m_dataPath / "shaders");

  auto vertShaderCode = readFile(shaderPath / "shader.vert.spv");
  auto fragShaderCode = readFile(shaderPath / "shader.frag.spv");

  auto vertexShader = m_vulkanDevice->createShaderModule(vertShaderCode);
  auto fragmentShader = m_vulkanDevice->createShaderModule(fragShaderCode);

  auto vkPipelineBuilder = m_vulkanDevice->getPipelineBuilder();
  vkPipelineBuilder.addShaderStage(vk::ShaderStageFlagBits::eVertex, vertexShader.get(), "main");
  vkPipelineBuilder.addShaderStage(vk::ShaderStageFlagBits::eFragment, fragmentShader.get(), "main");

  auto bindingDesc = Vertex::getBindingDescription();
  auto attributesDesc = Vertex::getAttributesDescription();
  vkPipelineBuilder.setVertexInputState(bindingDesc, attributesDesc);

  vkPipelineBuilder.setInputAssemblyState(vk::PrimitiveTopology::eTriangleList, false);

  auto extent = m_vulkanSwapchain->getSwapchainExtent();
  vkPipelineBuilder.addViewport({0, 0}, extent, 0.0f, 1.0f);
  vkPipelineBuilder.addScissor({0, 0}, extent);

  vkPipelineBuilder.setRasterizationState(false, false, vk::PolygonMode::eFill, 1.0f, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f);
  vkPipelineBuilder.setDepthStencilState(true, true, vk::CompareOp::eLess, false, 0.0f, 1.0f, false, {}, {});
  vkPipelineBuilder.setMultisampleState(false, vk::SampleCountFlagBits::e1, 1.0f, nullptr, false, false);

  vkPipelineBuilder.addColorBlendAttachment(VulkanPipelineBuilder::colorWriteMaskAll, false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd);
  vkPipelineBuilder.setColorBlendingInfo(false, vk::LogicOp::eCopy, {0.0f, 0.0f, 0.0f, 0.0f});
  vkPipelineBuilder.setPipelineLayoutInfo(m_descriptorSetLayout.get(), nullptr);

  std::tie(m_pipeline, m_pipelineLayout) = vkPipelineBuilder.buildGraphicsPipeline(m_renderPass.get());
}

void VkRenderer::createGBuffer()
{
  auto extent = m_vulkanSwapchain->getSwapchainExtent();

  // Depth
  std::vector<vk::Format> desiredFormats = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};
  auto depthFormat = selectSupportedFormat(desiredFormats, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);

  vk::ImageUsageFlags depthUsageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment;
  vk::ImageAspectFlags depthImgAspect = hasStencilComponent(depthFormat) ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;

  m_depthImage = m_vulkanDevice->createImage(extent, 1, depthFormat, vk::ImageTiling::eOptimal, depthUsageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImgAspect);
  m_vulkanDevice->setObjectName(m_depthImage.get(), "GBuffer:DepthBuffer");
  transitionImage(m_graphicsCmdBuffersTmp[0].get(), m_graphicsQueue, m_depthImage->getImage(), depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

  // https://medium.com/@lordned/unreal-engine-4-rendering-part-4-the-deferred-shading-pipeline-389fc0175789
  // Albedo

  // Normal

  // Material Properties + ShadingModelId

  //
}

void VkRenderer::createRenderPass()
{

  std::vector<vk::AttachmentDescription> attachmentsDesc;

  vk::AttachmentDescription colorAttachment{};
  colorAttachment.format = m_vulkanSwapchain->getFormat();
  colorAttachment.samples = vk::SampleCountFlagBits::e1;
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
  attachmentsDesc.push_back(colorAttachment);

  vk::AttachmentDescription depthAttachment{};
  depthAttachment.format = m_depthImage->getFormat();
  depthAttachment.samples = vk::SampleCountFlagBits::e1;
  depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
  depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
  depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  attachmentsDesc.push_back(depthAttachment);

  vk::AttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::SubpassDescription subpassDesc{};
  subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpassDesc.colorAttachmentCount = 1;
  subpassDesc.pColorAttachments = &colorAttachmentRef;
  subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

  vk::SubpassDependency subpassDependency{};
  subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependency.dstSubpass = 0;
  subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
  subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  vk::RenderPassCreateInfo renderPassCreateInfo{};
  renderPassCreateInfo.attachmentCount = (uint32_t)attachmentsDesc.size();
  renderPassCreateInfo.pAttachments = attachmentsDesc.data();
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDesc;
  renderPassCreateInfo.dependencyCount = 1;
  renderPassCreateInfo.pDependencies = &subpassDependency;

  m_renderPass = m_device->createRenderPassUnique(renderPassCreateInfo);
}

void VkRenderer::createDescriptorSetLayout()
{
  auto builder = m_vulkanDevice->getDescriptorSetLayoutBuilder();
  builder.addDescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);
  builder.addDescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr);

  m_descriptorSetLayout = builder.build();
}

void VkRenderer::createFramebuffers()
{
  auto extent = m_vulkanSwapchain->getSwapchainExtent();
  const auto& imgViews = m_vulkanSwapchain->getImageViews();
  m_swapchainFramebuffers.resize(imgViews.size());

  for (int i = 0; i < imgViews.size(); i++)
  {
    m_swapchainFramebuffers[i] = m_vulkanDevice->createFramebuffer(extent, m_renderPass.get(), {imgViews[i].get(), m_depthImage->getImageView()});
  }
}

void VkRenderer::createVertexBuffer()
{
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto [stagingBuffer, stagingBufferMemory] = m_vulkanDevice->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  auto data = m_device->mapMemory(stagingBufferMemory.get(), 0, bufferSize, vk::MemoryMapFlagBits{});
  memcpy(data, vertices.data(), (size_t)bufferSize);
  m_device->unmapMemory(stagingBufferMemory.get());

  std::tie(m_vertexBuffer, m_vertexBufferMemory) = m_vulkanDevice->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
  copyBuffer(stagingBuffer.get(), m_vertexBuffer.get(), bufferSize);
}

void VkRenderer::createIndexBuffer()
{
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto [stagingBuffer, stagingBufferMemory] = m_vulkanDevice->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  auto data = m_device->mapMemory(stagingBufferMemory.get(), 0, bufferSize, vk::MemoryMapFlagBits{});
  memcpy(data, indices.data(), (size_t)bufferSize);
  m_device->unmapMemory(stagingBufferMemory.get());

  std::tie(m_indexBuffer, m_indexBufferMemory) = m_vulkanDevice->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
  copyBuffer(stagingBuffer.get(), m_indexBuffer.get(), bufferSize);
}

void VkRenderer::createUniformBuffer()
{
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

  m_uboBuffersMemory.resize(VkRenderer::m_frameResourcesCount);
  m_uboBuffers.resize(VkRenderer::m_frameResourcesCount);

  for (size_t i = 0; i < VkRenderer::m_frameResourcesCount; i++)
  {
    std::tie(m_uboBuffers[i], m_uboBuffersMemory[i]) = m_vulkanDevice->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  }
}

void VkRenderer::createDescriptorPool()
{
  auto builder = m_vulkanDevice->getDescriptorPoolBuilder();
  builder.addDescriptorPoolSize(vk::DescriptorType::eUniformBuffer, VkRenderer::m_frameResourcesCount);
  builder.addDescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, VkRenderer::m_frameResourcesCount);
  m_descriptorPool = builder.build(VkRenderer::m_frameResourcesCount);
}

void VkRenderer::createDescriptorSets()
{
  std::vector<vk::DescriptorSetLayout> descriptorLayouts(VkRenderer::m_frameResourcesCount, m_descriptorSetLayout.get());

  vk::DescriptorSetAllocateInfo descriptorSetAllocInfo{};
  descriptorSetAllocInfo.descriptorPool = m_descriptorPool.get();
  descriptorSetAllocInfo.pSetLayouts = descriptorLayouts.data();
  descriptorSetAllocInfo.descriptorSetCount = VkRenderer::m_frameResourcesCount;

  m_descriptorSets = m_device->allocateDescriptorSets(descriptorSetAllocInfo);

  for (size_t i = 0; i < VkRenderer::m_frameResourcesCount; i++)
  {
    vk::DescriptorBufferInfo descriptorBufferInfo{};
    descriptorBufferInfo.buffer = m_uboBuffers[i].get();
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(UniformBufferObject);

    vk::DescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    descriptorImageInfo.imageView = m_vulkanTextureImage->getImageView();
    descriptorImageInfo.sampler = m_textureSampler.get();

    std::array<vk::WriteDescriptorSet, 2> descriptorSetWrites{};
    descriptorSetWrites[0].dstSet = m_descriptorSets[i];
    descriptorSetWrites[0].dstBinding = 0;
    descriptorSetWrites[0].dstArrayElement = 0;
    descriptorSetWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorSetWrites[0].descriptorCount = 1;
    descriptorSetWrites[0].pBufferInfo = &descriptorBufferInfo;

    descriptorSetWrites[1].dstSet = m_descriptorSets[i];
    descriptorSetWrites[1].dstBinding = 1;
    descriptorSetWrites[1].dstArrayElement = 0;
    descriptorSetWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorSetWrites[1].descriptorCount = 1;
    descriptorSetWrites[1].pImageInfo = &descriptorImageInfo;

    m_device->updateDescriptorSets(descriptorSetWrites, nullptr);
  }
}

void VkRenderer::createTextureImage()
{
  int32_t texWidth{};
  int32_t texHeight{};
  int32_t texChannels{};

  //auto texturePath = m_dataPath / "textures" / "texture.jpg";
  auto texturePath = m_dataPath / "textures" / "chalet.jpg";
  auto pixels = stbi_load(texturePath.u8string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels)
  {
    throw std::runtime_error("Failed to load texture image.");
  }

  auto mipLevels = (uint32_t)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

  auto [stagingBuffer, stagingBufferMemory] = m_vulkanDevice->createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  auto data = m_device->mapMemory(stagingBufferMemory.get(), 0, imageSize, vk::MemoryMapFlagBits{});
  memcpy(data, pixels, (size_t)imageSize);
  m_device->unmapMemory(stagingBufferMemory.get());

  stbi_image_free(pixels);

  auto format = vk::Format::eR8G8B8A8Unorm;

  auto imageUsage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
  m_vulkanTextureImage = m_vulkanDevice->createImage({(uint32_t)texWidth, (uint32_t)texHeight}, mipLevels, format, vk::ImageTiling::eOptimal, imageUsage, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor);

  transitionImage(m_transferCmdBuffers[0].get(), m_transferQueue, m_vulkanTextureImage->getImage(), format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
  copyBufferToImage(stagingBuffer.get(), m_vulkanTextureImage->getImage(), (uint32_t)texWidth, (uint32_t)texHeight);
  generateMipmaps(m_graphicsCmdBuffersTmp[0].get(), m_graphicsQueue, m_vulkanTextureImage->getImage(), format, texWidth, texHeight, mipLevels);
}

void VkRenderer::createTextureSampler(uint32_t mipLevels)
{
  vk::SamplerCreateInfo samplerInfo{};
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = true;
  samplerInfo.maxAnisotropy = 16;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = false;
  samplerInfo.compareEnable = false;
  samplerInfo.compareOp = vk::CompareOp::eAlways;

  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = (float)mipLevels;

  m_textureSampler = m_device->createSamplerUnique(samplerInfo);
}

void VkRenderer::copyBuffer(vk::Buffer& srcBuffer, vk::Buffer& dstBuffer, vk::DeviceSize size)
{
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  auto& transferCmdBuffer = m_transferCmdBuffers[0];
  transferCmdBuffer->begin(beginInfo);

  vk::BufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = size;

  transferCmdBuffer->copyBuffer(srcBuffer, dstBuffer, copyRegion);
  transferCmdBuffer->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCmdBuffer.get();

  m_transferQueue.submit(submitInfo, nullptr);
  m_transferQueue.waitIdle();

  transferCmdBuffer->reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

void VkRenderer::copyBufferToImage(vk::Buffer& buffer, vk::Image image, uint32_t width, uint32_t height)
{
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  auto& transferCmdBuffer = m_transferCmdBuffers[0];
  transferCmdBuffer->begin(beginInfo);

  vk::BufferImageCopy copyRegion{};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferRowLength = 0;
  copyRegion.bufferImageHeight = 0;

  copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  copyRegion.imageSubresource.mipLevel = 0;
  copyRegion.imageSubresource.baseArrayLayer = 0;
  copyRegion.imageSubresource.layerCount = 1;

  copyRegion.imageOffset = vk::Offset3D{0, 0, 0};
  copyRegion.imageExtent = vk::Extent3D{width, height, 1};

  transferCmdBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
  transferCmdBuffer->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCmdBuffer.get();

  m_transferQueue.submit(submitInfo, nullptr);
  m_transferQueue.waitIdle();

  transferCmdBuffer->reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

void VkRenderer::transitionImage(vk::CommandBuffer& cmdBuffer, vk::Queue queue, const vk::Image& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels, QueueFamilyIndex srcQueueFamilyIdx, QueueFamilyIndex dstQueueFamilyIdx)
{
  vk::PipelineStageFlags sourceStage{};
  vk::PipelineStageFlags destinationStage{};

  vk::AccessFlags srcAccessFlags{};
  vk::AccessFlags dstAccessFlags{};

  vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;

  if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
  {
    aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (hasStencilComponent(format))
    {
      aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  }

  if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
  {
    srcAccessFlags = vk::AccessFlagBits{};
    dstAccessFlags = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  }
  else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
  {
    srcAccessFlags = vk::AccessFlagBits::eTransferWrite;
    dstAccessFlags = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  }
  else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
  {
    srcAccessFlags = vk::AccessFlagBits{};
    dstAccessFlags = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
  }
  else
  {
    throw std::invalid_argument("Unsuported layout transition.");
  }

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  cmdBuffer.begin(beginInfo);

  vk::ImageMemoryBarrier imgBarrier{};
  imgBarrier.oldLayout = oldLayout;
  imgBarrier.newLayout = newLayout;
  imgBarrier.srcQueueFamilyIndex = srcQueueFamilyIdx;
  imgBarrier.dstQueueFamilyIndex = dstQueueFamilyIdx;

  imgBarrier.image = image;
  imgBarrier.subresourceRange.aspectMask = aspectMask;
  imgBarrier.subresourceRange.baseMipLevel = 0;
  imgBarrier.subresourceRange.levelCount = mipLevels;
  imgBarrier.subresourceRange.baseArrayLayer = 0;
  imgBarrier.subresourceRange.layerCount = 1;

  imgBarrier.srcAccessMask = srcAccessFlags;
  imgBarrier.dstAccessMask = dstAccessFlags;

  cmdBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlagBits{}, nullptr, nullptr, imgBarrier);

  cmdBuffer.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuffer;

  queue.submit(submitInfo, nullptr);
  queue.waitIdle();

  cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

void VkRenderer::updateUniformBuffer(uint32_t currentImage)
{
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  auto extent = m_vulkanSwapchain->getSwapchainExtent();
  UniformBufferObject ubo = {};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
  ubo.proj[1][1] *= -1; // any other way to fix this?

  auto data = m_device->mapMemory(m_uboBuffersMemory[currentImage].get(), 0, sizeof(ubo), vk::MemoryMapFlagBits{});
  memcpy(data, &ubo, (size_t)sizeof(ubo));
  m_device->unmapMemory(m_uboBuffersMemory[currentImage].get());
}

void VkRenderer::update()
{
  m_debugGui->startFrame();
}

void VkRenderer::recordGfxCommandBuffer(const VulkanCurrentFrameResources& currentFrameResources)
{
  auto& commandBuffer = currentFrameResources.m_frameResources->m_graphicsCmdBuffers[0];

  m_debugUtils->beginLabel(commandBuffer.get(), "Geometry");

  std::array<vk::ClearValue, 2> clearColor{};
  clearColor[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
  clearColor[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

  vk::RenderPassBeginInfo renderPassInfo{};
  renderPassInfo.renderPass = *m_renderPass;
  renderPassInfo.framebuffer = m_swapchainFramebuffers[currentFrameResources.m_swapchainImageIndex].get();
  renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
  renderPassInfo.renderArea.extent = currentFrameResources.m_swapchain->getSwapchainExtent();
  renderPassInfo.clearValueCount = (uint32_t)clearColor.size();
  renderPassInfo.pClearValues = clearColor.data();

  commandBuffer->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

  commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout.get(), 0, m_descriptorSets[currentFrameResources.m_frameResourceIndex], nullptr);

  std::array<vk::Buffer, 1> vertexBuffers = {m_vertexBuffer.get()};
  std::array<vk::DeviceSize, 1> offsets = {0};
  commandBuffer->bindVertexBuffers(0, vertexBuffers, offsets);
  commandBuffer->bindIndexBuffer(m_indexBuffer.get(), 0, vk::IndexType::eUint32);

  m_debugUtils->insertLabel(commandBuffer.get(), "DrawIndexedCmd", DebugUtils::m_darkGray);
  commandBuffer->drawIndexed((uint32_t)indices.size(), 1, 0, 0, 0);
  commandBuffer->endRenderPass();
  m_debugUtils->endLabel(commandBuffer.get());
}

void VkRenderer::render()
{
  static VulkanCurrentFrameResources currentFrameResources{};

  currentFrameResources.m_frameResourceIndex = m_currentFrameResourceIndex;
  currentFrameResources.m_frameResources = &m_frameResources[m_currentFrameResourceIndex];
  currentFrameResources.m_swapchain = m_vulkanSwapchain.get();
  currentFrameResources.m_debugUtils = m_debugUtils.get();

  m_device->waitForFences(currentFrameResources.m_frameResources->m_frameFence.get(), VK_TRUE, std::numeric_limits<uint64_t>::max());

  m_device->resetFences(currentFrameResources.m_frameResources->m_frameFence.get());

  try
  {
    m_device->acquireNextImageKHR(m_vulkanSwapchain->getSwapchain(), std::numeric_limits<uint64_t>::max(), currentFrameResources.m_frameResources->m_imageAcquiredSemaphores.get(), nullptr, &currentFrameResources.m_swapchainImageIndex);
  }
  catch (const vk::OutOfDateKHRError& /*exception*/)
  {
    recreateSwapchain();
    return;
  }
  m_debugUtils->beginLabel(m_graphicsQueue, "GfxQueue Begin", DebugUtils::m_yellow);
  updateUniformBuffer(currentFrameResources.m_frameResourceIndex);

  {
    auto& commandBuffer = currentFrameResources.m_frameResources->m_graphicsCmdBuffers[0];

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    beginInfo.pInheritanceInfo = nullptr;
    commandBuffer->begin(beginInfo);

    auto labelStr = std::string("Begin cmdBuffer") + std::to_string(currentFrameResources.m_frameResourceIndex);
    m_debugUtils->beginLabel(commandBuffer.get(), labelStr.c_str(), DebugUtils::m_green);

    recordGfxCommandBuffer(currentFrameResources);

    m_debugGui->recordCommandBuffers(currentFrameResources);

    m_debugUtils->endLabel(commandBuffer.get());
    commandBuffer->end();
  }

  vk::Semaphore waitSemaphores[] = {currentFrameResources.m_frameResources->m_imageAcquiredSemaphores.get()};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

  vk::Semaphore signalSemaphores[] = {currentFrameResources.m_frameResources->m_renderCompletedSemaphores.get()};

  vk::SubmitInfo submitInfo{};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;                                                                   // (uint32_t)currentFrameResources.m_frameResources->m_graphicsCmdBuffers.size();
  submitInfo.pCommandBuffers = &currentFrameResources.m_frameResources->m_graphicsCmdBuffers[0].get(); //currentFrameResources.m_frameResources->m_graphicsCmdBuffers.data();
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  m_debugUtils->insertLabel(m_graphicsQueue, "GfxQueue Submit", DebugUtils::m_lightGray);
  m_graphicsQueue.submit(submitInfo, currentFrameResources.m_frameResources->m_frameFence.get());
  m_debugUtils->endLabel(m_graphicsQueue);

  vk::SwapchainKHR swapchains[] = {m_vulkanSwapchain->getSwapchain()};

  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &currentFrameResources.m_swapchainImageIndex;
  presentInfo.pResults = nullptr;

  m_debugUtils->beginLabel(m_presentQueue, "PresentQueue Begin", DebugUtils::m_blue);
  try
  {
    auto result = m_presentQueue.presentKHR(presentInfo);
    if (result == vk::Result::eSuboptimalKHR)
    {
      recreateSwapchain();
    }
  }
  catch (const vk::OutOfDateKHRError& /*exception*/)
  {
    recreateSwapchain();
  }
  m_debugUtils->endLabel(m_presentQueue);

  m_currentFrameResourceIndex = ++m_currentFrameResourceIndex % VkRenderer::m_frameResourcesCount;
}

} // namespace VkHal

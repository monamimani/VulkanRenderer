#include "VkHal/Vulkan/VulkanDevice.h"

#include <string>

using namespace std::literals::string_literals;

namespace VkHal
{
vk::SurfaceFormatKHR selectSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& surfaceFormats)
{
  if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined)
  {
    return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
  }

  for (const auto& surfaceFormat : surfaceFormats)
  {
    if (surfaceFormat.format == vk::Format::eB8G8R8A8Unorm && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
    {
      return surfaceFormat;
    }
  }

  return surfaceFormats[0];
}

vk::PresentModeKHR selectPresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
  for (const auto& presentMode : presentModes)
  {
    if (presentMode == vk::PresentModeKHR::eMailbox)
    {
      return presentMode;
    }
  }

  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D selectSurfaceExtend(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D windowExtent)
{
  vk::Extent2D surfaceExtend{};

  if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    surfaceExtend = surfaceCapabilities.currentExtent;
  }
  else
  {
    surfaceExtend.width = std::clamp(windowExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
    surfaceExtend.height = std::clamp(windowExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
  }

  return surfaceExtend;
}

VulkanDevice::VulkanDevice(vk::PhysicalDevice physicalDevice, vk::PhysicalDeviceFeatures enabledFeatures, bool isHeadless, QueueFamilyIndices queueFamilyIndices)
    : m_physicalDevice(physicalDevice)
    , m_queueFamilyIndices(queueFamilyIndices)
    , m_physicalDeviceMemoryProperties(m_physicalDevice.getMemoryProperties())
{

  verifyDeviceExtensionAvailability(std::set<std::string>{cbegin(VulkanDevice::m_extensionName), cend(VulkanDevice::m_extensionName)});

  std::set<int32_t> uniqueQueueFamilyIdx;
  uniqueQueueFamilyIdx.insert(m_queueFamilyIndices.graphics);
  uniqueQueueFamilyIdx.insert(m_queueFamilyIndices.compute);
  uniqueQueueFamilyIdx.insert(m_queueFamilyIndices.transfer);
  uniqueQueueFamilyIdx.insert(m_queueFamilyIndices.present);

  constexpr auto priority = 1.0f;

  std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
  deviceQueueCreateInfos.reserve(uniqueQueueFamilyIdx.size());
  for (auto queueFamilyIdx : uniqueQueueFamilyIdx)
  {
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIdx;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &priority;

    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
  }

  vk::DeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfos.size();
  deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
  deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
  deviceCreateInfo.enabledExtensionCount = (uint32_t)VulkanDevice::m_extensionName.size();
  deviceCreateInfo.ppEnabledExtensionNames = VulkanDevice::m_extensionName.data();

  m_device = m_physicalDevice.createDeviceUnique(deviceCreateInfo);
}

void VulkanDevice::initDebugExtention()
{
  m_setObjectNameFct = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device.get(), "vkSetDebugUtilsObjectNameEXT");
  Check(m_setObjectNameFct != nullptr, "Could not find vkSetDebugUtilsObjectNameEXT");
}

std::unique_ptr<VulkanSwapchain> VulkanDevice::recreateSwapchain(vk::Extent2D extent, uint32_t desiredImageCount, const vk::SurfaceKHR& surface, const vk::SwapchainKHR* oldSwapChain)
{
  auto surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(surface);
  auto surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(surface);
  auto surfacePresentModes = m_physicalDevice.getSurfacePresentModesKHR(surface);

  auto selectedSurfaceFormat = selectSurfaceFormat(surfaceFormats);
  auto selectedPresentMode = selectPresentMode(surfacePresentModes);
  auto selectedExtent = selectSurfaceExtend(surfaceCapabilities, extent);

  uint32_t imageCount = std::max(desiredImageCount, surfaceCapabilities.minImageCount);
  if (surfaceCapabilities.maxImageCount > 0)
  {
    imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
  }

  vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
  swapchainCreateInfo.surface = surface;
  swapchainCreateInfo.minImageCount = imageCount;
  swapchainCreateInfo.imageFormat = selectedSurfaceFormat.format;
  swapchainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
  swapchainCreateInfo.imageExtent = selectedExtent;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  swapchainCreateInfo.oldSwapchain = oldSwapChain ? *oldSwapChain : nullptr;

  // Once i implement ownership transition it will be possible to make them exclusive.
  std::array<uint32_t, 2> queueFamilyIndices = {(uint32_t)m_queueFamilyIndices.graphics, (uint32_t)m_queueFamilyIndices.present};
  if (m_queueFamilyIndices.graphics != m_queueFamilyIndices.present)
  {
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    swapchainCreateInfo.queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size();
    swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  }
  else
  {
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
  }

  swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
  swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  swapchainCreateInfo.presentMode = selectedPresentMode;
  swapchainCreateInfo.clipped = VK_TRUE;

  auto swapchain = m_device->createSwapchainKHRUnique(swapchainCreateInfo);
  auto swapchainImages = m_device->getSwapchainImagesKHR(*swapchain);

  std::vector<vk::UniqueImageView> swapchainImageViews;
  swapchainImageViews.reserve(swapchainImages.size());
  for (const auto& image : swapchainImages)
  {
    swapchainImageViews.emplace_back(createImageView(image, selectedSurfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1));
  }

  return std::make_unique<VulkanSwapchain>(std::move(swapchain), std::move(swapchainImageViews), selectedExtent, selectedSurfaceFormat, selectedPresentMode);
}

uint32_t VulkanDevice::selectMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
  for (uint32_t i = 0; i < m_physicalDeviceMemoryProperties.memoryTypeCount; i++)
  {
    if ((typeFilter & (1 << i)) && (m_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type!");
}

std::unique_ptr<VulkanImage> VulkanDevice::createImage(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlags imgAspectflags) const
{
  auto [image, imageMemory] = createImage(extent, mipLevels, format, tiling, usage, properties);
  auto imageView = createImageView(*image, format, imgAspectflags, mipLevels);

  return std::make_unique<VulkanImage>(std::move(imageMemory), std::move(image), std::move(imageView), format, mipLevels);
}

std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> VulkanDevice::createImage(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) const
{
  vk::ImageCreateInfo imgCreateInfo{};
  imgCreateInfo.imageType = vk::ImageType::e2D;
  imgCreateInfo.extent.width = extent.width;
  imgCreateInfo.extent.height = extent.height;
  imgCreateInfo.extent.depth = 1;
  imgCreateInfo.format = format;
  imgCreateInfo.mipLevels = mipLevels;
  imgCreateInfo.arrayLayers = 1;
  imgCreateInfo.tiling = tiling;
  imgCreateInfo.usage = usage;
  imgCreateInfo.samples = vk::SampleCountFlagBits::e1;
  imgCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
  imgCreateInfo.sharingMode = vk::SharingMode::eExclusive;

  auto image = m_device->createImageUnique(imgCreateInfo);

  vk::MemoryRequirements memRequirements = m_device->getImageMemoryRequirements(image.get());

  vk::MemoryAllocateInfo allocInfo = {};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = selectMemoryType(memRequirements.memoryTypeBits, properties);

  auto memory = m_device->allocateMemoryUnique(allocInfo);
  m_device->bindImageMemory(image.get(), memory.get(), 0);

  return std::make_tuple(std::move(image), std::move(memory));
}

vk::UniqueImageView VulkanDevice::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const
{
  vk::ImageViewCreateInfo imgViewCreateInfo = {};
  imgViewCreateInfo.image = image;
  imgViewCreateInfo.viewType = vk::ImageViewType::e2D;
  imgViewCreateInfo.format = format;
  imgViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
  imgViewCreateInfo.subresourceRange.levelCount = mipLevels;
  imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
  imgViewCreateInfo.subresourceRange.layerCount = 1;

  return m_device->createImageViewUnique(imgViewCreateInfo);
}

vk::UniqueFramebuffer VulkanDevice::createFramebuffer(vk::Extent2D extent, const vk::RenderPass& renderPass, vk::ArrayProxy<const vk::ImageView> attachments) const
{
  vk::FramebufferCreateInfo frameBufferCreateInfo{};
  frameBufferCreateInfo.renderPass = renderPass;
  frameBufferCreateInfo.attachmentCount = (uint32_t)attachments.size();
  frameBufferCreateInfo.pAttachments = attachments.data();
  frameBufferCreateInfo.width = extent.width;
  frameBufferCreateInfo.height = extent.height;
  frameBufferCreateInfo.layers = 1;

  return m_device->createFramebufferUnique(frameBufferCreateInfo);
}

std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> VulkanDevice::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperties) const
{
  std::array<QueueFamilyIndex, 2> queueFamilyIndices = {m_queueFamilyIndices.transfer, m_queueFamilyIndices.graphics};
  vk::BufferCreateInfo bufferInfo = {};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = vk::SharingMode::eConcurrent; // for staging buffer this could be exclusive and transfer the ownership of the buffer with a memory barrier since with a staging buffer, the buffer wouldn't be used by 2 queue at the same time
  bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  bufferInfo.queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size();

  auto buffer = m_device->createBufferUnique(bufferInfo);

  auto memoryRequirements = m_device->getBufferMemoryRequirements(buffer.get());

  vk::MemoryAllocateInfo memoryAllocateInfo = {};
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = selectMemoryType(memoryRequirements.memoryTypeBits, memoryProperties);

  auto memory = m_device->allocateMemoryUnique(memoryAllocateInfo);

  m_device->bindBufferMemory(buffer.get(), memory.get(), 0);

  return std::make_tuple(std::move(buffer), std::move(memory));
}

vk::UniqueSemaphore VulkanDevice::createSemaphore() const
{
  vk::SemaphoreCreateInfo semaphoreCreateInfo{};
  return m_device->createSemaphoreUnique(semaphoreCreateInfo);
}

vk::UniqueFence VulkanDevice::createFence(bool createSignaled) const
{
  vk::FenceCreateInfo fenceCreateInfo{};
  fenceCreateInfo.flags = createSignaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlagBits{};
  return m_device->createFenceUnique(fenceCreateInfo);
}

vk::UniqueCommandPool VulkanDevice::createCommandPool(QueueFamilyIndex queueFamilyIndx, vk::CommandPoolCreateFlags cmdPoolFlags) const
{
  vk::CommandPoolCreateInfo cmdPoolCreateInfo{};
  cmdPoolCreateInfo.flags = cmdPoolFlags;
  cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIndx;

  return m_device->createCommandPoolUnique(cmdPoolCreateInfo);
}

std::vector<vk::UniqueCommandBuffer> VulkanDevice::allocateCommandBuffer(vk::CommandPool& cmdPool, uint32_t count, bool isPrimary) const
{
  vk::CommandBufferAllocateInfo cmdBufferAllocateInfo{};
  cmdBufferAllocateInfo.level = isPrimary ? vk::CommandBufferLevel::ePrimary : vk::CommandBufferLevel::eSecondary;
  cmdBufferAllocateInfo.commandPool = cmdPool;
  cmdBufferAllocateInfo.commandBufferCount = count;

  return m_device->allocateCommandBuffersUnique(cmdBufferAllocateInfo);
}

vk::UniqueShaderModule VulkanDevice::createShaderModule(const std::vector<char>& code) const
{
  vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
  shaderModuleCreateInfo.codeSize = code.size();
  shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  return m_device->createShaderModuleUnique(shaderModuleCreateInfo);
}

void VulkanDevice::resetCommandPool(vk::CommandPool cmdPool) const
{
  m_device->resetCommandPool(cmdPool, vk::CommandPoolResetFlagBits::eReleaseResources);
}

void VulkanDevice::setObjectName(const VulkanImage* vulkanImage, const char* name)
{
  setObjectName(vulkanImage->getImage(), vk::ObjectType::eImage, name);
  setObjectName(vulkanImage->getImageView(), vk::ObjectType::eImageView, (std::string(name) + "_view"s).c_str());
}

void VulkanDevice::verifyDeviceExtensionAvailability(std::set<std::string> extensionNames) const
{
  auto deviceExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();

  for (const auto& deviceExtension : deviceExtensions)
  {
    extensionNames.erase(deviceExtension.extensionName);
    if (extensionNames.empty())
    {
      break;
    }
  }

  if (!extensionNames.empty())
  {
    throw std::runtime_error("Device extension not supported.");
  }
}
} // namespace VkHal

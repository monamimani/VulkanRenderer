#pragma once

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "VkHal/Vulkan/VulkanBuilder/VulkanDescriptorPoolBuilder.h"
#include "VkHal/Vulkan/VulkanBuilder/VulkanDescriptorSetLayoutBuilder.h"
#include "VkHal/Vulkan/VulkanBuilder/VulkanPipelineBuilder.h"
#include "VkHal/Vulkan/VulkanImage.h"
#include "VkHal/Vulkan/VulkanSwapchain.h"
#include "VkHal/Vulkan/VulkanUtils.h"

namespace VkHal
{
enum class QueueFamilyType
{
  Graphics = 0,
  Compute = 1,
  Transfer = 2,
  Present = 3,
  Count
};

using QueueFamilyIndex = uint32_t;
/** @brief Contains queue family indices.  */
struct QueueFamilyIndices
{
  QueueFamilyIndex graphics = -1;
  QueueFamilyIndex compute = -1;
  QueueFamilyIndex transfer = -1;
  QueueFamilyIndex present = -1;
};

class VulkanDevice
{

public:
  static constexpr std::array<const char*, 1> m_extensionName = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VulkanDevice(vk::PhysicalDevice physicalDevice, vk::PhysicalDeviceFeatures enabledFeatures, bool isHeadless, QueueFamilyIndices queueFamilyIndices);
  ~VulkanDevice() = default;

  void initDebugExtention();

  // Temporary
  const auto& getDevice() const
  {
    return m_device.get();
  }

  const auto& getPhysicalDevice() const
  {
    return m_physicalDevice;
  }

  // Temporary
  const auto getQueues()
  {
    /** @brief Array that store the vulkan queue. Each index correspond to one queue family.*/
    std::array<vk::Queue, (size_t)QueueFamilyType::Count> queues;
    queues[(size_t)QueueFamilyType::Graphics] = m_device->getQueue(m_queueFamilyIndices.graphics, 0);
    queues[(size_t)QueueFamilyType::Compute] = m_device->getQueue(m_queueFamilyIndices.compute, 0);
    queues[(size_t)QueueFamilyType::Transfer] = m_device->getQueue(m_queueFamilyIndices.transfer, 0);
    queues[(size_t)QueueFamilyType::Present] = m_device->getQueue(m_queueFamilyIndices.present, 0);

    return queues;
  }

  auto getPipelineBuilder() const
  {
    return VulkanPipelineBuilder{m_device.get()};
  }

  auto getDescriptorSetLayoutBuilder() const
  {
    return VulkanDescriptorSetLayoutBuilder{m_device.get()};
  }

  auto getDescriptorPoolBuilder() const
  {
    return VulkanDescriptorPoolBuilder{m_device.get()};
  }
  std::unique_ptr<VulkanSwapchain> recreateSwapchain(vk::Extent2D extent, uint32_t desiredImageCount, const vk::SurfaceKHR& surface, const vk::SwapchainKHR* oldSwapChain);
  auto createSwapchain(vk::Extent2D extent, uint32_t desiredImageCount, const vk::SurfaceKHR& surface)
  {
    return recreateSwapchain(extent, desiredImageCount, surface, nullptr);
  }

  std::unique_ptr<VulkanImage> createImage(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlags imgAspectflags) const;
  std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> createImage(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) const;
  vk::UniqueImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const;

  vk::UniqueFramebuffer createFramebuffer(vk::Extent2D extent, const vk::RenderPass& renderPass, vk::ArrayProxy<const vk::ImageView> attachments) const;

  std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperties) const;

  vk::UniqueSemaphore createSemaphore() const;
  vk::UniqueFence createFence(bool createSignaled) const;
  vk::UniqueCommandPool createCommandPool(QueueFamilyIndex queueFamilyIndx, vk::CommandPoolCreateFlags cmdPoolFlags) const;
  std::vector<vk::UniqueCommandBuffer> allocateCommandBuffer(vk::CommandPool& cmdPool, uint32_t count, bool isPrimary) const;

  vk::UniqueShaderModule createShaderModule(const std::vector<char>& code) const;

  void resetCommandPool(vk::CommandPool cmdPool) const;

  template <typename VkHandle_t>
  void setObjectName(VkHandle_t objHandle, vk::ObjectType objType, const char* name);

  void setObjectName(const VulkanImage* vulkanImage, const char* name);

private:
  void verifyDeviceExtensionAvailability(std::set<std::string> extensionNames) const;
  uint32_t selectMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

  PFN_vkSetDebugUtilsObjectNameEXT m_setObjectNameFct;

  /** @brief Physical device representation. */
  vk::PhysicalDevice m_physicalDevice;

  /** @brief Logical device representation (application's view of the device). */
  vk::UniqueDevice m_device;

  /** @brief Memory types and heaps of the physical device. */
  vk::PhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

  /** @brief The index of the QueueFamily.*/
  QueueFamilyIndices m_queueFamilyIndices;
};

template <typename VkHandle_t>
void VulkanDevice::setObjectName(VkHandle_t objHandle, vk::ObjectType objType, const char* name)
{
  if (!m_setObjectNameFct)
  {
    return;
  }

  vk::DebugUtilsObjectNameInfoEXT nameInfo{};
  nameInfo.objectHandle = reinterpret_cast<uint64_t&>(objHandle);
  nameInfo.objectType = objType;
  nameInfo.pObjectName = name;

  vkCheck(m_setObjectNameFct(m_device.get(), reinterpret_cast<VkDebugUtilsObjectNameInfoEXT*>(&nameInfo)));
}

} // namespace VkHal

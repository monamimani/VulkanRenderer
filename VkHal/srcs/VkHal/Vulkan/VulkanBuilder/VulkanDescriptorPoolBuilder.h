#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VkHal
{
class VulkanDescriptorPoolBuilder
{
public:
  VulkanDescriptorPoolBuilder(const vk::Device& device);
  ~VulkanDescriptorPoolBuilder() = default;

  VulkanDescriptorPoolBuilder addDescriptorPoolSize(vk::DescriptorType descriptorType, uint32_t descriptorCount);

  vk::UniqueDescriptorPool build(uint32_t maxPoolSize);

private:
  const vk::Device& m_device;

  std::vector<vk::DescriptorPoolSize> m_descriptorPoolSizeArray;
};
} // namespace VkHal

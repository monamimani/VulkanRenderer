#include "VulkanDescriptorPoolBuilder.h"

namespace VkHal
{
VulkanDescriptorPoolBuilder::VulkanDescriptorPoolBuilder(const vk::Device& device)
    : m_device{device}
{
}
VulkanDescriptorPoolBuilder VulkanDescriptorPoolBuilder::addDescriptorPoolSize(vk::DescriptorType descriptorType, uint32_t descriptorCount)
{
  vk::DescriptorPoolSize descriptorPoolSize{};
  descriptorPoolSize.type = descriptorType;
  descriptorPoolSize.descriptorCount = descriptorCount;

  m_descriptorPoolSizeArray.push_back(descriptorPoolSize);

  return *this;
}
vk::UniqueDescriptorPool VulkanDescriptorPoolBuilder::build(uint32_t maxPoolSize)
{
  vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.poolSizeCount = (uint32_t)m_descriptorPoolSizeArray.size();
  descriptorPoolInfo.pPoolSizes = m_descriptorPoolSizeArray.data();
  descriptorPoolInfo.maxSets = maxPoolSize;

  return m_device.createDescriptorPoolUnique(descriptorPoolInfo);
}
} // namespace VkHal

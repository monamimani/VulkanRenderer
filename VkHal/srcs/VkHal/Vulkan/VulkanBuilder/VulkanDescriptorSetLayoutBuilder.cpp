#include "VulkanDescriptorSetLayoutBuilder.h"

namespace VkHal
{
VulkanDescriptorSetLayoutBuilder::VulkanDescriptorSetLayoutBuilder(const vk::Device& device)
    : m_device{device}
{
}

VulkanDescriptorSetLayoutBuilder VulkanDescriptorSetLayoutBuilder::addDescriptorSetLayoutBinding(uint32_t bindPoint, vk::DescriptorType descriptorType, uint32_t descriptorCount, vk::ShaderStageFlagBits shaderStage, const vk::Sampler* immutableSampler)
{
  vk::DescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = bindPoint;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = descriptorCount;
  layoutBinding.stageFlags = shaderStage;
  layoutBinding.pImmutableSamplers = immutableSampler;

  m_bindings.push_back(layoutBinding);

  return *this;
}

vk::UniqueDescriptorSetLayout VulkanDescriptorSetLayoutBuilder::build()
{
  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.bindingCount = (uint32_t)m_bindings.size();
  descriptorSetLayoutInfo.pBindings = m_bindings.data();

  return m_device.createDescriptorSetLayoutUnique(descriptorSetLayoutInfo);
}

} // namespace VkHal

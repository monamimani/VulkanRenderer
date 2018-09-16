#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>

namespace VkHal
{
class VulkanDescriptorSetLayoutBuilder
{
public:
  VulkanDescriptorSetLayoutBuilder(const vk::Device& device);
  ~VulkanDescriptorSetLayoutBuilder() = default;

  VulkanDescriptorSetLayoutBuilder addDescriptorSetLayoutBinding(uint32_t bindPoint, vk::DescriptorType descriptorType, uint32_t descriptorCount, vk::ShaderStageFlagBits shaderStage, const vk::Sampler* immutableSampler);

  vk::UniqueDescriptorSetLayout build();

private:
  const vk::Device& m_device;

  std::vector<vk::DescriptorSetLayoutBinding> m_bindings;
};

} // namespace VkHal

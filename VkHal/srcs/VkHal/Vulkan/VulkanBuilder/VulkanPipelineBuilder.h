#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VkHal
{
class VulkanPipelineBuilder
{
public:
  static constexpr vk::ColorComponentFlags colorWriteMaskAll = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  VulkanPipelineBuilder(const vk::Device& device);
  ~VulkanPipelineBuilder() = default;

  VulkanPipelineBuilder addShaderStage(vk::ShaderStageFlagBits shaderStage, const vk::ShaderModule& shaderModule, const char* entryName, vk::SpecializationInfo* specialization = nullptr);

  VulkanPipelineBuilder setVertexInputState(vk::ArrayProxy<vk::VertexInputBindingDescription> inputBindingDescArray, vk::ArrayProxy<vk::VertexInputAttributeDescription> attributeDescArray);
  VulkanPipelineBuilder setInputAssemblyState(vk::PrimitiveTopology topology, bool primitiveRestartEnable);

  VulkanPipelineBuilder addViewport(vk::Offset2D offset, vk::Extent2D extent, float minDepth, float maxDepth); // should the offset and extent actually be floats?
  VulkanPipelineBuilder addScissor(vk::Offset2D offset, vk::Extent2D extent);

  VulkanPipelineBuilder setRasterizationState(bool depthClampEnable, bool rasterizerDiscardEnable, vk::PolygonMode polygonMode, float lineWidth, vk::CullModeFlagBits cullMode, vk::FrontFace frontFace, bool depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);
  VulkanPipelineBuilder setDepthStencilState(bool depthTestEnable, bool depthWriteEnable, vk::CompareOp compareOp, bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds, bool stencilTestEnable, vk::StencilOpState front, vk::StencilOpState back);
  VulkanPipelineBuilder setMultisampleState(bool sampleShadingEnable, vk::SampleCountFlagBits sampleCount, float minSampleShading, const vk::SampleMask* sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable);

  VulkanPipelineBuilder addColorBlendAttachment(vk::ColorComponentFlags colorWriteMask, bool blendEnable, vk::BlendFactor srcColorBlendFactor, vk::BlendFactor dstColorBlendFactor, vk::BlendFactor srcAlphaBlendFactor, vk::BlendFactor dstAlphaBlendFactor, vk::BlendOp alphaBlendOp);
  VulkanPipelineBuilder setColorBlendingInfo(bool logicOpEnable, vk::LogicOp logicOp, std::array<float, 4> blendConstants);

  VulkanPipelineBuilder setPipelineLayoutInfo(vk::ArrayProxy<vk::DescriptorSetLayout> descriptorSetLayoutArray, vk::ArrayProxy<vk::PushConstantRange> pushConstantArray);

  std::tuple<vk::UniquePipeline, vk::UniquePipelineLayout> buildGraphicsPipeline(vk::RenderPass& renderPass);

private:
  const vk::Device& m_device;

  // Programmable part of the pipeline
  std::vector<vk::PipelineShaderStageCreateInfo> m_shaderStages;

  // Fixed-function part of the pipeline
  vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo = {};
  vk::PipelineInputAssemblyStateCreateInfo m_inputAssemblyInfo = {};

  vk::PipelineRasterizationStateCreateInfo m_rasterizerStateInfo = {};
  vk::PipelineDepthStencilStateCreateInfo m_depthStencilStateInfo = {};

  vk::PipelineMultisampleStateCreateInfo m_multisamplingInfo = {};

  std::vector<vk::PipelineColorBlendAttachmentState> m_colorBlendAttachments;
  vk::PipelineColorBlendStateCreateInfo m_colorBlendingInfo = {};

  std::vector<vk::Viewport> m_viewports;
  std::vector<vk::Rect2D> m_scissors;

  vk::PipelineLayoutCreateInfo m_pipelineLayoutInfo = {};
};

} // namespace VkHal

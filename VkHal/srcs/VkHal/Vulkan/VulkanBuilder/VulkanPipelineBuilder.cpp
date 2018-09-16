#include "VulkanPipelineBuilder.h"

namespace VkHal
{

VulkanPipelineBuilder::VulkanPipelineBuilder(const vk::Device& device)
    : m_device{device}
{
}

VulkanPipelineBuilder VulkanPipelineBuilder::addShaderStage(vk::ShaderStageFlagBits shaderStage, const vk::ShaderModule& shaderModule, const char* entryName, vk::SpecializationInfo* specialization)
{
  vk::PipelineShaderStageCreateInfo shaderStageInfo{};
  shaderStageInfo.stage = shaderStage;
  shaderStageInfo.module = shaderModule;
  shaderStageInfo.pName = entryName;
  shaderStageInfo.pSpecializationInfo = specialization; // to specialize the shader at pipeline creation time.

  m_shaderStages.push_back(shaderStageInfo);

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setVertexInputState(vk::ArrayProxy<vk::VertexInputBindingDescription> inputBindingDescArray, vk::ArrayProxy<vk::VertexInputAttributeDescription> attributeDescArray)
{
  m_vertexInputInfo.vertexBindingDescriptionCount = inputBindingDescArray.size();
  m_vertexInputInfo.pVertexBindingDescriptions = inputBindingDescArray.data();
  m_vertexInputInfo.vertexAttributeDescriptionCount = attributeDescArray.size();
  m_vertexInputInfo.pVertexAttributeDescriptions = attributeDescArray.data();

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setInputAssemblyState(vk::PrimitiveTopology topology, bool primitiveRestartEnable)
{
  m_inputAssemblyInfo.topology = topology;
  m_inputAssemblyInfo.primitiveRestartEnable = primitiveRestartEnable;

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::addViewport(vk::Offset2D offset, vk::Extent2D extent, float minDepth, float maxDepth)
{
  vk::Viewport viewport{};
  viewport.x = (float)offset.x;
  viewport.y = (float)offset.y;
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.minDepth = minDepth;
  viewport.maxDepth = maxDepth;

  m_viewports.push_back(viewport);

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::addScissor(vk::Offset2D offset, vk::Extent2D extent)
{
  vk::Rect2D scissor{};
  scissor.offset = offset;
  scissor.extent = extent;

  m_scissors.push_back(scissor);

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setRasterizationState(bool depthClampEnable, bool rasterizerDiscardEnable, vk::PolygonMode polygonMode, float lineWidth, vk::CullModeFlagBits cullMode, vk::FrontFace frontFace, bool depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
  m_rasterizerStateInfo.depthClampEnable = depthClampEnable;
  m_rasterizerStateInfo.rasterizerDiscardEnable = rasterizerDiscardEnable;
  m_rasterizerStateInfo.polygonMode = polygonMode;
  m_rasterizerStateInfo.lineWidth = lineWidth;
  m_rasterizerStateInfo.cullMode = cullMode;
  m_rasterizerStateInfo.frontFace = frontFace;
  m_rasterizerStateInfo.depthBiasEnable = depthBiasEnable;
  m_rasterizerStateInfo.depthBiasConstantFactor = depthBiasConstantFactor;
  m_rasterizerStateInfo.depthBiasClamp = depthBiasClamp;
  m_rasterizerStateInfo.depthBiasSlopeFactor = depthBiasSlopeFactor;

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setDepthStencilState(bool depthTestEnable, bool depthWriteEnable, vk::CompareOp compareOp, bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds, bool stencilTestEnable, vk::StencilOpState front, vk::StencilOpState back)
{
  m_depthStencilStateInfo.depthTestEnable = depthTestEnable;
  m_depthStencilStateInfo.depthWriteEnable = depthWriteEnable;
  m_depthStencilStateInfo.depthCompareOp = compareOp;
  m_depthStencilStateInfo.depthBoundsTestEnable = depthBoundsTestEnable;
  m_depthStencilStateInfo.minDepthBounds = minDepthBounds;
  m_depthStencilStateInfo.maxDepthBounds = maxDepthBounds;
  m_depthStencilStateInfo.stencilTestEnable = stencilTestEnable;
  m_depthStencilStateInfo.front = front;
  m_depthStencilStateInfo.back = back;

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setMultisampleState(bool sampleShadingEnable, vk::SampleCountFlagBits sampleCount, float minSampleShading, const vk::SampleMask* sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable)
{
  m_multisamplingInfo.sampleShadingEnable = sampleShadingEnable;
  m_multisamplingInfo.rasterizationSamples = sampleCount;
  m_multisamplingInfo.minSampleShading = minSampleShading;
  m_multisamplingInfo.pSampleMask = sampleMask;
  m_multisamplingInfo.alphaToCoverageEnable = alphaToCoverageEnable;
  m_multisamplingInfo.alphaToOneEnable = alphaToOneEnable;

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::addColorBlendAttachment(vk::ColorComponentFlags colorWriteMask, bool blendEnable, vk::BlendFactor srcColorBlendFactor, vk::BlendFactor dstColorBlendFactor, vk::BlendFactor srcAlphaBlendFactor, vk::BlendFactor dstAlphaBlendFactor, vk::BlendOp alphaBlendOp)
{
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = colorWriteMask;
  colorBlendAttachment.blendEnable = blendEnable;
  colorBlendAttachment.srcColorBlendFactor = srcColorBlendFactor;
  colorBlendAttachment.dstColorBlendFactor = dstColorBlendFactor;
  colorBlendAttachment.srcAlphaBlendFactor = srcAlphaBlendFactor;
  colorBlendAttachment.dstAlphaBlendFactor = dstAlphaBlendFactor;
  colorBlendAttachment.alphaBlendOp = alphaBlendOp;

  m_colorBlendAttachments.push_back(colorBlendAttachment);
  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setColorBlendingInfo(bool logicOpEnable, vk::LogicOp logicOp, std::array<float, 4> blendConstants)
{
  m_colorBlendingInfo.logicOpEnable = logicOpEnable;
  m_colorBlendingInfo.logicOp = logicOp;
  m_colorBlendingInfo.blendConstants[0] = blendConstants[0];
  m_colorBlendingInfo.blendConstants[1] = blendConstants[1];
  m_colorBlendingInfo.blendConstants[2] = blendConstants[2];
  m_colorBlendingInfo.blendConstants[3] = blendConstants[3];

  return *this;
}

VulkanPipelineBuilder VulkanPipelineBuilder::setPipelineLayoutInfo(vk::ArrayProxy<vk::DescriptorSetLayout> descriptorSetLayoutArray, vk::ArrayProxy<vk::PushConstantRange> pushConstantArray)
{
  m_pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutArray.size();
  m_pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutArray.data();
  m_pipelineLayoutInfo.pushConstantRangeCount = pushConstantArray.size();
  m_pipelineLayoutInfo.pPushConstantRanges = pushConstantArray.data();

  return *this;
}

std::tuple<vk::UniquePipeline, vk::UniquePipelineLayout> VulkanPipelineBuilder::buildGraphicsPipeline(vk::RenderPass& renderPass)
{
  auto pipelineLayout = m_device.createPipelineLayoutUnique(m_pipelineLayoutInfo);

  m_colorBlendingInfo.attachmentCount = (uint32_t)m_colorBlendAttachments.size();
  m_colorBlendingInfo.pAttachments = m_colorBlendAttachments.data();

  vk::PipelineViewportStateCreateInfo viewportStateInfo{};
  viewportStateInfo.viewportCount = (uint32_t)m_viewports.size();
  viewportStateInfo.pViewports = m_viewports.data();
  viewportStateInfo.scissorCount = (uint32_t)m_scissors.size();
  viewportStateInfo.pScissors = m_scissors.data();

  vk::GraphicsPipelineCreateInfo gfxPipelineInfo{};
  gfxPipelineInfo.stageCount = (uint32_t)m_shaderStages.size();
  gfxPipelineInfo.pStages = m_shaderStages.data();
  gfxPipelineInfo.pVertexInputState = &m_vertexInputInfo;
  gfxPipelineInfo.pInputAssemblyState = &m_inputAssemblyInfo;
  gfxPipelineInfo.pViewportState = &viewportStateInfo;
  gfxPipelineInfo.pRasterizationState = &m_rasterizerStateInfo;
  gfxPipelineInfo.pMultisampleState = &m_multisamplingInfo;
  gfxPipelineInfo.pColorBlendState = &m_colorBlendingInfo;
  gfxPipelineInfo.pDepthStencilState = &m_depthStencilStateInfo;
  gfxPipelineInfo.pDynamicState = nullptr;
  gfxPipelineInfo.layout = pipelineLayout.get();
  gfxPipelineInfo.renderPass = renderPass;
  gfxPipelineInfo.subpass = 0;
  gfxPipelineInfo.basePipelineHandle = nullptr;
  gfxPipelineInfo.basePipelineIndex = -1;

  auto pipeline = m_device.createGraphicsPipelineUnique(nullptr, gfxPipelineInfo);

  return std::make_tuple(std::move(pipeline), std::move(pipelineLayout));
}

} // namespace VkHal

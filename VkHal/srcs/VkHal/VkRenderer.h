#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <memory>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>

#include "DebugGui/DebugGui.h"
#include "VkHal/VkHalDefines.h"
#include "VkHal/Vulkan/VulkanDebug.h"
#include "VkHal/Vulkan/VulkanDevice.h"
#include "VkHal/Vulkan/VulkanImage.h"

namespace VkHal
{
struct GBuffer
{
  std::unique_ptr<VulkanImage> m_depthImage;
  //std::unique_ptr<VulkanImage> m_albedoImage;
  //std::unique_ptr<VulkanImage> m_normalImage;
  //std::unique_ptr<VulkanImage> m_materialPropImage;
};

struct VulkanFrameResources
{
  vk::UniqueSemaphore m_imageAcquiredSemaphores;
  vk::UniqueSemaphore m_renderCompletedSemaphores;
  vk::UniqueFence m_frameFence;

  vk::UniqueCommandPool m_graphicsCmdPool;
  std::vector<vk::UniqueCommandBuffer> m_graphicsCmdBuffers;

  vk::UniqueCommandPool m_computeCmdPool;
  std::vector<vk::UniqueCommandBuffer> m_computeCmdBuffers;

  vk::UniqueCommandPool m_transferCmdPool;
  std::vector<vk::UniqueCommandBuffer> m_transferCmdBuffers;

  vk::UniqueCommandPool m_presentCmdPool;
  std::vector<vk::UniqueCommandBuffer> m_presentCmdBuffers;
};

struct VulkanCurrentFrameResources
{
  DebugUtils* m_debugUtils;
  uint32_t m_frameResourceIndex = {};
  VulkanFrameResources* m_frameResources = {};
  const VulkanSwapchain* m_swapchain = {};
  uint32_t m_swapchainImageIndex = {};
};

class VkRenderer
{
public:
  VKHAL_API VkRenderer(bool isHeadless, bool enableValidation, const std::string& appName);
  VKHAL_API ~VkRenderer();

  VKHAL_API void initialize(HINSTANCE appInstance, HWND windowHandle);
  VKHAL_API void recreateSwapchain();
  VKHAL_API void prepare(uint32_t windowWidth = 0, uint32_t windowHeight = 0);
  VKHAL_API void update();
  VKHAL_API void render();

private:
  using QueueFamilyIndex = uint32_t;

  void createSurface(HINSTANCE appInstance, HWND windowHandle);
  void createDeviceAndQueues(const vk::PhysicalDevice& physicalDevice);
  void createFrameResources();

  void createCommandPools();
  void createCommandBuffers();

  void createGBuffer();
  void createRenderPass();
  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void createFramebuffers();

  void createVertexBuffer();
  void createIndexBuffer();
  void createUniformBuffer();
  void createDescriptorPool();
  void createDescriptorSets();
  void createTextureImage();
  void createTextureSampler(uint32_t mipLevels);

  std::vector<vk::PhysicalDevice> selectPhysicalDevice();
  vk::Format selectSupportedFormat(const std::vector<vk::Format>& formats, vk::ImageTiling desiredTilling, vk::FormatFeatureFlags featuresDesired);

  void copyBuffer(vk::Buffer& srcBuffer, vk::Buffer& dstBuffer, vk::DeviceSize size);
  void copyBufferToImage(vk::Buffer& buffer, vk::Image image, uint32_t width, uint32_t height);
  void transitionImage(vk::CommandBuffer& cmdBuffer, vk::Queue queue, const vk::Image& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels, QueueFamilyIndex srcQueueFamilyIdx, QueueFamilyIndex dstQueueFamilyIdx);

  void generateMipmaps(vk::CommandBuffer& cmdBuffer, vk::Queue queue, vk::Image image, vk::Format format, int32_t width, int32_t height, uint32_t mipLevels);
  void recordGfxCommandBuffer(const VulkanCurrentFrameResources& currentFrameResources);
  void updateUniformBuffer(uint32_t currentImage);

  const bool m_isHeadless = true;
  const bool m_enableValidation = false;
  uint32_t m_frameResourcesCount = 3;

  std::filesystem::path m_dataPath;
  uint32_t m_currentFrameResourceIndex = 0;

  std::unique_ptr<DevGuiRenderer> m_debugGui;

  HINSTANCE m_appInstance = {};
  HWND m_windowHandle = {};

  uint32_t m_windowWidth = 0;
  uint32_t m_windowHeight = 0;

  vk::UniqueInstance m_instance;
  std::unique_ptr<VkDebugReport> m_vkDebugReport;
  std::unique_ptr<DebugUtils> m_debugUtils;
  vk::UniqueSurfaceKHR m_surface;

  std::unique_ptr<VulkanDevice> m_vulkanDevice;

  vk::PhysicalDevice m_physicalDevice;
  QueueFamilyIndices m_queueFamilyIndices;

  const vk::Device* m_device;
  vk::Queue m_graphicsQueue;
  vk::Queue m_computeQueue;
  vk::Queue m_transferQueue;
  vk::Queue m_presentQueue;

  vk::UniqueCommandPool m_graphicsCmdPoolTmp;
  std::vector<vk::UniqueCommandBuffer> m_graphicsCmdBuffersTmp;

  vk::UniqueCommandPool m_transferCmdPool;
  std::vector<vk::UniqueCommandBuffer> m_transferCmdBuffers;

  std::unique_ptr<VulkanSwapchain> m_vulkanSwapchain;

  std::vector<VulkanFrameResources> m_frameResources;

  std::unique_ptr<VulkanImage> m_depthImage;

  vk::UniqueRenderPass m_renderPass;

  vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
  vk::UniquePipelineLayout m_pipelineLayout;
  vk::UniquePipeline m_pipeline;

  std::vector<vk::UniqueFramebuffer> m_swapchainFramebuffers;

  vk::UniqueDeviceMemory m_vertexBufferMemory;
  vk::UniqueBuffer m_vertexBuffer;

  vk::UniqueDeviceMemory m_indexBufferMemory;
  vk::UniqueBuffer m_indexBuffer;

  vk::UniqueDescriptorPool m_descriptorPool;
  std::vector<vk::DescriptorSet> m_descriptorSets;
  std::vector<vk::UniqueDeviceMemory> m_uboBuffersMemory;
  std::vector<vk::UniqueBuffer> m_uboBuffers;

  std::unique_ptr<VulkanImage> m_vulkanTextureImage;
  vk::UniqueSampler m_textureSampler;
};
} // namespace VkHal

#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "VkHal/Vulkan/VulkanDevice.h"

namespace VkHal
{
struct VulkanCurrentFrameResources;

class DevGuiRenderer
{
public:
  DevGuiRenderer(vk::Instance* instance, VulkanDevice* device, uint32_t graphicsQueueFamily, vk::Queue& graphicsQueue);
  ~DevGuiRenderer();

  DevGuiRenderer(DevGuiRenderer&&) = default;
  DevGuiRenderer& operator=(DevGuiRenderer&&) = default;

  void prepare(HWND windowHandle, VulkanSwapchain* swapchain);
  void startFrame();
  void recordCommandBuffers(const VulkanCurrentFrameResources& currentFrameResources);

private:
  void createRenderPass(vk::Format surfaceFormat);
  void createFramebuffer(VulkanSwapchain* swapchain);
  void UploadFonts();
  void statsGui();

  vk::Instance* m_instance;
  VulkanDevice* m_device;
  uint32_t m_graphicsQueueFamily;
  vk::Queue& m_graphicsQueue;
  vk::UniqueDescriptorPool m_descriptorPool;
  vk::UniqueRenderPass m_renderPass;
  std::vector<vk::UniqueFramebuffer> m_framebuffers;
};
} // namespace VkHal

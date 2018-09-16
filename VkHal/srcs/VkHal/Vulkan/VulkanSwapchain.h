#pragma once

#include <vulkan/vulkan.hpp>

namespace VkHal
{
class VulkanSwapchain
{
public:
  VulkanSwapchain(vk::UniqueSwapchainKHR&& swapchain, std::vector<vk::UniqueImageView>&& imageViews, vk::Extent2D extent, vk::SurfaceFormatKHR format, vk::PresentModeKHR presentMode);
  ~VulkanSwapchain() = default;

  uint32_t getSwapchainImageCount() const
  {
    return (uint32_t)m_imageViews.size();
  }

  vk::Extent2D getSwapchainExtent() const
  {
    return m_extent;
  }

  vk::Format getFormat() const
  {
    return m_format.format;
  }

  const std::vector<vk::UniqueImageView>& getImageViews() const
  {
    return m_imageViews;
  }

  const vk::SwapchainKHR& getSwapchain() const
  {
    return m_swapchain.get();
  }

private:
  vk::UniqueSwapchainKHR m_swapchain;
  std::vector<vk::UniqueImageView> m_imageViews;
  vk::Extent2D m_extent;
  vk::SurfaceFormatKHR m_format;
  vk::PresentModeKHR m_presentMode;
};

} // namespace VkHal

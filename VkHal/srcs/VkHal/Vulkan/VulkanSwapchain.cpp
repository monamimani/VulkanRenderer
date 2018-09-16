#include "VulkanSwapchain.h"

namespace VkHal
{
VulkanSwapchain::VulkanSwapchain(vk::UniqueSwapchainKHR&& swapchain, std::vector<vk::UniqueImageView>&& imageViews, vk::Extent2D extent, vk::SurfaceFormatKHR format, vk::PresentModeKHR presentMode)
    : m_swapchain{std::move(swapchain)}
    , m_imageViews{std::move(imageViews)}
    , m_extent{extent}
    , m_format{format}
    , m_presentMode{presentMode}

{
}

} // namespace VkHal

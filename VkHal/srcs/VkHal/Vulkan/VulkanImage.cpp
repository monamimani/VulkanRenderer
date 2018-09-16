#include "VulkanImage.h"

namespace VkHal
{
VulkanImage::VulkanImage(vk::UniqueDeviceMemory&& imgMemory, vk::UniqueImage&& img, vk::UniqueImageView&& imgView, vk::Format format, uint32_t mipCount)
    : m_imageMemory{std::move(imgMemory)}
    , m_image{std::move(img)}
    , m_imageView{std::move(imgView)}
    , m_format{format}
    , m_mipCount{mipCount}
{
}
} // namespace VkHal

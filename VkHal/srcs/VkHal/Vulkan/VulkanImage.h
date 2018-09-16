#pragma once

#include <vulkan/vulkan.hpp>

namespace VkHal
{
class VulkanImage
{
public:
  VulkanImage(vk::UniqueDeviceMemory&& imgMemory, vk::UniqueImage&& img, vk::UniqueImageView&& imgView, vk::Format format, uint32_t mipCount);
  ~VulkanImage() = default;

  const vk::ImageView& getImageView() const
  {
    return m_imageView.get();
  }

  const vk::Image& getImage() const
  {
    return m_image.get();
  }

  vk::Format getFormat() const
  {
    return m_format;
  }

  uint32_t getMipCount()
  {
    return m_mipCount;
  }

private:
  vk::UniqueDeviceMemory m_imageMemory;
  vk::UniqueImage m_image;
  vk::UniqueImageView m_imageView;

  vk::Format m_format;
  uint32_t m_mipCount;
};
} // namespace VkHal

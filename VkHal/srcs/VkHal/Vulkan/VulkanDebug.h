#pragma once

#include <vulkan/vulkan.hpp>

#include "VkHal/Vulkan/VulkanUtils.h"

namespace VkHal
{
class VkDebugReport
{
public:
  static constexpr char m_debugExtensionName[] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

  VkDebugReport(const vk::Instance& instance);
  ~VkDebugReport();

private:
  const vk::Instance& m_instance;
  vk::DebugReportCallbackEXT m_debugReportCallback;
};

class DebugUtils
{
public:
  static constexpr char m_debugExtensionName[] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  static constexpr std::array<float, 4> m_black = {0.0f, 0.0f, 0.0f, 1.0f};
  static constexpr std::array<float, 4> m_white = {1.0f, 1.0f, 1.0f, 1.0f};
  static constexpr std::array<float, 4> m_gray = {0.5f, 0.5f, 0.5f, 1.0f};
  static constexpr std::array<float, 4> m_darkGray = {0.75f, 0.75f, 0.75f, 1.0f};
  static constexpr std::array<float, 4> m_lightGray = {0.25f, 0.25f, 0.25f, 1.0f};
  static constexpr std::array<float, 4> m_blue = {0.0f, 0.0f, 1.0f, 1.0f};
  static constexpr std::array<float, 4> m_green = {0.0f, 1.0f, 0.0f, 1.0f};
  static constexpr std::array<float, 4> m_red = {1.0f, 0.0f, 0.0f, 1.0f};
  static constexpr std::array<float, 4> m_cyan = {0.0f, 1.0f, 1.0f, 1.0f};
  static constexpr std::array<float, 4> m_magenta = {1.0f, 0.0f, 1.0f, 1.0f};
  static constexpr std::array<float, 4> m_yellow = {1.0f, 0.92f, 0.016f, 1.0f};

  DebugUtils(const vk::Instance& instance);
  ~DebugUtils();

  //template <typename VkHandle_t>
  //void setObjectName(const vk::Device& device, VkHandle_t objHandle, vk::ObjectType objType, const char* name);

  void insertLabel(const vk::CommandBuffer& cmdBuffer, const char* name, std::array<float, 4> bgColor = DebugUtils::m_white);
  void beginLabel(const vk::CommandBuffer& cmdBuffer, const char* name, std::array<float, 4> bgColor = DebugUtils::m_white);
  void endLabel(const vk::CommandBuffer& cmdBuffer);
  void insertLabel(const vk::Queue& queue, const char* name, std::array<float, 4> bgColor = DebugUtils::m_white);
  void beginLabel(const vk::Queue& queue, const char* name, std::array<float, 4> bgColor = DebugUtils::m_white);
  void endLabel(const vk::Queue& queue);

private:
  const vk::Instance& m_instance;
  vk::DebugUtilsMessengerEXT m_debugUtilsMessenger;

  PFN_vkSetDebugUtilsObjectNameEXT m_setObjectNameFct;

  PFN_vkCmdInsertDebugUtilsLabelEXT m_cmdInsertLabelFct;
  PFN_vkCmdBeginDebugUtilsLabelEXT m_cmdBeginLabelFct;
  PFN_vkCmdEndDebugUtilsLabelEXT m_cmdEndLabelFct;

  PFN_vkQueueInsertDebugUtilsLabelEXT m_queueInsertLabelFct;
  PFN_vkQueueBeginDebugUtilsLabelEXT m_queueBeginLabelFct;
  PFN_vkQueueEndDebugUtilsLabelEXT m_queueEndLabelFct;
};

//template <typename VkHandle_t>
//void DebugUtils::setObjectName(const vk::Device& device, VkHandle_t objHandle, vk::ObjectType objType, const char* name)
//{
//  vk::DebugUtilsObjectNameInfoEXT nameInfo{};
//  nameInfo.objectHandle = reinterpret_cast<uint64_t&>(objHandle);
//  nameInfo.objectType = objType;
//  nameInfo.pObjectName = name;
//
//  vkCheck(m_setObjectNameFct(device, reinterpret_cast<VkDebugUtilsObjectNameInfoEXT*>(&nameInfo)));
//}

} // namespace VkHal

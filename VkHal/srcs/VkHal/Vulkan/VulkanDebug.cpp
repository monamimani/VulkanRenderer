#include "VulkanDebug.h"

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

#include <cstdio>
#include <iostream>

namespace VkHal
{
static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
  auto flagsStr = vk::to_string(vk::DebugReportFlagsEXT(flags));
  auto objTypeStr = vk::to_string(vk::DebugReportObjectTypeEXT(objType));

  const auto size = std::snprintf(nullptr, 0, "VK_DEBUG_REPORT::%s: %s flags=%s, objType=%s, obj=%llu, location=%lld, code=%d\n", layerPrefix, msg, flagsStr.c_str(), objTypeStr.c_str(), obj, location, code);
  std::string output(size + 1, '\0');
  std::snprintf(output.data(), output.size(), "VK_DEBUG_REPORT::%s: %s flags=%s, objType=%s, obj=%llu, location=%lld, code=%d\n", layerPrefix, msg, flagsStr.c_str(), objTypeStr.c_str(), obj, location, code);
  OutputDebugStringA(output.c_str());
  std::cout << output << "\n";
  return VK_FALSE;
}

VkDebugReport::VkDebugReport(const vk::Instance& instance)
    : m_instance{instance}
{
  vk::DebugReportCallbackCreateInfoEXT callbackInfo{};
  callbackInfo.flags = vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::eError;
  callbackInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugReportCallback;

  PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");

  Check(func != nullptr, "Could not find vkCreateDebugReportCallbackEXT");
  vkCheck((func(instance, (VkDebugReportCallbackCreateInfoEXT*)&callbackInfo, nullptr, reinterpret_cast<VkDebugReportCallbackEXT*>(&m_debugReportCallback))));
  // m_debugReportCallback = instance.createDebugReportCallbackEXTUnique(callbackInfo, nullptr, dispatchLoaderDynamicInstance);
}

VkDebugReport::~VkDebugReport()
{
  PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
  Check(func != nullptr, "Could not find vkDestroyDebugReportCallbackEXT");
  func(m_instance, m_debugReportCallback, nullptr);
}

template <typename... Args_t>
std::string makeStringMsg(const char* formatStr, Args_t... args)
{
  const auto size = std::snprintf(nullptr, 0, formatStr, args...);
  std::string output(size + 1, '\0');
  std::snprintf(output.data(), output.size(), formatStr, args...);

  return output;
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{

  auto messageSeverityStr = vk::to_string(vk::DebugUtilsMessageSeverityFlagBitsEXT{messageSeverity});
  auto messageTypeStr = vk::to_string(vk::DebugUtilsMessageTypeFlagsEXT{messageType});
  auto callbackData = reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData);

  auto formatStr = "VK_DEBUG_UTILS(%s, %s) - MsgId=%d, MsgName=%s : \n%s\n";
  auto msgOutPut = makeStringMsg(formatStr, messageTypeStr.c_str(), messageSeverityStr.c_str(), callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);

  if (callbackData->objectCount > 0)
  {
    formatStr = "\tObjects - %d\n";
    msgOutPut += makeStringMsg(formatStr, callbackData->objectCount);

    formatStr = "\t\tObjects[%d] - objType=%s, obj=%llu, name=%s\n";
    for (uint32_t i = 0; i < callbackData->objectCount; i++)
    {
      auto object = callbackData->pObjects[i];
      auto objTypeStr = vk::to_string(object.objectType);
      msgOutPut += makeStringMsg(formatStr, i, objTypeStr.c_str(), object.objectHandle, object.pObjectName);
    }
  }

  if (callbackData->cmdBufLabelCount > 0)
  {
    formatStr = "\tCmdBuffer labels - %d\n";
    msgOutPut += makeStringMsg(formatStr, callbackData->cmdBufLabelCount);

    formatStr = "\t\tLabel[%d] - %s {%f, %f, %f}\n";
    for (uint32_t i = 0; i < callbackData->cmdBufLabelCount; i++)
    {
      auto cmdBufferLabel = callbackData->pCmdBufLabels[i];
      msgOutPut += makeStringMsg(formatStr, i, cmdBufferLabel.pLabelName, cmdBufferLabel.color[0], cmdBufferLabel.color[1], cmdBufferLabel.color[2], cmdBufferLabel.color[3]);
    }
  }

  //auto formatStr = "VK_DEBUG_UTILS(%s, %s) - MsgId=%d%s, MsgName=%s : \n%s\n";
  //const auto size = std::snprintf(nullptr, 0, formatStr, messageTypeStr.c_str(), messageSeverityStr.c_str(), callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
  //std::string output(size + 1, '\0');
  //std::snprintf(output.data(), output.size(), formatStr, messageTypeStr.c_str(), messageSeverityStr.c_str(), callbackData->pMessage, callbackData->pMessageIdName);

  OutputDebugStringA(msgOutPut.c_str());
  std::cout << msgOutPut << "\n";

  return VK_FALSE;
}

DebugUtils::DebugUtils(const vk::Instance& instance)
    : m_instance{instance}
{
  vk::DebugUtilsMessengerCreateInfoEXT debugUtilsInfo{};
  debugUtilsInfo.flags = vk::DebugUtilsMessengerCreateFlagBitsEXT{};
  debugUtilsInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning; // | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
  debugUtilsInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
  debugUtilsInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)debugUtilsMessengerCallback;

  PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
  Check(func != nullptr, "Could not find vkCreateDebugUtilsMessengerEXT");
  vkCheck(static_cast<vk::Result>(func(instance, (VkDebugUtilsMessengerCreateInfoEXT*)&debugUtilsInfo, nullptr, reinterpret_cast<VkDebugUtilsMessengerEXT*>(&m_debugUtilsMessenger))));
  //m_debugUtilsMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsInfo);

  m_setObjectNameFct = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_instance, "vkSetDebugUtilsObjectNameEXT");
  Check(m_setObjectNameFct != nullptr, "Could not find vkSetDebugUtilsObjectNameEXT");

  m_cmdInsertLabelFct = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkCmdInsertDebugUtilsLabelEXT");
  Check(m_cmdInsertLabelFct != nullptr, "Could not find vkCmdInsertDebugUtilsLabelEXT");
  m_cmdBeginLabelFct = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkCmdBeginDebugUtilsLabelEXT");
  Check(m_cmdBeginLabelFct != nullptr, "Could not find vkCmdBeginDebugUtilsLabelEXT");
  m_cmdEndLabelFct = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkCmdEndDebugUtilsLabelEXT");
  Check(m_cmdEndLabelFct != nullptr, "Could not find vkCmdEndDebugUtilsLabelEXT");

  m_queueInsertLabelFct = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkQueueInsertDebugUtilsLabelEXT");
  Check(m_queueInsertLabelFct != nullptr, "Could not find vkQueueInsertDebugUtilsLabelEXT");
  m_queueBeginLabelFct = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkQueueBeginDebugUtilsLabelEXT");
  Check(m_queueBeginLabelFct != nullptr, "Could not find vkQueueBeginDebugUtilsLabelEXT");
  m_queueEndLabelFct = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_instance, "vkQueueEndDebugUtilsLabelEXT");
  Check(m_queueEndLabelFct != nullptr, "Could not find vkQueueEndDebugUtilsLabelEXT");
}
DebugUtils::~DebugUtils()
{
  PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
  Check(func != nullptr, "Could not find vkDestroyDebugUtilsMessengerEXT");
  func(m_instance, m_debugUtilsMessenger, nullptr);
}

void DebugUtils::insertLabel(const vk::CommandBuffer& cmdBuffer, const char* name, std::array<float, 4> bgColor)
{
  auto labelInfo = vk::DebugUtilsLabelEXT{name, bgColor};
  m_cmdInsertLabelFct(cmdBuffer, reinterpret_cast<VkDebugUtilsLabelEXT*>(&labelInfo));
}

void DebugUtils::beginLabel(const vk::CommandBuffer& cmdBuffer, const char* name, std::array<float, 4> bgColor)
{
  auto labelInfo = vk::DebugUtilsLabelEXT{name, bgColor};
  m_cmdBeginLabelFct(cmdBuffer, reinterpret_cast<VkDebugUtilsLabelEXT*>(&labelInfo));
}

void DebugUtils::endLabel(const vk::CommandBuffer& cmdBuffer)
{
  m_cmdEndLabelFct(cmdBuffer);
}

void DebugUtils::insertLabel(const vk::Queue& queue, const char* name, std::array<float, 4> bgColor)
{
  auto labelInfo = vk::DebugUtilsLabelEXT{name, bgColor};
  m_queueInsertLabelFct(queue, reinterpret_cast<VkDebugUtilsLabelEXT*>(&labelInfo));
}

void DebugUtils::beginLabel(const vk::Queue& queue, const char* name, std::array<float, 4> bgColor)
{
  auto labelInfo = vk::DebugUtilsLabelEXT{name, bgColor};
  m_queueBeginLabelFct(queue, reinterpret_cast<VkDebugUtilsLabelEXT*>(&labelInfo));
}

void DebugUtils::endLabel(const vk::Queue& queue)
{
  m_queueEndLabelFct(queue);
}

} // namespace VkHal

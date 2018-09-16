#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

constexpr bool enableVulkanChecks = true;

namespace
{
auto Check(bool result, const char* msg)
{
  if (!result)
  {
    throw std::runtime_error(msg);
  }
}
} // namespace

namespace VkHal
{
static __forceinline void throwVkException(vk::Result result, char const* message)
{
  switch (result)
  {
    case vk::Result::eErrorOutOfHostMemory:
      throw vk::OutOfHostMemoryError(message);
    case vk::Result::eErrorOutOfDeviceMemory:
      throw vk::OutOfDeviceMemoryError(message);
    case vk::Result::eErrorInitializationFailed:
      throw vk::InitializationFailedError(message);
    case vk::Result::eErrorDeviceLost:
      throw vk::DeviceLostError(message);
    case vk::Result::eErrorMemoryMapFailed:
      throw vk::MemoryMapFailedError(message);
    case vk::Result::eErrorLayerNotPresent:
      throw vk::LayerNotPresentError(message);
    case vk::Result::eErrorExtensionNotPresent:
      throw vk::ExtensionNotPresentError(message);
    case vk::Result::eErrorFeatureNotPresent:
      throw vk::FeatureNotPresentError(message);
    case vk::Result::eErrorIncompatibleDriver:
      throw vk::IncompatibleDriverError(message);
    case vk::Result::eErrorTooManyObjects:
      throw vk::TooManyObjectsError(message);
    case vk::Result::eErrorFormatNotSupported:
      throw vk::FormatNotSupportedError(message);
    case vk::Result::eErrorFragmentedPool:
      throw vk::FragmentedPoolError(message);
    case vk::Result::eErrorOutOfPoolMemory:
      throw vk::OutOfPoolMemoryError(message);
    case vk::Result::eErrorInvalidExternalHandle:
      throw vk::InvalidExternalHandleError(message);
    case vk::Result::eErrorSurfaceLostKHR:
      throw vk::SurfaceLostKHRError(message);
    case vk::Result::eErrorNativeWindowInUseKHR:
      throw vk::NativeWindowInUseKHRError(message);
    case vk::Result::eErrorOutOfDateKHR:
      throw vk::OutOfDateKHRError(message);
    case vk::Result::eErrorIncompatibleDisplayKHR:
      throw vk::IncompatibleDisplayKHRError(message);
    case vk::Result::eErrorValidationFailedEXT:
      throw vk::ValidationFailedEXTError(message);
    case vk::Result::eErrorInvalidShaderNV:
      throw vk::InvalidShaderNVError(message);
    case vk::Result::eErrorFragmentationEXT:
      throw vk::FragmentationEXTError(message);
    case vk::Result::eErrorNotPermittedEXT:
      throw vk::NotPermittedEXTError(message);
    default:
      throw vk::SystemError(make_error_code(result));
  }
}

static auto vkCheck(vk::Result returnValue)
{

  if constexpr (enableVulkanChecks)
  {
    if (returnValue != vk::Result::eSuccess)
    {
      auto msg = "vkCheck Failed: " + vk::to_string(returnValue);

      // This works because the mssages are single byes.
      OutputDebugString(std::wstring(msg.begin(), msg.end()).c_str());

      throwVkException(returnValue, msg.c_str());

      return returnValue;
    }
  }

  return returnValue;
}

static auto vkCheck(VkResult returnValue)
{
  return vkCheck(static_cast<vk::Result>(returnValue));
}

static void printAvailablePhysicalDeviceProperties(vk::Instance vkInstance)
{
  uint32_t deviceCount = 0;

  auto physicalDevices = vkInstance.enumeratePhysicalDevices();

  if (physicalDevices.empty())
  {
    throw std::runtime_error("Failed to find GPUs with Vulkan support!");
  }

  for (const auto& physicalDevice : physicalDevices)
  {
    auto properties = physicalDevice.getProperties();
    {
      auto deviceTypeStr = vk::to_string(properties.deviceType);
      const auto size = std::snprintf(nullptr, 0, "Physical device: %s, driver version: %d, type: %s \n", properties.deviceName, properties.driverVersion, deviceTypeStr.c_str());
      std::string output(size + 1, '\0');
      std::snprintf(output.data(), output.size(), "Physical device: %s, driver version: %d, type: %s \n", properties.deviceName, properties.driverVersion, deviceTypeStr.c_str());
      std::cout << output.c_str();
    }
    {
      const auto size = std::snprintf(nullptr, 0, "Vulkan API version supported %d.%d.%d \n", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
      std::string output(size + 1, '\0');
      std::snprintf(output.data(), output.size(), "Vulkan API version supported %d.%d.%d \n", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
      std::cout << output.c_str();
    }
  }
  std::cout << std::endl;
}

static void verifyValidationLayersAvailability(std::vector<const char*> validationLayerNames)
{
  auto instanceLayers = vk::enumerateInstanceLayerProperties();

  for (const auto& instanceLayer : instanceLayers)
  {
    auto it = std::find_if(begin(validationLayerNames), end(validationLayerNames), [instanceLayer](auto layerName) { return strcmp(layerName, instanceLayer.layerName) == 0; });
    if (it != end(validationLayerNames))
    {
      validationLayerNames.erase(it);
    }

    if (validationLayerNames.empty())
    {
      break;
    }
  }

  if (!validationLayerNames.empty())
  {
    throw std::runtime_error("Validation layer not supported.");
  }
}

static void verifyInstanceExtensionAvailability(std::vector<const char*> extensionNames)
{

  auto instanceExtensions = vk::enumerateInstanceExtensionProperties();
  for (const auto& instanceExtension : instanceExtensions)
  {
    auto it = std::find_if(begin(extensionNames), end(extensionNames), [instanceExtension](auto extensionName) { return strcmp(extensionName, instanceExtension.extensionName) == 0; });
    if (it != end(extensionNames))
    {
      extensionNames.erase(it);
    }

    if (extensionNames.empty())
    {
      break;
    }
  }

  if (!extensionNames.empty())
  {
    throw std::runtime_error("Instance extension not supported.");
  }
}

static void verifyInstanceExtensionAvailability(std::set<std::string> extensionNames)
{
  auto instanceExtensions = vk::enumerateInstanceExtensionProperties();
  for (const auto& instanceExtension : instanceExtensions)
  {
    extensionNames.erase(instanceExtension.extensionName);
    if (extensionNames.empty())
    {
      break;
    }
  }

  if (!extensionNames.empty())
  {
    throw std::runtime_error("Instance extension not supported.");
  }
}

static bool hasStencilComponent(vk::Format format)
{
  return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD16UnormS8Uint;
}

static std::vector<char> readFile(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::ate | std::ios::binary);

  if (!file)
  {
    throw std::runtime_error("Failed to open file.");
  }

  auto fileSize = (size_t)file.tellg();
  std::vector<char> fileBuffer(fileSize);
  file.seekg(0);
  file.read(fileBuffer.data(), fileSize);

  file.close();
  return fileBuffer;
}
} // namespace VkHal

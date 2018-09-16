#include "DebugGui.h"

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <WinUser.h>

#include <imgui.h>

#include "VkHal/DebugGui/imgui/imgui_impl_vulkan.h"
#include "VkHal/DebugGui/imgui/imgui_impl_win32.h"

#include "VkHal/VkRenderer.h"
#include "VkHal/Vulkan/VulkanUtils.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace VkHal
{
void CheckVkresult(VkResult result)
{
  vkCheck(result);
}

WNDPROC originalProc{};

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  return CallWindowProc(originalProc, hWnd, msg, wParam, lParam);
}

DevGuiRenderer::DevGuiRenderer(vk::Instance* instance, VulkanDevice* device, uint32_t graphicsQueueFamily, vk::Queue& graphicsQueue)
    : m_instance{instance}
    , m_device{device}
    , m_graphicsQueueFamily{graphicsQueueFamily}
    , m_graphicsQueue{graphicsQueue}
{
}

DevGuiRenderer::~DevGuiRenderer()
{
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

void DevGuiRenderer::prepare(HWND windowHandle, VulkanSwapchain* swapchain)
{
  originalProc = (WNDPROC)SetWindowLongPtr(windowHandle, GWLP_WNDPROC, (int64_t)WndProc);

  ImGui::CreateContext();

  auto builder = m_device->getDescriptorPoolBuilder();

  builder.addDescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1);
  m_descriptorPool = builder.build(1);

  createRenderPass(swapchain->getFormat());
  createFramebuffer(swapchain);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = *m_instance;
  init_info.PhysicalDevice = m_device->getPhysicalDevice();
  init_info.Device = m_device->getDevice();
  init_info.QueueFamily = m_graphicsQueueFamily;
  init_info.Queue = m_graphicsQueue;
  init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = m_descriptorPool.get();
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = &CheckVkresult;
  ImGui_ImplVulkan_Init(&init_info, m_renderPass.get());

  ImGui_ImplWin32_Init(windowHandle);

  ImGui::StyleColorsDark();

  UploadFonts();
}

void DevGuiRenderer::startFrame()
{
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  statsGui();
}

void DevGuiRenderer::recordCommandBuffers(const VulkanCurrentFrameResources& currentFrameResources)
{
  ImGui::Render();

  auto& commandBuffer = currentFrameResources.m_frameResources->m_graphicsCmdBuffers[0];
  currentFrameResources.m_debugUtils->beginLabel(commandBuffer.get(), "DevGui");
  std::array<vk::ClearValue, 1> clearColor{};
  clearColor[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});

  vk::RenderPassBeginInfo renderPassInfo{};
  renderPassInfo.renderPass = *m_renderPass;
  renderPassInfo.framebuffer = m_framebuffers[currentFrameResources.m_swapchainImageIndex].get();
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = currentFrameResources.m_swapchain->getSwapchainExtent();
  renderPassInfo.clearValueCount = (uint32_t)clearColor.size();
  renderPassInfo.pClearValues = clearColor.data();

  commandBuffer->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());

  commandBuffer->endRenderPass();
  currentFrameResources.m_debugUtils->endLabel(commandBuffer.get());
}

void DevGuiRenderer::createRenderPass(vk::Format surfaceFormat)
{
  std::vector<vk::AttachmentDescription> attachmentsDesc;

  vk::AttachmentDescription attachment = {};
  attachment.format = surfaceFormat;
  attachment.samples = vk::SampleCountFlagBits::e1;

  attachment.loadOp = vk::AttachmentLoadOp::eLoad;
  attachment.storeOp = vk::AttachmentStoreOp::eStore;
  attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
  attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
  attachmentsDesc.push_back(attachment);

  vk::AttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription subpassDesc{};
  subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpassDesc.colorAttachmentCount = 1;
  subpassDesc.pColorAttachments = &colorAttachmentRef;

  vk::SubpassDependency subpassDependency{};
  subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependency.dstSubpass = 0;
  subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
  subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  vk::RenderPassCreateInfo renderPassCreateInfo{};
  renderPassCreateInfo.attachmentCount = (uint32_t)attachmentsDesc.size();
  renderPassCreateInfo.pAttachments = attachmentsDesc.data();
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDesc;
  renderPassCreateInfo.dependencyCount = 1;
  renderPassCreateInfo.pDependencies = &subpassDependency;

  m_renderPass = m_device->getDevice().createRenderPassUnique(renderPassCreateInfo);
}

void DevGuiRenderer::createFramebuffer(VulkanSwapchain* swapchain)
{
  auto extent = swapchain->getSwapchainExtent();
  const auto& imgViews = swapchain->getImageViews();
  m_framebuffers.resize(imgViews.size());

  for (int i = 0; i < imgViews.size(); i++)
  {
    m_framebuffers[i] = m_device->createFramebuffer(extent, m_renderPass.get(), {imgViews[i].get()});
  }
}

void DevGuiRenderer::UploadFonts()
{
  auto graphicsCmdPoolTmp = m_device->createCommandPool(m_graphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
  m_device->setObjectName(graphicsCmdPoolTmp.get(), vk::ObjectType::eCommandPool, "GfxCmdPoolTmp");
  auto graphicsCmdBuffersTmp = m_device->allocateCommandBuffer(*graphicsCmdPoolTmp, 1, true);
  m_device->setObjectName(graphicsCmdBuffersTmp[0].get(), vk::ObjectType::eCommandBuffer, "GfxCmdBufferTmp");

  auto& commandBuffer = graphicsCmdBuffersTmp[0];

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  commandBuffer->begin(beginInfo);
  ImGui_ImplVulkan_CreateFontsTexture(commandBuffer.get());
  commandBuffer->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer.get();

  m_graphicsQueue.submit(submitInfo, nullptr);
  m_graphicsQueue.waitIdle();
  ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

void DevGuiRenderer::statsGui()
{
  ImGuiIO& io = ImGui::GetIO();

  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 120.0f, 20.0f));
  ImGui::SetNextWindowSize(ImVec2(100.0f, 100.0));
  ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

  static bool show_fps = true;

  if (ImGui::RadioButton("FPS", show_fps))
  {
    show_fps = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("ms", !show_fps))
  {
    show_fps = false;
  }

  if (show_fps)
  {
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("%7.1f", io.Framerate);

    //auto & histogram = timer.GetFPSHistogram();
    //ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
  }
  else
  {
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("%9.3f", io.DeltaTime);

    //auto & histogram = timer.GetDeltaTimeHistogram();
    //ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
  }

  ImGui::End();
}

} // namespace VkHal

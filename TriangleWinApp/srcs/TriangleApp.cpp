#include "TriangleApp.h"

TriangleApp::TriangleApp(HINSTANCE windowInstance)
    : WindowApp(windowInstance, true, L"Triangle Win APP")
{
}

TriangleApp::~TriangleApp() {}

void TriangleApp::initialize(uint32_t windowWidth, uint32_t windowHeight)
{
  createWindow(windowWidth, windowHeight);

  constexpr bool isHeadless = false;
  constexpr bool enableValidation = true;
  m_gfxSystem = std::make_unique<VkHal::VkRenderer>(isHeadless, enableValidation, getApplicationName());
  m_gfxSystem->initialize(getHInstance(), getWindowHandle());
  m_gfxSystem->prepare(windowWidth, windowHeight);
}

void TriangleApp::update()
{
  m_gfxSystem->update();
}

void TriangleApp::render()
{
  m_gfxSystem->render();
}
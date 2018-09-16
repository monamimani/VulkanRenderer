#pragma once

#include <memory>

#include "AppCore/WindowApp.h"

#include "VkHal/VkRenderer.h"

class TriangleApp final : public AppCore::WindowApp
{
public:
  TriangleApp(HINSTANCE windowInstance);
  ~TriangleApp() override;

private:
  void initialize(uint32_t windowWidth, uint32_t windowHeight) final;
  void update() final;
  void render() final;

  std::unique_ptr<VkHal::VkRenderer> m_gfxSystem; // vkRenderer is not the gfxSytstem but when I implement it the gfx system should own the specific impl of the renderer
};

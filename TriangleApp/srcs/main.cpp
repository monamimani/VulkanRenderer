#include <system_error>

#include "VkHal/VkRenderer.h"

int main(int argc, char* argv[])
{
  constexpr bool isHeadless = false;
  constexpr bool enableValidation = true;
  VkHal::VkRenderer renderer(isHeadless, enableValidation, "Triangle App Console");
  // renderer.initialize(hInstance, windowHandle);
  uint32_t width = 1280;
  uint32_t height = 720;
  renderer.prepare(width, height);

  return EXIT_SUCCESS;
}

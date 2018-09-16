#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

#include <string>

#include "Utility/Timer.h"

namespace AppCore
{
class WindowApp
{
public:
  WindowApp(HINSTANCE windowInstance, bool createConsole, std::wstring applicationName);
  virtual ~WindowApp();

  virtual void initialize(uint32_t windowWidth, uint32_t windowHeight) {}

  void showErrorMessageBox(const wchar_t* msg);
  void showInfoMessageBox(const wchar_t* msg);

  int run();

  virtual void update() {}
  virtual void render() {}

protected:
  HINSTANCE getHInstance() const { return m_windowInstance; }

  HWND getWindowHandle() const { return m_window; }

  std::string getApplicationName() const
  {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, m_applicationName.data(), (int)m_applicationName.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, m_applicationName.data(), (int)m_applicationName.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
  }

  void createWindow(uint32_t windowWidth, uint32_t windowHeight);

private:
  friend LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  int64_t handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  HINSTANCE m_windowInstance = {};
  HWND m_window = {};
  WNDCLASSEX m_windowClassEx = {};

  std::wstring m_applicationName;

  uint32_t m_clientRectWidth = {};
  uint32_t m_clientRectHeight = {};
  uint32_t m_windowPosX = 64;
  uint32_t m_windowPosY = 64;

  StepTimer m_timer;
};
} // namespace AppCore

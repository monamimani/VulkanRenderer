
#include "WindowApp.h"

#include <iostream>

namespace AppCore
{
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WindowApp* windowApp = nullptr;

WindowApp::WindowApp(HINSTANCE windowInstance, bool createConsole, std::wstring applicationName)
    : m_windowInstance(windowInstance)
    , m_window(nullptr)
    , m_applicationName(applicationName)
    , m_clientRectWidth(1)
    , m_clientRectHeight(1)
{
  if (createConsole)
  {
    // AllocConsole();
    // AttachConsole(GetCurrentProcessId());
    // FILE* stream;
    // freopen_s(&stream, "CON", "w", stdout);
    // assert(stream == stdout);
    // freopen_s(&stream, "CON", "w", stderr);
    // assert(stream == stderr);

    // SetConsoleTitle(TEXT(m_applicationName.c_str()));
  }

  m_timer.SetFixedTimeStep(true);
  m_timer.SetTargetDeltaTimeInSeconds(std::chrono::seconds(1) / 60.0);

  windowApp = this;
}

WindowApp::~WindowApp()
{
  UnregisterClass(m_applicationName.c_str(), m_windowInstance);
}

void WindowApp::createWindow(uint32_t windowWidth_, uint32_t windowHeight_)
{
  // Setup m_window class attributes.
  auto intResource = IDI_APPLICATION;
  m_windowClassEx.cbSize = sizeof(WNDCLASSEX);
  m_windowClassEx.style = CS_HREDRAW | CS_VREDRAW;
  m_windowClassEx.lpfnWndProc = WndProc;
  m_windowClassEx.cbClsExtra = 0;
  m_windowClassEx.cbWndExtra = 0;
  m_windowClassEx.hInstance = m_windowInstance;
  m_windowClassEx.hIcon = LoadIcon(m_windowInstance, intResource);
  m_windowClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
  m_windowClassEx.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  m_windowClassEx.lpszMenuName = NULL;
  m_windowClassEx.lpszClassName = m_applicationName.c_str();
  m_windowClassEx.hIconSm = LoadIcon(m_windowClassEx.hInstance, intResource);

  if (!RegisterClassEx(&m_windowClassEx))
  {
    throw(std::runtime_error("Failed to register the window class."));
  }

  DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
  DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  RECT clientRectAdjusted{0, 0, (int32_t)windowWidth_, (int32_t)windowHeight_};
  ::AdjustWindowRectEx(&clientRectAdjusted, dwStyle, false, dwExStyle);

  // Setup m_window initialization attributes.
  CREATESTRUCT cs;
  ZeroMemory(&cs, sizeof(cs));

  cs.x = m_windowPosX;                                        // Window X position
  cs.y = m_windowPosY;                                        // Window Y position
  cs.cx = clientRectAdjusted.right - clientRectAdjusted.left; // Window width
  cs.cy = clientRectAdjusted.bottom - clientRectAdjusted.top; // Window height
  cs.hInstance = m_windowInstance;                            // Window instance.
  cs.lpszClass = m_windowClassEx.lpszClassName;               // Window class name
  cs.lpszName = m_applicationName.c_str();                    // Window title
  cs.style = dwStyle;                                         // Window style
  cs.dwExStyle = dwExStyle;                                   // Window extended style

  // Create the m_window.
  m_window = ::CreateWindowEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);

  if (!m_window)
  {
    throw(std::runtime_error("Failed to create the window."));
  }

  m_clientRectWidth = windowWidth_;
  m_clientRectHeight = windowHeight_;

  ::ShowWindow(m_window, SW_SHOW);
  ::SetForegroundWindow(m_window);
  ::UpdateWindow(m_window);
  ::SetFocus(m_window);
}

int64_t WindowApp::handleMessages(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_CLOSE:
      DestroyWindow(hWnd);
      break;
    case WM_DESTROY:
      PostQuitMessage(EXIT_SUCCESS);
      return EXIT_SUCCESS;
      break;
      //case WM_PAINT:
      //  ValidateRect(hWnd, NULL);
      //  break;
    case WM_SIZE:
      // TODO: Handle Resize
      break;
    default:
      break;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

void WindowApp::showErrorMessageBox(const wchar_t* msg)
{
  MessageBox(nullptr, msg, m_applicationName.c_str(), MB_ICONERROR);
}

void WindowApp::showInfoMessageBox(const wchar_t* msg)
{
  MessageBox(nullptr, msg, m_applicationName.c_str(), MB_ICONINFORMATION);
}

int WindowApp::run()
{
  MSG systemMsg{};
  try
  {
    while (true)
    {
      while (::PeekMessage(&systemMsg, nullptr, 0, 0, PM_REMOVE))
      {
        // Handle messages
        ::TranslateMessage(&systemMsg);
        ::DispatchMessage(&systemMsg);
      }

      if (systemMsg.message == WM_QUIT)
      {
        break;
      }

      // Application code
      m_timer.Tick([this]() {
        // std::cout << "Timer stats" << std::endl;
        // std::cout << "GetDeltaTimeInSeconds " << m_timer.GetDeltaTimeInSeconds() << std::endl;
        // std::cout << "GetFrameCount " << m_timer.GetFrameCount() << std::endl;
        // std::cout << "GetFramesPerSecond " << m_timer.GetFramesPerSecond() << std::endl;
        // std::cout << "GetTotalSeconds " << m_timer.GetTotalSeconds() << std::endl;
        // std::cout << std::endl;

        update();
      });
      render();
    }
  }
  catch (const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return (int)systemMsg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (windowApp)
  {
    return windowApp->handleMessages(hWnd, msg, wParam, lParam);
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}
} // namespace AppCore

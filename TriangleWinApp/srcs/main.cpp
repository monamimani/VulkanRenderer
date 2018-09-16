// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

#include <fcntl.h>
#include <filesystem>
#include <io.h>

#include <iostream>

#include "TriangleApp.h"

class Console
{
public:
  Console(bool bindStdIn, bool bindStdOut, bool bindStdErr);
  ~Console() = default;
};

Console::Console(bool bindStdIn, bool bindStdOut, bool bindStdErr)
{
  ::AllocConsole();

  // Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
  // observed that the file number of our standard handle file objects can be assigned internally to a value of -2
  // when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
  // call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
  // before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
  // use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
  // using the "_dup2" function.
  if (bindStdIn)
  {
    FILE* dummyFile;
    freopen_s(&dummyFile, "nul", "r", stdin);
  }
  if (bindStdOut)
  {
    FILE* dummyFile;
    freopen_s(&dummyFile, "nul", "w", stdout);
  }
  if (bindStdErr)
  {
    FILE* dummyFile;
    freopen_s(&dummyFile, "nul", "w", stderr);
  }

  // Redirect unbuffered stdin from the current standard input handle
  if (bindStdIn)
  {
    HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
      int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
      if (fileDescriptor != -1)
      {
        FILE* file = _fdopen(fileDescriptor, "r");
        if (file != nullptr)
        {
          int dup2Result = _dup2(_fileno(file), _fileno(stdin));
          if (dup2Result == 0)
          {
            setvbuf(stdin, nullptr, _IONBF, 0);
          }
        }
      }
    }
  }

  // Redirect unbuffered stdout to the current standard output handle
  if (bindStdOut)
  {
    HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
      int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
      if (fileDescriptor != -1)
      {
        FILE* file = _fdopen(fileDescriptor, "w");
        if (file != nullptr)
        {
          int dup2Result = _dup2(_fileno(file), _fileno(stdout));
          if (dup2Result == 0)
          {
            setvbuf(stdout, nullptr, _IONBF, 0);
          }
        }
      }
    }
  }

  // Redirect unbuffered stderr to the current standard error handle
  if (bindStdErr)
  {
    HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
      int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
      if (fileDescriptor != -1)
      {
        FILE* file = _fdopen(fileDescriptor, "w");
        if (file != nullptr)
        {
          int dup2Result = _dup2(_fileno(file), _fileno(stderr));
          if (dup2Result == 0)
          {
            setvbuf(stderr, nullptr, _IONBF, 0);
          }
        }
      }
    }
  }

  // Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
  // standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
  // versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
  // has been read from or written to the targets or not.
  if (bindStdIn)
  {
    std::wcin.clear();
    std::cin.clear();
  }
  if (bindStdOut)
  {
    std::wcout.clear();
    std::cout.clear();
  }
  if (bindStdErr)
  {
    std::wcerr.clear();
    std::cerr.clear();
  }
}

Console console{true, true, true};

std::unique_ptr<AppCore::WindowApp> app;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  auto returnCode = EXIT_SUCCESS;
  try
  {

    app = std::make_unique<TriangleApp>(hInstance);

    uint32_t width = 1280;
    uint32_t height = 720;
    app->initialize(width, height); // those number represent the widht and height eventually they'll need to be provided from the command line or config file.
    returnCode = app->run();

    // app->showInfoMessageBox("Bye");
  }
  catch (const std::exception& e)
  {
    returnCode = EXIT_FAILURE;
    std::cout << e.what() << std::endl;
  }

  app.reset();
  return returnCode;
}

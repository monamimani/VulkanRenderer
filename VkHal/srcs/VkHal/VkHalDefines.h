#pragma once

#ifdef VKHAL_EXPORTS
#define VKHAL_API _declspec(dllexport)
#else
#define VKHAL_API _declspec(dllimport)
#endif

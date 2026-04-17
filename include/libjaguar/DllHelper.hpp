#pragma once

#ifdef _WIN32
#ifdef LJBUILD
#define LJAPI __declspec(dllexport)
#else
#define LJAPI __declspec(dllimport)
#endif
#else
#define LJAPI
#endif
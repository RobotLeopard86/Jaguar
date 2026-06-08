#pragma once

#if defined(_WIN32) && !defined(LJSTATIC)
#ifdef LJBUILD
#define LJAPI __declspec(dllexport)
#else
#define LJAPI __declspec(dllimport)
#endif
#else
#define LJAPI
#endif
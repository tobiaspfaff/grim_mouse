#if !defined(INCLUDED_FROM_BASE_VERSION_CPP) && !defined(RC_INVOKED)
#error This file may only be included by base/version.cpp or dists/residualvm.rc
#endif

// Reads revision number from file
// (this is used when building with Visual Studio)
#ifdef SCUMMVM_INTERNAL_REVISION
#include "internal_revision.h"
#endif

#ifdef RELEASE_BUILD
#undef SCUMMVM_REVISION
#endif

#ifndef SCUMMVM_REVISION
#define SCUMMVM_REVISION
#endif

#define SCUMMVM_VERSION "_Grim_Mouse_0.8_git" SCUMMVM_REVISION

#define SCUMMVM_VER_MAJOR 0
#define SCUMMVM_VER_MINOR 4
#define SCUMMVM_VER_PATCH 0

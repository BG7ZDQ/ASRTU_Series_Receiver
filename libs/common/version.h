#pragma once

// ASRTU_VERSION is injected by CMake from the project VERSION
// (see CMakeLists.txt: project(ASRTU1Qt VERSION ...)).  The fallback keeps
// translation-unit-only builds working when the macro is not defined.
#ifndef ASRTU_VERSION
#define ASRTU_VERSION "unknown"
#endif

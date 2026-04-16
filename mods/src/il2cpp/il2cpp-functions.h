/**
 * @file il2cpp-functions.h
 * @brief Foreign-function declarations for the IL2CPP runtime API.
 *
 * Uses an X-macro pattern with il2cpp-api-functions.h to declare every IL2CPP
 * C API function. On Windows the symbols are imported directly from
 * GameAssembly.lib at link time. On macOS, function-pointer typedefs and
 * extern declarations are emitted instead, to be resolved at runtime by
 * init_il2cpp_pointers().
 */
#pragma once

#ifdef _WIN32
#pragma comment(lib, "GameAssembly.lib")
#endif

#include <il2cpp-api-types.h>
#include <il2cpp-class-internals.h>
#include <il2cpp-config.h>
#include <il2cpp-object-internals.h>

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus
// On Windows: DO_API expands to __declspec(dllimport) declarations (via IL2CPP_IMPORT).
// On macOS:   DO_API expands to a function-pointer typedef + extern variable,
//             populated later by init_il2cpp_pointers().
#if _WIN32
#define DO_API(r, n, p) IL2CPP_IMPORT r n p;
#define DO_API_NO_RETURN(r, n, p) IL2CPP_IMPORT NORETURN r n p;
#else
#define DO_API(r, n, p)                                                                                                \
  using n##_t = r(*) p;                                                                                                \
  extern n##_t n;
#define DO_API_NO_RETURN(r, n, p) DO_API(r, n, p)
#endif
#include "il2cpp-api-functions.h"
#undef DO_API
#undef DO_API_NORETURN
#if defined(__cplusplus)
}
#endif // __cplusplus

/**
 * @brief Resolve IL2CPP API function pointers at runtime (macOS only).
 *
 * On Windows this is a no-op because symbols are resolved at link time via
 * GameAssembly.lib. On macOS it dlopen()s GameAssembly.dylib and fills every
 * function pointer declared above via dlsym().
 */
void init_il2cpp_pointers();

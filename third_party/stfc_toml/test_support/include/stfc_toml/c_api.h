#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(STFC_TOML_BUILD)
#define STFC_TOML_API __declspec(dllexport)
#else
#define STFC_TOML_API __declspec(dllimport)
#endif
#define STFC_TOML_CALL __cdecl
#else
#define STFC_TOML_API __attribute__((visibility("default")))
#define STFC_TOML_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
STFC_TOML_API uint32_t STFC_TOML_CALL stfc_toml_abi_version(void);
// Zero means an allocated structured response, including operation failures.
// Lengths exclude the optional terminating NUL. Free using this module only.
STFC_TOML_API int STFC_TOML_CALL  stfc_toml_execute(const char* request, size_t request_length, char** response_out,
                                                    size_t* response_length_out);
STFC_TOML_API void STFC_TOML_CALL stfc_toml_free(void* response);
#ifdef __cplusplus
}
#endif

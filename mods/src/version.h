/**
 * @file version.h
 * @brief Compile-time version constants and PE resource macros.
 *
 * VERSION_MAJOR/MINOR/REVISION/PATCH are substituted by the build system
 * (see docs/VERSION_SUBSTITUTION.md).  VERSION_PATCH > 0 marks a beta build.
 */
#pragma once

#if defined(STFC_RUNTIME_IDENTITY_GENERATED)
#include <runtime_identity.generated.h>
#endif

// clang-format off
#define VERSION_MAJOR               2
#define VERSION_MINOR               1
#define VERSION_REVISION            0
#define VERSION_PATCH               0

#define STRINGIFY_(s)               #s
#define STRINGIFY(s)                STRINGIFY_(s)

#ifndef STFC_DISTRIBUTION_ID
#define STFC_DISTRIBUTION_ID         "guffawaffle.stfc-community-mod"
#endif
#ifndef STFC_MOD_DISPLAY_NAME
#define STFC_MOD_DISPLAY_NAME        "Guffawaffle STFC Mod"
#endif
#ifndef STFC_UNOFFICIAL_LABEL
#define STFC_UNOFFICIAL_LABEL        "Unofficial downstream build"
#endif
#ifndef STFC_BUILD_CLASS
#define STFC_BUILD_CLASS             "local"
#endif
#ifndef STFC_BUILD_CLASS_LABEL
#define STFC_BUILD_CLASS_LABEL       "Local build"
#endif
#ifndef STFC_SOURCE_STATE_ID
#define STFC_SOURCE_STATE_ID         "unavailable"
#endif
#ifndef STFC_BASE_COMMIT
#define STFC_BASE_COMMIT             "not-recorded"
#endif
#ifndef STFC_UPSTREAM_BASE
#define STFC_UPSTREAM_BASE           "netniV/stfc-mod@v1.1.4#d912611fa1eca49fc54f363bdf8377dfebf8def0"
#endif
#ifndef STFC_TEST_TARGET
#define STFC_TEST_TARGET             ""
#endif
#ifndef STFC_TEST_EXPIRY
#define STFC_TEST_EXPIRY             ""
#endif
#ifndef STFC_SUPPORT_BOUNDARY
#define STFC_SUPPORT_BOUNDARY        ""
#endif
#ifndef STFC_SOURCE_REPRODUCIBLE
#define STFC_SOURCE_REPRODUCIBLE     0
#endif
#ifndef STFC_SOURCE_REPRODUCIBLE_STR
#define STFC_SOURCE_REPRODUCIBLE_STR "false"
#endif

#if VERSION_PATCH
	#define VERSION_PATCH_STR " (Beta)"
#else
	#define VERSION_PATCH_STR ""
#endif

#define VER_FILE_DESCRIPTION_STR    STFC_MOD_DISPLAY_NAME " - " STFC_UNOFFICIAL_LABEL VERSION_PATCH_STR

#define VER_FILE_VERSION            VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_PATCH
#define VER_FILE_VERSION_STR        STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_REVISION) "." STRINGIFY(VERSION_PATCH)
#define VER_FORK_LOCAL_VERSION_STR  VER_FILE_VERSION_STR "-guffa.local"

#ifndef STFC_RELEASE_TAG
#define STFC_RELEASE_TAG            VER_FORK_LOCAL_VERSION_STR
#endif

#define VER_RUNTIME_VERSION_STR     STFC_RELEASE_TAG

#define VER_PRODUCTNAME_STR         STFC_MOD_DISPLAY_NAME
#define VER_PRODUCT_VERSION         VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION
#define VER_PRODUCT_VERSION_STR     VER_RUNTIME_VERSION_STR
#define VER_SUPPORT_IDENTITY_STR    STFC_MOD_DISPLAY_NAME " " VER_RUNTIME_VERSION_STR " | " STFC_UNOFFICIAL_LABEL \
                                    " | class=" STFC_BUILD_CLASS " | distribution=" STFC_DISTRIBUTION_ID       \
                                    " | source=" STFC_SOURCE_STATE_ID " | base=" STFC_BASE_COMMIT              \
                                    " | upstream=" STFC_UPSTREAM_BASE " | reproducible="                     \
                                    STFC_SOURCE_REPRODUCIBLE_STR " | target=" STFC_TEST_TARGET                \
                                    " | expires=" STFC_TEST_EXPIRY " | support=" STFC_SUPPORT_BOUNDARY
#define VER_ORIGINAL_FILENAME_STR   "guffawaffle-stfc-mod.dll"
#define VER_INTERNAL_NAME_STR       VER_ORIGINAL_FILENAME_STR
#define VER_COPYRIGHT_STR           "Copyright (C) 2026"

#ifdef DEBUG
#define VER_FILEFLAGS               VS_FF_DEBUG
#else
#define VER_FILEFLAGS               0
#endif

#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO 1
#endif
// clang-format on

/**
 * @file errormsg.h
 * @brief Centralised error-logging helpers for common failure patterns.
 *
 * Provides short, consistent spdlog::error() wrappers for missing IL2CPP
 * methods/helpers, sync transport errors, and WinRT HRESULT failures.
 */
#pragma once

#include "str_utils.h"

#include <spdlog/spdlog.h>

#if _WIN32
#include <winrt/Windows.Foundation.h>
#endif

/**
 * @brief Structured error-logging helpers for recurring failure patterns.
 *
 * Each function logs via spdlog::error() with a consistent format so grep/log
 * analysis can easily filter by category (method lookup, sync transport, etc.).
 */
namespace ErrorMsg
{
/** @brief Log a missing instance method (classname->methodname). */
static auto MissingMethod(const char* classname, const char* methodname)
{
  spdlog::error("Unable to find method '{}->{}'", classname, methodname);
}

/** @brief Log a missing static method (classname::methodname). */
static void MissingStaticMethod(const char* classname, const char* methodname)
{
  spdlog::error("Unable to find method '{}::{}'", classname, methodname);
}

/** @brief Log a missing IL2CPP helper class (namespace.classname). */
static void MissingHelper(const char* namespacename, const char* classname)
{
  spdlog::error("Unable to find helper '{}.{}'", namespacename, classname);
}

static void SyncMsg(const char* section, const std::string& msg)
{
  spdlog::error("Failed to send {} sync data: {}", section, msg);
}

static void SyncMsg(const char* section, const std::wstring& msg)
{
  spdlog::error("Failed to send {} sync data: {}", section, to_string(msg));
}

static void SyncRuntime(const char* section, const std::runtime_error& e)
{
  spdlog::error("Runtime error sending {} sync data: {}", section, e.what());
}

static void SyncException(const char* section, const std::exception& e)
{
  spdlog::error("Exception sending {} sync data: {}", section, e.what());
}

#if _WIN32
static void SyncWinRT(const char* section, winrt::hresult_error const& ex)
{
  spdlog::error("WINRT Error sending {} sync data: {}", section, winrt::to_string(ex.message()));
}
#endif
};

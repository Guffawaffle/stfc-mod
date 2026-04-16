/**
 * @file folder_manager.h
 * @brief macOS NSSearchPathForDirectoriesInDomains bridge for C++.
 *
 * Wraps the Objective-C Foundation API so C++ code can resolve standard
 * macOS directories (Library, Caches, Application Support, etc.) without
 * importing Foundation headers directly.
 */
#pragma once

namespace fm
{
/// NSSearchPathDirectory constants mirroring Apple's Foundation enum.
/// Only the subset actually used by the mod is meaningful; the rest
/// are included for completeness if future code needs them.
enum {
  NSApplicationDirectory = 1,
  NSDemoApplicationDirectory,
  NSDeveloperApplicationDirectory,
  NSAdminApplicationDirectory,
  NSLibraryDirectory,
  NSDeveloperDirectory,
  NSUserDirectory,
  NSDocumentationDirectory,
  NSDocumentDirectory,
  NSCoreServiceDirectory,
  NSAutosavedInformationDirectory = 11,
  NSDesktopDirectory              = 12,
  NSCachesDirectory               = 13,
  NSApplicationSupportDirectory   = 14,
  NSDownloadsDirectory            = 15,
  NSInputMethodsDirectory         = 16,
  NSMoviesDirectory               = 17,
  NSMusicDirectory                = 18,
  NSPicturesDirectory             = 19,
  NSPrinterDescriptionDirectory   = 20,
  NSSharedPublicDirectory         = 21,
  NSPreferencePanesDirectory      = 22,
  NSApplicationScriptsDirectory   = 23,
  NSItemReplacementDirectory      = 99,
  NSAllApplicationsDirectory      = 100,
  NSAllLibrariesDirectory         = 101,
  NSTrashDirectory                = 102
};
typedef unsigned long SearchPathDirectory;

/// NSSearchPathDomainMask constants.
enum {
  NSUserDomainMask = 1, // user's home directory --- place to install user's personal items (~)
  NSLocalDomainMask =
      2, // local to the current machine --- place to install items available to everyone on this machine (/Library)
  NSNetworkDomainMask = 4,     // publically available location in the local area network --- place to install items
                               // available on the network (/Network)
  NSSystemDomainMask = 8,      // provided by Apple, unmodifiable (/System)
  NSAllDomainsMask   = 0x0ffff // all domains: all of the above and future items
};
typedef unsigned long SearchPathDomainMask;

/**
 * @brief C++ bridge to NSSearchPathForDirectoriesInDomains.
 *
 * Implementation is in Objective-C++ (folder_manager.mm).  Returns
 * C strings owned by a static cache — valid for the process lifetime.
 */
class FolderManager
{
public:
  /**
   * @brief Resolve a standard macOS directory.
   * @param directory  NSSearchPathDirectory constant (e.g. NSLibraryDirectory).
   * @param domainMask Domain mask (typically NSUserDomainMask).
   * @return Null-terminated path string, valid for the process lifetime.
   */
  static const char *pathForDirectory(SearchPathDirectory directory, SearchPathDomainMask domainMask);
  static const char *pathForDirectoryAppropriateForItemAtPath(SearchPathDirectory  directory,
                                                              SearchPathDomainMask domainMask, const char *itemPath,
                                                              bool create = false);
};
}; // namespace fm

// The ordinary mod files exposed by the launcher's file actions. Keep this path aligned with File's macOS
// preferences directory; opening these files does not select a profile or create a configuration.

import Foundation

enum ModFiles {
  static var directoryURL: URL? {
    FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first?
      .appendingPathComponent("Preferences", isDirectory: true)
      .appendingPathComponent("com.stfcmod.startrekpatch", isDirectory: true)
  }

  static var configURL: URL? {
    directoryURL?.appendingPathComponent("community_patch_settings.toml")
  }
}

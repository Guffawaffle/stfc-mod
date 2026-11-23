// Config and folder locations exposed by the launcher's file actions.

import Foundation

struct ModFiles {
  let directoryURL: URL
  let configURL: URL?

  // Keep ordinary play aligned with File's macOS preferences directory.
  static var ordinary: ModFiles? {
    guard let library = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first else {
      return nil
    }
    let directory = library.appendingPathComponent("Preferences", isDirectory: true)
      .appendingPathComponent("com.stfcmod.startrekpatch", isDirectory: true)
    return ModFiles(
      directoryURL: directory,
      configURL: directory.appendingPathComponent("community_patch_settings.toml", isDirectory: false))
  }
}

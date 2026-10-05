import Foundation

enum ModFiles {
  // Keep this path aligned with File::MakePath in mods/src/file.cc.
  static var directoryURL: URL? {
    FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first?
      .appendingPathComponent("Preferences", isDirectory: true)
      .appendingPathComponent("com.stfcmod.startrekpatch", isDirectory: true)
  }

  static var configURL: URL? {
    directoryURL?.appendingPathComponent("community_patch_settings.toml")
  }
}

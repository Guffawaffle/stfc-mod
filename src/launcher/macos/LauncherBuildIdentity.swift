// The bundle supplies the exact player-facing version and channel selected before signing.
import Foundation

enum LauncherBuildIdentity {
  static let displayVersion: String = {
    if let value = Bundle.main.object(forInfoDictionaryKey: "STFCDisplayVersion") as? String,
      !value.isEmpty
    {
      return value
    }
    if let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String,
      !version.isEmpty
    {
      return version
    }
    return "Unknown version"
  }()

  static func gameEnvironment() -> [String: String] {
    var environment = ProcessInfo.processInfo.environment
    environment.removeValue(forKey: "STFC_MOD_DISPLAY_VERSION")
    environment.removeValue(forKey: "STFC_MOD_DISPLAY_SOURCE")
    // Pass only this bundle's identity to its own loader process. The mod checks
    // the source against its compiled version hash before using the label.
    if let version = Bundle.main.object(forInfoDictionaryKey: "STFCDisplayVersion") as? String,
      let source = Bundle.main.object(forInfoDictionaryKey: "STFCSourceCommit") as? String,
      !version.isEmpty, !source.isEmpty
    {
      environment["STFC_MOD_DISPLAY_VERSION"] = version
      environment["STFC_MOD_DISPLAY_SOURCE"] = source
    }
    return environment
  }
}

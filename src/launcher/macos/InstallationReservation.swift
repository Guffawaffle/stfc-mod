// Launchers hold shared access through child exit; writers hold exclusive access through every mutation.

#if PROFILES_PREVIEW
import Foundation
import STFCProfiles

final class InstallationReservation {
  private var lease: UnsafeMutableRawPointer?

  private init(lease: UnsafeMutableRawPointer) { self.lease = lease }

  static func acquire(gameDirectory: URL, forUpdate: Bool) throws -> InstallationReservation {
    var lease: UnsafeMutableRawPointer?
    var error: UnsafeMutablePointer<CChar>?
    let result = gameDirectory.path.withCString {
      if forUpdate {
        return stfc_profiles_acquire_installation_update_lease_v1(nil, $0, &lease, &error)
      }
      return stfc_profiles_acquire_installation_lease_v1(nil, $0, &lease, &error)
    }
    defer { if let error { stfc_profiles_free_v1(error) } }
    guard result == 0, let lease else {
      throw NSError(
        domain: "STFCProfilesPreview", code: Int(result),
        userInfo: [NSLocalizedDescriptionKey: error.map { String(cString: $0) }
          ?? "This game installation is busy. Wait for its current operation to finish."])
    }
    return InstallationReservation(lease: lease)
  }

  func release() {
    if let lease { stfc_profiles_release_installation_lease_v1(lease) }
    lease = nil
  }

  deinit { release() }
}
#endif

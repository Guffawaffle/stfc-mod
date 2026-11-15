// The updater holds exclusive access in its own process throughout every installation mutation.

#if PROFILES_PREVIEW
import Foundation
import STFCProfiles

final class InstallationUpdateReservation {
  private var lease: UnsafeMutableRawPointer?

  private init(lease: UnsafeMutableRawPointer) { self.lease = lease }

  static func acquire(gameDirectory: URL) throws -> InstallationUpdateReservation {
    var lease: UnsafeMutableRawPointer?
    var error: UnsafeMutablePointer<CChar>?
    let result = gameDirectory.path.withCString {
      stfc_profiles_acquire_installation_update_lease_v1(nil, $0, &lease, &error)
    }
    defer { if let error { stfc_profiles_free_v1(error) } }
    guard result == 0, let lease else {
      throw NSError(
        domain: "STFCProfilesPreview", code: Int(result),
        userInfo: [NSLocalizedDescriptionKey: error.map { String(cString: $0) }
          ?? "Quit games using this installation before updating it."])
    }
    return InstallationUpdateReservation(lease: lease)
  }

  func release() {
    if let lease { stfc_profiles_release_installation_lease_v1(lease) }
    lease = nil
  }

  deinit { release() }
}
#endif

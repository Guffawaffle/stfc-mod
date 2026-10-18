import Foundation
import STFCProfiles

// The actual updater process holds exclusive installation access until every
// mutation finishes. Process termination also ends its writes and releases access.
final class InstallationUpdateReservation {
  private var lease: UnsafeMutableRawPointer?

  private init(lease: UnsafeMutableRawPointer) { self.lease = lease }

  static func acquire(gameExecutable: String) async throws -> InstallationUpdateReservation {
    let directory = URL(fileURLWithPath: gameExecutable).deletingLastPathComponent().path
    var lease: UnsafeMutableRawPointer?
    var error: UnsafeMutablePointer<CChar>?
    let result = directory.withCString {
      stfc_profiles_acquire_installation_update_lease_v1(nil, $0, &lease, &error)
    }
    defer { if let error { stfc_profiles_free_v1(error) } }
    guard result == 0, let lease else {
      throw NSError(domain: "STFCProfilesPreview", code: Int(result),
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

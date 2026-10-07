import Foundation

// The CLI holds the same exclusive installation lease used by game runtimes.
// Closing stdin releases it, including when the launcher process terminates.
final class InstallationUpdateReservation: @unchecked Sendable {
  private let process: Process
  private let input: Pipe

  private init(process: Process, input: Pipe) {
    self.process = process
    self.input = input
  }

  static func acquire(gameExecutable: String) async throws -> InstallationUpdateReservation {
    try await Task.detached { try start(gameExecutable: gameExecutable) }.value
  }

  private static func start(gameExecutable: String) throws -> InstallationUpdateReservation {
    let helper = Bundle.main.bundleURL.appendingPathComponent("Contents/stfc-profiles")
    let process = Process(), input = Pipe(), output = Pipe()
    process.executableURL = helper
    process.arguments = ["--internal-installation-update", "--game",
      URL(fileURLWithPath: gameExecutable).deletingLastPathComponent().path, "--json"]
    process.environment = ProcessInfo.processInfo.environment.filter { !$0.key.hasPrefix("DYLD_") }
    process.standardInput = input
    process.standardOutput = output
    process.standardError = FileHandle.nullDevice
    try process.run()
    var data = Data()
    while data.count < 65536 {
      guard let byte = try output.fileHandleForReading.read(upToCount: 1), !byte.isEmpty else { break }
      data.append(byte)
      if byte[0] == 10 { break }
    }
    guard let reply = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
      reply["ok"] as? Bool == true, reply["readiness"] as? String == "reserved"
    else {
      try? input.fileHandleForWriting.close()
      process.waitUntilExit()
      let reply = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any]
      let message = (reply?["error"] as? [String: Any])?["message"] as? String
      throw NSError(domain: "STFCProfilesPreview", code: 1,
        userInfo: [NSLocalizedDescriptionKey: message ?? "Quit games using this installation before updating it."])
    }
    return InstallationUpdateReservation(process: process, input: input)
  }

  func release() {
    try? input.fileHandleForWriting.close()
    process.waitUntilExit()
  }

  deinit { try? input.fileHandleForWriting.close() }
}

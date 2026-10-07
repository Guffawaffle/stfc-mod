import AppKit
import Foundation
import SwiftUI

private struct LaunchProfile: Decodable, Identifiable, Sendable {
  let id: String
  let name: String
  let kind: String
  let directory: String
  let configPath: String?
}

private struct ProfileReply: Decodable, Sendable {
  struct Problem: Decodable, Sendable { let message: String }
  let ok: Bool
  let error: Problem?
  let issues: [Problem]?
  let profiles: [LaunchProfile]?
  let profile: LaunchProfile?
  let processId: Int?
  let readiness: String?
}

private enum ProfileCoordinator {
  static var directory: URL { Bundle.main.bundleURL.appendingPathComponent("Contents") }

  static func run(_ arguments: [String]) throws -> ProfileReply {
    let helper = directory.appendingPathComponent("stfc-profiles")
    guard FileManager.default.isExecutableFile(atPath: helper.path) else {
      throw failure("This app is missing its profiles coordinator. Use the complete preview app bundle.")
    }
    let process = Process()
    process.executableURL = helper
    process.arguments = arguments + ["--json"]
    process.environment = ProcessInfo.processInfo.environment.filter { !$0.key.hasPrefix("DYLD_") }
    let output = Pipe(), errors = Pipe()
    process.standardOutput = output
    process.standardError = errors
    process.standardInput = FileHandle.nullDevice
    try process.run()
    let data = output.fileHandleForReading.readDataToEndOfFile()
    let errorData = errors.fileHandleForReading.readDataToEndOfFile()
    process.waitUntilExit()
    guard let reply = try? JSONDecoder().decode(ProfileReply.self, from: data) else {
      throw failure(String(data: errorData, encoding: .utf8) ?? "The profiles coordinator returned an invalid response.")
    }
    guard process.terminationStatus == 0 && reply.ok else {
      throw failure(reply.error?.message ?? "The profile operation failed.")
    }
    return reply
  }

  static func call(_ arguments: [String]) async throws -> ProfileReply {
    try await Task.detached { try run(arguments) }.value
  }

  static func failure(_ message: String) -> NSError {
    NSError(domain: "STFCProfilesPreview", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
  }
}

struct ProfilesPreviewView: View {
  @Environment(\.dismiss) private var dismiss
  @State private var profiles: [LaunchProfile] = []
  @State private var selectedID = ""
  @State private var newName = "Quasel Test"
  @State private var gameExecutable: URL?
  @State private var busy = false
  @State private var status = ""
  @State private var error = ""

  private var selected: LaunchProfile? { profiles.first { $0.id == selectedID } }

  var body: some View {
    VStack(alignment: .leading, spacing: 16) {
      Text("Profiles Preview").font(.title2)
      Text("Launch a separate game login and mod configuration. Each named profile starts fresh; sign in to the account you want to test.")
      HStack {
        TextField("New profile name", text: $newName)
        Button("Create") { createProfile() }
          .disabled(newName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
      }
      Picker("Profile", selection: $selectedID) {
        Text("Choose a profile").tag("")
        ForEach(profiles) { profile in Text(profile.name).tag(profile.id) }
      }
      HStack {
        Button("Choose Game…") { chooseGame() }
        Text(gameExecutable?.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().lastPathComponent ?? "No game selected")
          .lineLimit(1).truncationMode(.middle)
      }
      if let executable = gameExecutable {
        Text(executable.path).font(.caption).textSelection(.enabled)
      }
      HStack {
        Button("Launch Profile") { launchProfile() }.disabled(selected == nil || gameExecutable == nil)
        Button("Open Profile Folder") { openProfile() }.disabled(selected == nil)
        Button("Refresh") { Task { await refresh() } }
      }
      Text("For ordinary play, close this panel and use Engage. Isolated sign-in requires Chrome or Edge in Applications. Quit the profile's game and sign-in browser before opening it again.")
        .font(.caption).foregroundColor(.secondary)
      if busy { ProgressView().controlSize(.small) }
      if !status.isEmpty { Text(status).textSelection(.enabled) }
      if !error.isEmpty { Text(error).foregroundColor(.red).textSelection(.enabled) }
      Spacer(minLength: 0)
      HStack { Spacer(); Button("Done") { dismiss() }.disabled(busy) }
    }
    .padding(24).frame(width: 590, height: 450)
    .disabled(busy)
    .task {
      if let path = try? fetchGamePath() { gameExecutable = URL(fileURLWithPath: path) }
      await refresh()
    }
  }

  @MainActor private func refresh() async {
    busy = true; error = ""
    defer { busy = false }
    do {
      let reply = try await ProfileCoordinator.call(["list"])
      profiles = (reply.profiles ?? []).filter { $0.kind == "isolated" }
      if !profiles.contains(where: { $0.id == selectedID }) { selectedID = profiles.first?.id ?? "" }
      error = reply.issues?.map(\.message).joined(separator: "\n") ?? ""
    } catch { self.error = error.localizedDescription }
  }

  private func createProfile() {
    let name = newName.trimmingCharacters(in: .whitespacesAndNewlines)
    Task { @MainActor in
      busy = true; error = ""; status = ""
      do {
        let reply = try await ProfileCoordinator.call(["create", name])
        selectedID = reply.profile?.id ?? ""
        status = "Created \(reply.profile?.name ?? name)."
        busy = false
        await refresh()
      } catch { busy = false; self.error = error.localizedDescription }
    }
  }

  private func chooseGame() {
    let panel = NSOpenPanel()
    panel.title = "Choose the Star Trek Fleet Command game app"
    panel.canChooseDirectories = false; panel.canChooseFiles = true
    panel.allowsMultipleSelection = false; panel.allowedFileTypes = ["app"]
    guard panel.runModal() == .OK, let app = panel.url else { return }
    let executable = app.appendingPathComponent("Contents/MacOS/Star Trek Fleet Command")
    guard FileManager.default.isExecutableFile(atPath: executable.path) else {
      error = "Choose Star Trek Fleet Command.app, containing the game executable."; return
    }
    gameExecutable = executable
  }

  private func launchProfile() {
    guard let profile = selected, let executable = gameExecutable else { return }
    Task { @MainActor in
      busy = true; error = ""; status = "Waiting for profile isolation…"
      defer { busy = false }
      do {
        let reply = try await ProfileCoordinator.call(["launch", "--profile", profile.id,
          "--game", executable.deletingLastPathComponent().path, "--runtime",
          ProfileCoordinator.directory.appendingPathComponent("libstfc-community-mod.dylib").path])
        guard reply.readiness == "ready", let pid = reply.processId else {
          throw ProfileCoordinator.failure("The game did not confirm profile isolation.")
        }
        status = "\(profile.name) is ready (PID \(pid))."
      } catch { status = ""; self.error = error.localizedDescription }
    }
  }

  private func openProfile() {
    guard let profile = selected else { return }
    if !NSWorkspace.shared.open(URL(fileURLWithPath: profile.directory)) {
      error = "Could not open the profile folder in Finder."
    }
  }
}

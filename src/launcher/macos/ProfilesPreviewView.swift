// The optional Profiles panel invokes the bundled coordinator; the shared catalog remains its only source.

#if PROFILES_PREVIEW
import AppKit
import Foundation
import SwiftUI
import UniformTypeIdentifiers

private struct LaunchProfile: Decodable, Identifiable, Sendable {
  let id: String
  let name: String
  let kind: String
  let directory: String
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
  static var directory: URL { Bundle.main.bundleURL.appendingPathComponent("Contents", isDirectory: true) }

  static func failure(_ message: String) -> NSError {
    NSError(domain: "STFCProfilesPreview", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
  }

  // The coordinator returns structured failures on stdout. Reading that pipe while it runs also lets the
  // readiness wait finish without blocking the UI or filling a pipe that the game might inherit.
  private static func run(_ arguments: [String]) throws -> ProfileReply {
    let helper = directory.appendingPathComponent("stfc-profiles")
    guard FileManager.default.isExecutableFile(atPath: helper.path) else {
      throw failure("The profiles helper is missing. Install the complete Profiles preview app.")
    }
    let process = Process()
    process.executableURL = helper
    process.arguments = arguments + ["--json"]
    process.environment = ProcessInfo.processInfo.environment.filter { !$0.key.hasPrefix("DYLD_") }
    process.standardInput = FileHandle.nullDevice
    process.standardError = FileHandle.nullDevice
    let output = Pipe()
    process.standardOutput = output
    try process.run()
    let data = output.fileHandleForReading.readDataToEndOfFile()
    process.waitUntilExit()
    guard let reply = try? JSONDecoder().decode(ProfileReply.self, from: data) else {
      throw failure("The profiles helper did not return a valid response. Check whether its game is still running before retrying.")
    }
    guard process.terminationReason == .exit, process.terminationStatus == 0, reply.ok else {
      throw failure(reply.error?.message ?? "The profile operation failed.")
    }
    return reply
  }

  static func call(_ arguments: [String]) async throws -> ProfileReply {
    try await withCheckedThrowingContinuation { continuation in
      DispatchQueue.global(qos: .userInitiated).async {
        do { continuation.resume(returning: try run(arguments)) }
        catch { continuation.resume(throwing: error) }
      }
    }
  }

  static func prepareGame(_ app: URL) async throws {
    try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
      DispatchQueue.global(qos: .userInitiated).async {
        do {
          try ensureGameHasLoaderEntitlements(gameApp: app, requireExactSelection: true)
          continuation.resume()
        } catch { continuation.resume(throwing: error) }
      }
    }
  }
}

struct ProfilesPreviewView: View {
  @Environment(\.dismiss) private var dismiss
  @State private var profiles: [LaunchProfile] = []
  @State private var selectedID = ""
  @State private var newName = ""
  @State private var gameApp: URL?
  @State private var busy = true
  @State private var status = ""
  @State private var problem = ""

  private var selected: LaunchProfile? { profiles.first { $0.id == selectedID } }
  private var gameDirectory: URL? { gameApp?.appendingPathComponent("Contents/MacOS", isDirectory: true) }

  var body: some View {
    VStack(spacing: 16) {
      ScrollView {
        profileControls
          .frame(maxWidth: .infinity, alignment: .leading)
      }
      HStack {
        Text("Profiles preview").font(.caption).foregroundStyle(.secondary)
        Spacer()
        Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
      }
    }
    .padding(24)
    .frame(width: 590, height: 480)
    .disabled(busy)
    .interactiveDismissDisabled(busy)
    .task {
      if let install = try? XsollaUpdater(gameName: "Star Trek Fleet Command").gamePath() {
        let app = URL(fileURLWithPath: install).appendingPathComponent("Star Trek Fleet Command.app", isDirectory: true)
        if isGameApp(app) { gameApp = app }
      }
      await refresh()
    }
  }

  private var profileControls: some View {
    VStack(alignment: .leading, spacing: 16) {
      Text("Profiles").font(.title2)
      Text("Each profile has its own game login, settings and sign-in browser. New profiles start fresh.")
      HStack {
        TextField("New profile name", text: $newName)
          .onSubmit { if !newName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty { createProfile() } }
        Button("Create") { createProfile() }
          .disabled(newName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
      }
      Picker("Profile", selection: $selectedID) {
        Text(profiles.isEmpty ? "Create a profile to begin" : "Choose a profile").tag("")
        ForEach(profiles) { profile in Text(profile.name).tag(profile.id) }
      }
      HStack {
        Text("Game")
        Text(gameApp?.path ?? "Choose Star Trek Fleet Command.app")
          .lineLimit(1).truncationMode(.middle).textSelection(.enabled)
          .foregroundStyle(.secondary)
        Spacer(minLength: 0)
        Button("Choose Game…") { chooseGame() }
      }
      HStack {
        Button("Launch Profile") { launchProfile() }
          .disabled(selected == nil || gameDirectory == nil)
        Button("Open Profile Folder") { openProfileFolder() }.disabled(selected == nil)
        Button("Refresh") {
          busy = true
          Task { await refresh() }
        }
      }
      Text("Use Engage in the launcher for ordinary play. Profile sign-in needs Chrome or Edge in Applications. Quit a profile's game and sign-in browser before opening it again.")
        .font(.caption).foregroundStyle(.secondary)
      if busy { ProgressView().controlSize(.small) }
      if !status.isEmpty { Text(status).textSelection(.enabled) }
      if !problem.isEmpty { Text(problem).foregroundStyle(.red).textSelection(.enabled) }
    }
  }

  @MainActor private func refresh() async {
    busy = true
    problem = ""
    defer { busy = false }
    do {
      let reply = try await ProfileCoordinator.call(["list"])
      guard let listed = reply.profiles else { throw ProfileCoordinator.failure("The profiles list was missing.") }
      profiles = listed.filter { $0.kind == "isolated" }
      if !profiles.contains(where: { $0.id == selectedID }) { selectedID = profiles.first?.id ?? "" }
      problem = reply.issues?.map(\.message).joined(separator: "\n") ?? ""
    } catch { problem = error.localizedDescription }
  }

  private func createProfile() {
    let name = newName.trimmingCharacters(in: .whitespacesAndNewlines)
    guard !name.isEmpty, !busy else { return }
    busy = true
    status = ""
    problem = ""
    Task { @MainActor in
      do {
        let reply = try await ProfileCoordinator.call(["create", name])
        guard let created = reply.profile else { throw ProfileCoordinator.failure("The new profile was missing from the reply.") }
        selectedID = created.id
        newName = ""
        status = "Created \(created.name)."
        await refresh()
      } catch {
        problem = error.localizedDescription
        busy = false
      }
    }
  }

  private func isGameApp(_ app: URL) -> Bool {
    FileManager.default.isExecutableFile(atPath: app.appendingPathComponent("Contents/MacOS/Star Trek Fleet Command").path)
  }

  private func chooseGame() {
    let panel = NSOpenPanel()
    panel.title = "Choose Star Trek Fleet Command.app"
    panel.canChooseFiles = true
    panel.canChooseDirectories = false
    panel.allowsMultipleSelection = false
    panel.allowedContentTypes = [.applicationBundle]
    guard panel.runModal() == .OK, let app = panel.url else { return }
    guard isGameApp(app) else {
      problem = "Choose Star Trek Fleet Command.app containing the game executable."
      return
    }
    gameApp = app
    status = ""
    problem = ""
  }

  private func launchProfile() {
    guard let profile = selected, let app = gameApp, let directory = gameDirectory, !busy else { return }
    busy = true
    problem = ""
    status = "Waiting for \(profile.name) to start…"
    Task { @MainActor in
      defer { busy = false }
      do {
        let runtime = ProfileCoordinator.directory.appendingPathComponent("libstfc-community-mod.dylib")
        guard FileManager.default.fileExists(atPath: runtime.path),
          FileManager.default.isExecutableFile(atPath: ProfileCoordinator.directory.appendingPathComponent("stfc-profiles").path)
        else {
          throw ProfileCoordinator.failure("The profiles helper or runtime is missing. Install the complete Profiles preview app.")
        }
        try await ProfileCoordinator.prepareGame(app)
        let reply = try await ProfileCoordinator.call([
          "launch", "--profile", profile.id, "--game", directory.path, "--runtime", runtime.path,
        ])
        guard reply.readiness == "ready", let pid = reply.processId, pid > 0 else {
          throw ProfileCoordinator.failure("The game did not confirm profile isolation. Check whether it is still running before retrying.")
        }
        status = "\(profile.name) is ready (PID \(pid)). Sign in to the account you want in its browser window."
      } catch {
        status = ""
        problem = error.localizedDescription
      }
    }
  }

  private func openProfileFolder() {
    guard let profile = selected else { return }
    if !NSWorkspace.shared.open(URL(fileURLWithPath: profile.directory, isDirectory: true)) {
      problem = "Could not open the profile folder in Finder."
    }
  }
}
#endif

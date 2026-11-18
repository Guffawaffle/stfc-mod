// Launch controls use the existing loader, updater and optional profile coordinator.

import AppKit
import SwiftUI
import os

private let logger = Logger(subsystem: "com.stfcmod.startrekpatch", category: "launcher")

struct ActionView: View {
  @AppStorage("launcher.ordinaryGameName") private var ordinaryGameName = ""
  @State private var ordinaryGameNameDraft = ""
  @State private var showingOrdinaryGameRename = false
  #if PROFILES_PREVIEW
  @Binding var showingProfiles: Bool
  @State private var profiles: [LaunchProfile] = []
  @State private var selectedProfileID: String?
  @State private var profileLaunching = false
  @State private var profileListProblem = ""
  #endif
  private let gameUpdater = XsollaUpdater(gameName: "Star Trek Fleet Command")
  @State private var gameVersion = 0
  @State private var gameUpdateAvailable = false
  @State private var updating = false
  @State private var updateAction = ""
  @State private var updateSubAction = ""
  @State private var gameInstalled = false
  @State private var gameRunning = false
  @State private var showErrorAlert = false
  @State private var errorMessage = ""
  @State private var launchStatus = ""

  var body: some View {
    VStack(spacing: 0) {
      HStack {
        Text("QUICK LAUNCH").font(.system(size: 11, weight: .semibold)).tracking(1)
          .foregroundStyle(.secondary)
        Spacer()
        HStack(spacing: 5) {
          Circle().frame(width: 5, height: 5)
          Text(LauncherBuildIdentity.channelLabel).font(.system(size: 11, weight: .semibold))
        }
        .foregroundStyle(LauncherPalette.channel)
        .padding(.horizontal, 8).padding(.vertical, 6)
        .background(LauncherPalette.channel.opacity(0.12), in: RoundedRectangle(cornerRadius: 6))
      }
      .padding(.horizontal, 24).padding(.top, 16)

      GeometryReader { geometry in
        ScrollView(.vertical) {
          VStack(spacing: 0) {
            Image(nsImage: NSApplication.shared.applicationIconImage)
              .resizable().interpolation(.high).scaledToFit().frame(width: 72, height: 72)
              .accessibilityLabel("STFC Mod")
            Text("STFC Mod").font(.system(size: 23, weight: .bold)).padding(.top, 6)
            Text(launchDescription).font(.system(size: 13)).foregroundStyle(.secondary)
              .multilineTextAlignment(.center).lineLimit(2).help(launchDescription)
              .frame(maxWidth: 410, minHeight: 28).padding(.top, 6)
            launchButton.padding(.top, 10)
            utilityButtons.padding(.top, 16)

            if updating {
              HStack(spacing: 8) {
                ProgressView().controlSize(.small)
                Text(updateSubAction.isEmpty ? updateAction : "\(updateAction) · \(updateSubAction)").font(.caption)
                  .fixedSize(horizontal: false, vertical: true)
              }
              .padding(.top, 16)
            } else if !launchStatus.isEmpty {
              Text(launchStatus).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                .fixedSize(horizontal: false, vertical: true).help(launchStatus).padding(.top, 16)
                .accessibilityAddTraits(.updatesFrequently)
            }
          }
          .padding(.horizontal, 24).padding(.vertical, 16)
          .frame(maxWidth: .infinity)
          // Center at the default size; scroll instead of clipping in a short window.
          .frame(minHeight: geometry.size.height)
        }
        .scrollBounceBehavior(.basedOnSize)
      }
      Divider()
      HStack(spacing: 20) {
        VStack(alignment: .leading, spacing: 4) {
          Text(LauncherBuildIdentity.displayVersion).textSelection(.enabled)
            .fixedSize(horizontal: false, vertical: true)
          Text(gameInstalled ? "Game build \(gameVersion)" : "Game not installed")
        }
        .font(.system(size: 11)).foregroundStyle(.secondary)
        Spacer(minLength: 0)
        Button("About & credits") { LauncherBuildIdentity.showAbout() }
          .buttonStyle(.link).font(.system(size: 11)).fixedSize()
      }
      .padding(.horizontal, 24).padding(.vertical, 14)
      .background(LauncherPalette.surface)
    }
    .background(LauncherPalette.background)
    .task {
      // Check installed data and available updates once a minute, never on redraw.
      repeat {
        gameVersion = gameUpdater.installedVersion()
        gameInstalled = gameVersion > 0
        gameUpdateAvailable = await gameUpdater.checkForUpdateAvailable()
        try? await Task.sleep(for: .seconds(60))
      } while !Task.isCancelled
    }
    #if PROFILES_PREVIEW
    .task { await refreshProfiles() }
    .onChange(of: showingProfiles) { _, presented in
      if !presented { Task { await refreshProfiles() } }
    }
    #endif
    .alert("Error", isPresented: $showErrorAlert) {
      Button("OK", role: .cancel) {}
    } message: { Text(errorMessage) }
    .alert("Rename ordinary game", isPresented: $showingOrdinaryGameRename) {
      TextField("Ordinary game", text: $ordinaryGameNameDraft)
      Button("Save") {
        ordinaryGameName = ordinaryGameNameDraft.trimmingCharacters(in: .whitespacesAndNewlines)
      }
      Button("Cancel", role: .cancel) {}
    } message: {
      Text("Choose a launcher name for your existing login and settings. Leave it blank to use Ordinary game.")
    }
  }

  private var ordinaryLaunchName: String {
    ordinaryGameName.isEmpty ? "Ordinary game" : ordinaryGameName
  }

  private func renameOrdinaryGame() {
    ordinaryGameNameDraft = ordinaryGameName
    showingOrdinaryGameRename = true
  }

  private var launchDescription: String {
    #if PROFILES_PREVIEW
    if let profile = selectedProfile { return "\(profile.name) · Separate login and settings." }
    #endif
    return ordinaryGameName.isEmpty
      ? "Your existing login and normal settings."
      : "\(ordinaryLaunchName) · Your existing login and normal settings."
  }

  private var launchTitle: String {
    #if PROFILES_PREVIEW
    if profileLaunching { return "Launching…" }
    if let profile = selectedProfile { return "Launch \(profile.name)" }
    #endif
    if gameRunning { return "Game running" }
    return ordinaryGameName.isEmpty ? "Launch game" : "Launch \(ordinaryLaunchName)"
  }

  private var launchDisabled: Bool {
    if updating || !gameInstalled { return true }
    #if PROFILES_PREVIEW
    if profileLaunching { return true }
    if selectedProfile != nil { return false }
    #endif
    return gameRunning
  }

  private var launchButton: some View {
    HStack(spacing: 0) {
      Button(action: launchSelected) {
        Label(launchTitle, systemImage: "play")
          .font(.system(size: 14, weight: .semibold)).lineLimit(1).truncationMode(.middle)
          .frame(maxWidth: .infinity, minHeight: 48).contentShape(Rectangle())
      }
      .buttonStyle(.plain).disabled(launchDisabled)
      .help(launchDescription)
      #if PROFILES_PREVIEW
      Rectangle().fill(LauncherPalette.onAccent.opacity(0.23)).frame(width: 1, height: 48)
      Menu {
        Button { selectProfile(nil) } label: {
          if selectedProfileID == nil { Label(ordinaryLaunchName, systemImage: "checkmark") }
          else { Text(ordinaryLaunchName) }
        }
        ForEach(profiles) { profile in
          Button { selectProfile(profile.id) } label: {
            if selectedProfileID == profile.id { Label(profile.name, systemImage: "checkmark") }
            else { Text(profile.name) }
          }
        }
        if !profileListProblem.isEmpty {
          Divider()
          Text("Profiles unavailable").disabled(true)
          Button("Retry loading profiles") { Task { await refreshProfiles() } }
        }
        Divider()
        Button("Rename ordinary game…", action: renameOrdinaryGame)
        Button("Manage profiles…") { showingProfiles = true }
      } label: {
        Image(systemName: "chevron.down").font(.system(size: 11, weight: .semibold))
          .frame(width: 36, height: 48).contentShape(Rectangle())
      }
      .menuStyle(.borderlessButton).menuIndicator(.hidden).fixedSize()
      .accessibilityLabel("Choose launch target")
      .help("Choose ordinary play or a profile. Selection does not launch the game.")
      .disabled(updating || profileLaunching)
      #endif
    }
    .foregroundStyle(LauncherPalette.onAccent)
    .background(LauncherPalette.accent, in: RoundedRectangle(cornerRadius: 9))
    .frame(maxWidth: 330)
  }

  private var utilityButtons: some View {
    ViewThatFits(in: .horizontal) {
      HStack(spacing: 10) { utilityControls }.fixedSize(horizontal: true, vertical: false)
      VStack(spacing: 10) { utilityControls }
    }
    .font(.system(size: 12)).buttonStyle(.bordered).controlSize(.large)
  }

  @ViewBuilder private var utilityControls: some View {
    #if PROFILES_PREVIEW
    Button { showingProfiles = true } label: { Label("Profiles", systemImage: "square.3.layers.3d") }
      .disabled(updating || profileLaunching)
    #endif
    Menu {
      Button("Open web configuration", action: openConfigSite)
      Button("Open ordinary TOML", action: openConfigFile)
      Divider()
      Button("Rename ordinary game…", action: renameOrdinaryGame)
    } label: { Label("Mod settings", systemImage: "slider.horizontal.3") }
    .help("Settings for ordinary play. Named profiles have their own configuration folders.")
    Button(action: requestUpdate) {
      Label(gameUpdateAvailable ? "Update game" : "Game update", systemImage: "arrow.down.to.line")
    }
    .disabled(updateDisabled)
  }

  private func requestUpdate() {
    guard gameInstalled, !updating, !gameRunning else { return }
    #if PROFILES_PREVIEW
    guard !profileLaunching else { return }
    #endif
    // Claim the UI state before scheduling, including when checking for an update.
    updating = true
    updateAction = "Checking for updates"
    updateSubAction = ""
    launchStatus = ""
    Task { @MainActor in
      gameUpdateAvailable = await gameUpdater.checkForUpdateAvailable()
      if gameUpdateAvailable { await updateGame() }
      else { updating = false; launchStatus = "No game update is currently reported." }
    }
  }

  private var updateDisabled: Bool {
    #if PROFILES_PREVIEW
    if profileLaunching { return true }
    #endif
    return updating || gameRunning || !gameInstalled
  }

  private func launchSelected() {
    guard !launchDisabled else { return }
    launchStatus = ""
    #if PROFILES_PREVIEW
    if let profile = selectedProfile { launchProfile(profile); return }
    #endif
    launchGame()
  }

  #if PROFILES_PREVIEW
  private var selectedProfile: LaunchProfile? { profiles.first { $0.id == selectedProfileID } }

  private func selectProfile(_ id: String?) {
    selectedProfileID = id
    launchStatus = ""
  }

  @MainActor private func refreshProfiles() async {
    do {
      let reply = try await ProfileCoordinator.call(["list"])
      guard let listed = reply.profiles else { throw ProfileCoordinator.failure("The profiles list was missing.") }
      profiles = listed.filter { $0.kind == "isolated" }
      profileListProblem = reply.issues?.map(\.message).joined(separator: "\n") ?? ""
      if let id = selectedProfileID, !profiles.contains(where: { $0.id == id }) {
        selectedProfileID = nil
        launchStatus = "The selected profile is no longer available. \(ordinaryLaunchName) is selected."
      }
    } catch { profileListProblem = error.localizedDescription }
  }

  private func launchProfile(_ profile: LaunchProfile) {
    guard let app = ProfileCoordinator.installedGameApp() else {
      showError("The installed game could not be found. Open Profiles to choose its game app.")
      return
    }
    profileLaunching = true
    launchStatus = "Waiting for \(profile.name) to start…"
    Task { @MainActor in
      defer { profileLaunching = false }
      do {
        _ = try await ProfileCoordinator.launch(profile: profile, gameApp: app)
        launchStatus = "\(profile.name) started."
      } catch { launchStatus = ""; showError(error.localizedDescription) }
    }
  }
  #endif

  private func openConfigSite() {
    guard let configSiteURL = URL(string: "https://modconfig.pages.dev") else { return }
    NSWorkspace.shared.open(configSiteURL)
  }

  private func openConfigFile() {
    guard let configURL = ModFiles.configURL else {
      showError("Could not locate your Library folder.")
      return
    }
    var isDirectory: ObjCBool = false
    guard FileManager.default.fileExists(atPath: configURL.path, isDirectory: &isDirectory),
      !isDirectory.boolValue
    else {
      showError(
        "The mod config file is not available at \(configURL.path). Launch the game with the mod once to create it.")
      return
    }

    guard let applicationURL = NSWorkspace.shared.urlForApplication(toOpen: configURL)
      ?? NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.apple.TextEdit")
    else {
      showError("No app is available to open the mod config file. Install a text editor and try again.")
      return
    }

    Task { @MainActor in
      do {
        try await NSWorkspace.shared.open(
          [configURL], withApplicationAt: applicationURL, configuration: NSWorkspace.OpenConfiguration())
      } catch {
        showError("Could not open the mod config file: \(error.localizedDescription)")
      }
    }
  }

  private func showProgress(_ progress: XsollaUpdateProgress) {
    switch progress {
    case .start:
      updateAction = "Updating"
      updateSubAction = "Planning"
    case .progress(let currentAction, let totalActions):
      updateAction = "Updating \(currentAction) of \(totalActions)"
    case .extracting:
      updateSubAction = "Extracting"
    case .downloading:
      updateSubAction = "Downloading"
    case .patching:
      updateSubAction = "Patching"
    case .waiting:
      updateSubAction = "Waiting"
    case .applyVersion:
      updateSubAction = "Applying Version"
    case .versionApplied:
      updateSubAction = "Version Applied"
    case .finalizing:
      updateSubAction = "Finalizing"
    case .cleaningUp:
      updateSubAction = "Cleaning Up"
    case .complete:
      updateSubAction = "Complete"
    }
  }

  @MainActor
  private func updateGame() async {
    logger.info("User requested game update")
    updateAction = "Starting"
    updateSubAction = "Planning"
    do {
      try await gameUpdater.updateGame(onProgress: showProgress)
      logger.info("Game update finished successfully")
    } catch {
      logger.error("Error updating game: \(error.localizedDescription, privacy: .public)")
      updateAction = "Update Failed"
      updateSubAction = "Failed"
      showError("Game update failed: \(error.localizedDescription)")
    }
    updating = false
    gameVersion = gameUpdater.installedVersion()
    gameUpdateAvailable = await gameUpdater.checkForUpdateAvailable()
  }

  private func showError(_ message: String) {
    errorMessage = message
    showErrorAlert = true
  }

  // A GCD thread rather than a Task: runGame blocks until the game exits, which must not tie up a Swift concurrency
  // thread, and it waits for the entitlements panel, which needs the main thread free.
  private func launchGame() {
    gameRunning = true
    DispatchQueue.global().async {
      let result = Result { try runGame() }
      DispatchQueue.main.async {
        gameRunning = false
        if case .failure(let error) = result {
          showError(error.localizedDescription)
        }
      }
    }
  }
}

private enum LaunchError: LocalizedError {
  case loaderMissing
  case loaderFailed(Error)

  var errorDescription: String? {
    switch self {
    case .loaderMissing:
      return "The mod loader is missing from the launcher. Reinstall the community mod."
    case .loaderFailed(let error):
      return "Could not start the game: \(error.localizedDescription)"
    }
  }
}

// Runs the game through the mod's loader until it exits. A free function, so it can run off the main actor.
private func runGame() throws {
  #if PROFILES_PREVIEW
  let install = try XsollaUpdater(gameName: "Star Trek Fleet Command").gamePath()
  let gameApp = URL(fileURLWithPath: install)
    .appendingPathComponent("Star Trek Fleet Command.app", isDirectory: true)
    .resolvingSymlinksInPath().standardizedFileURL
  let gameDirectory = gameApp.appendingPathComponent("Contents/MacOS", isDirectory: true)
  #endif
  do {
    #if PROFILES_PREVIEW
    try ensureGameHasLoaderEntitlements(gameApp: gameApp, requireExactSelection: true, requireConfiguredGame: true)
    #else
    try ensureGameHasLoaderEntitlements()
    #endif
  } catch {
    logger.error("Error ensuring game entitlements: \(error.localizedDescription, privacy: .public)")
    throw error
  }

  guard let loader = Bundle.main.path(forAuxiliaryExecutable: "stfc-community-mod-loader") else {
    logger.error("The mod loader is missing from the launcher bundle")
    throw LaunchError.loaderMissing
  }
  let process = Process()
  process.executableURL = URL(fileURLWithPath: loader)
  process.environment = LauncherBuildIdentity.gameEnvironment()
  #if PROFILES_PREVIEW
  // Admission must precede exec/dyld, and the loader must use this exact admitted directory.
  let reservation = try InstallationReservation.acquire(gameDirectory: gameDirectory, forUpdate: false)
  defer { reservation.release() }
  process.arguments = ["--game-directory", gameDirectory.path]
  #endif
  do {
    try process.run()
  } catch {
    logger.error("Could not start the mod loader: \(error.localizedDescription, privacy: .public)")
    throw LaunchError.loaderFailed(error)
  }
  process.waitUntilExit()
}

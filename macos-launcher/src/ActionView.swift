import AppKit
import Foundation
import SwiftUI
import os

/// Logger for launcher actions
private let logger = Logger(subsystem: "com.stfcmod.startrekpatch", category: "launcher")

struct ActionView: View, XSollaUpdaterDelegate {

  @StateObject var gameUpdater = GameUpdaterViewModel()
  @State private var gameVersion: Int = 0
  @State private var gameUpdateAvailable: Bool = false
  @State private var updating: Bool = false
  @State private var updateAction: String = ""
  @State private var updateSubAction: String = ""
  @State private var updateProgress: Float = 0.0
  @State private var gameInstalled: Bool = false
  @State private var gameRunning: Bool = false
  @State private var showErrorAlert: Bool = false
  @State private var errorMessage: String = ""
  private var model = PulsatingViewModel()

  func updateProgress(progress: XsollaUpdateProgress) {
    switch progress {
    case .Start(let totalActions):
      updateAction = "Updating"
      updateSubAction = "Planning"
      updateProgress = totalActions > 0 ? 0.0 : 1.0
    case .Progress(let currentAction, let totalActions):
      updateAction = "Updating \(currentAction) of \(totalActions)"
      updateProgress = Float(currentAction) / Float(totalActions)
      break
    case .Extracting(_):
      updateSubAction = "Extracting"
      break
    case .ExtractComplete(_):
      break
    case .Downloading(_):
      updateSubAction = "Downloading"
      break
    case .DownloadComplete(_):
      break
    case .Patching(_):
      updateSubAction = "Patching"
      break
    case .PatchingProgress(_, _):
      break
    case .PatchStepComplete:
      break
    case .PatchComplete:
      break
    case .Waiting:
      updateSubAction = "Waiting"
      break
    case .ApplyVersion:
      updateSubAction = "Applying Version"
    case .VersionApplied:
      updateSubAction = "Version Applied"
    case .Finalizing:
      updateSubAction = "Finalizing"
      break
    case .CleaningUp:
      updateSubAction = "Cleaning Up"
      break
    case .Complete:
      updateSubAction = "Complete"
      updateProgress = 1.0
      break
    }
  }

  var body: some View {
    GeometryReader { geo in
      Grid(horizontalSpacing: 8) {
        GridRow {
          Button {
            withAnimation {
              openConfigSite()
            }
          } label: {
            commonButton(text: "Configure Mod")
              .foregroundColor(.lcarViolet)
          }.buttonStyle(PlainButtonStyle())

          Button {
            openConfigFile()
          } label: {
            commonButton(text: "Open TOML")
              .foregroundColor(.lcarViolet)
          }
          .buttonStyle(PlainButtonStyle())
          .help("Open community_patch_settings.toml in your associated app or TextEdit")

          if gameInstalled {
            Group {
              Button {
                withAnimation {
                  if gameUpdateAvailable && !updating {
                    Task {
                      do {
                        logger.info("User requested game update")
                        updating = true
                        updateAction = "Starting"
                        updateSubAction = "Planning"
                        try await gameUpdater.updateGame(delegate: self)
                        logger.info("Game update finished successfully")
                      } catch {
                        logger.error("Error updating game: \(error.localizedDescription)")
                        updateAction = "Update Failed"
                        updateSubAction = "Failed"
                        errorMessage = "Game update failed: \(error.localizedDescription)"
                        showErrorAlert = true
                      }
                      updating = false
                      gameVersion = gameUpdater.getInstalledGameVersion()
                      gameUpdateAvailable = await gameUpdater.checkForGameUpdate()
                    }
                  }
                }
              } label: {
                if gameUpdateAvailable {
                  commonButton(text: "Update Game!")
                    .foregroundColor(.lcarTan)
                } else {
                  commonButton()
                    .foregroundColor(.lcarTan)
                }
              }.buttonStyle(PlainButtonStyle())
              Button {
                withAnimation {
                  launchGame()
                }
              } label: {
                commonButton(text: "Engage!")
                  .foregroundColor(.lcarOrange)
              }.buttonStyle(PlainButtonStyle())
            }
            .opacity(updating || gameRunning ? 0.5 : 1.0)
            .allowsHitTesting(!updating && !gameRunning)
          } else {
            Text("Game not installed")
              .font(.custom("HelveticaNeue-CondensedBold", size: 24))
              .foregroundColor(.lcarTan)
              .lineLimit(1)
              .minimumScaleFactor(0.7)
              .frame(width: 192, height: 50)
          }

        }
      }
      .frame(width: geo.size.width, height: 150)
      .offset(x: 85, y: 20)
      .task {
        repeat {
          gameVersion = gameUpdater.getInstalledGameVersion()
          gameInstalled = gameVersion > 0
          gameVersion = gameUpdater.getInstalledGameVersion()
          gameUpdateAvailable = await gameUpdater.checkForGameUpdate()
          try? await Task.sleep(for: .seconds(60))
        } while !Task.isCancelled

      }
      .overlay(alignment: .bottomTrailing) {
        Text("Game Version: \(String(format: "%02d", gameVersion))")
          .font(.custom("HelveticaNeue-CondensedBold", size: 17))
          .foregroundColor(.lcarTan)
          .offset(x: -10)
      }
      .overlay(alignment: .bottomLeading) {
        if updating {
          HStack {
            PulsatingView(viewModel: model)
            Text("\(updateAction) (\(updateSubAction))")
              .font(.custom("HelveticaNeue-CondensedBold", size: 17))
              .foregroundColor(.lcarTan)
              .offset(x: -10)
          }
          .offset(x: 115, y: 10)
        }
      }
      .alert("Error", isPresented: $showErrorAlert) {
        Button("OK", role: .cancel) {}
      } message: {
        Text(errorMessage)
      }
    }
  }

  private func commonButton(text: String = "") -> some View {
    // Keep the four-button row to 392 points, including three 8-point gaps.
    RoundedRectangle(cornerRadius: 20)
      .frame(width: 92, height: 50)
      .overlay(alignment: .bottomTrailing) {
        HStack {
          Spacer()
          Text(text.count > 0 ? text : "\(randomDigits(4))-\(randomDigits(3))")
            .font(.custom("HelveticaNeue-CondensedBold", size: 17))
            .foregroundColor(.black)
            .lineLimit(1)
            .minimumScaleFactor(0.7)
        }
        .padding(.bottom, 5)
        .padding(.leading, 8)
        .padding(.trailing, 12)
      }
  }

  private func randomDigits(_ count: Int) -> String {
    (1...count)
      .map { _ in "\(Int.random(in: 0...9))" }
      .joined()
  }

  private func openConfigSite() {
    guard let configSiteURL = URL(string: "https://modconfig.pages.dev") else { return }
    NSWorkspace.shared.open(configSiteURL)
  }

  private func openConfigFile() {
    guard let library = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first else {
      errorMessage = "Could not locate your Library folder."
      showErrorAlert = true
      return
    }
    // Keep this path aligned with File::MakePath in mods/src/file.cc.
    let configURL = library
      .appendingPathComponent("Preferences", isDirectory: true)
      .appendingPathComponent("com.stfcmod.startrekpatch", isDirectory: true)
      .appendingPathComponent("community_patch_settings.toml")
    var isDirectory: ObjCBool = false
    guard FileManager.default.fileExists(atPath: configURL.path, isDirectory: &isDirectory),
      !isDirectory.boolValue
    else {
      errorMessage = """
        The mod config file is not available at \(configURL.path).
        Launch the game with the mod once to create it.
        """
      showErrorAlert = true
      return
    }

    guard let applicationURL = NSWorkspace.shared.urlForApplication(toOpen: configURL)
      ?? NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.apple.TextEdit")
    else {
      errorMessage = "No app is available to open the mod config file. Install a text editor and try again."
      showErrorAlert = true
      return
    }

    Task { @MainActor in
      do {
        try await NSWorkspace.shared.open(
          [configURL], withApplicationAt: applicationURL, configuration: NSWorkspace.OpenConfiguration())
      } catch {
        errorMessage = "Could not open the mod config file: \(error.localizedDescription)"
        showErrorAlert = true
      }
    }
  }

  private func launchGame() {
    DispatchQueue.global().async {
      DispatchQueue.main.async {
        gameRunning = true
      }

      // Ensure the game has the required entitlements before launching
      do {
        try ensureGameHasLoaderEntitlements()
      } catch let error as EntitlementError {
        logger.error("Error ensuring game entitlements: \(error.localizedDescription)")
        DispatchQueue.main.async {
          errorMessage = error.localizedDescription
          showErrorAlert = true
          gameRunning = false
        }
        return
      } catch {
        logger.error("Error ensuring game entitlements: \(error.localizedDescription)")
        DispatchQueue.main.async {
          errorMessage = "Failed to prepare game for launch: \(error.localizedDescription)"
          showErrorAlert = true
          gameRunning = false
        }
        return
      }

      let process = Process()
      let helper = Bundle.main.path(forAuxiliaryExecutable: "stfc-community-mod-loader")
      process.executableURL = URL(fileURLWithPath: helper!)
      DispatchQueue.global().async {
        do {
          try process.run()
        } catch {
          DispatchQueue.main.async {
            gameRunning = false
          }
          return
        }
        process.waitUntilExit()
        DispatchQueue.main.async {
          gameRunning = false
        }
      }
    }
  }
}

class PulsatingViewModel: ObservableObject {
  @Published var colorIndex = 1
}

struct PulsatingView: View {

  @ObservedObject var viewModel: PulsatingViewModel

  func colourToShow() -> Color {
    switch viewModel.colorIndex {
    case 0:
      return Color.lcarOrange
    case 1:
      return Color.lcarTan
    case 2:
      return Color.lcarViolet
    default:
      return Color.lcarPink
    }
  }

  @State var animate = false
  var body: some View {
    VStack {
      ZStack {
        Circle().fill(colourToShow().opacity(0.25)).frame(width: 40, height: 40).scaleEffect(
          self.animate ? 1 : 0)
        Circle().fill(colourToShow().opacity(0.35)).frame(width: 30, height: 30).scaleEffect(
          self.animate ? 1 : 0)
        Circle().fill(colourToShow().opacity(0.45)).frame(width: 15, height: 15).scaleEffect(
          self.animate ? 1 : 0)
        Circle().fill(colourToShow()).frame(width: 16.25, height: 16.25)
      }
      .onAppear { self.animate = true }
      .animation(
        animate ? Animation.easeInOut(duration: 1.5).repeatForever(autoreverses: true) : .default,
        value: animate
      )
      .onChange(of: viewModel.colorIndex) { _ in
        self.animate = false
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
          self.animate = true
        }
      }

    }
  }
}

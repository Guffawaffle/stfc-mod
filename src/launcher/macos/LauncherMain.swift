// The launcher app: a resizable window, and quitting when it closes.

import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
  func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    true
  }
}

@main
struct STFC_Community_Patch_LauncherApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
  @FocusedValue(\.modFolderAction) private var modFolderAction
  #if PROFILES_PREVIEW
  @FocusedValue(\.profilesPresentation) private var profilesPresentation
  #endif

  var body: some Scene {
    WindowGroup("STFC Mod") {
      ContentView()
        .frame(minWidth: 360, minHeight: 360)
    }
    .defaultSize(width: 560, height: 520)
    .windowResizability(.contentMinSize)
    .commands {
      CommandGroup(replacing: .appInfo) {
        Button("About STFC Community Mod") {
          LauncherBuildIdentity.showAbout()
        }
      }
      CommandGroup(after: .sidebar) {
        #if PROFILES_PREVIEW
        Button("Profiles…") { profilesPresentation?.wrappedValue = true }
          .keyboardShortcut("p", modifiers: [.command, .shift])
          .disabled(profilesPresentation == nil)
        #endif
        Divider()
        Button("View Mod Folder") { modFolderAction?() }
          .disabled(modFolderAction == nil)
      }
    }
  }
}

private struct ModFolderActionKey: FocusedValueKey {
  typealias Value = @MainActor () -> Void
}

extension FocusedValues {
  var modFolderAction: (@MainActor () -> Void)? {
    get { self[ModFolderActionKey.self] }
    set { self[ModFolderActionKey.self] = newValue }
  }
}

#if PROFILES_PREVIEW
private struct ProfilesPresentationKey: FocusedValueKey {
  typealias Value = Binding<Bool>
}

extension FocusedValues {
  var profilesPresentation: Binding<Bool>? {
    get { self[ProfilesPresentationKey.self] }
    set { self[ProfilesPresentationKey.self] = newValue }
  }
}
#endif

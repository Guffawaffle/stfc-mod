// The launcher app: one fixed-size window, and quitting when it closes.

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
  #if PROFILES_PREVIEW
  @FocusedValue(\.profilesPresentation) private var profilesPresentation
  #endif

  var body: some Scene {
    WindowGroup {
      ContentView()
        .frame(width: 600, height: 400)
        .fixedSize()
    }
    .windowResizability(.contentSize)
    #if PROFILES_PREVIEW
    .commands {
      CommandGroup(after: .sidebar) {
        Button("Profiles…") { profilesPresentation?.wrappedValue = true }
          .keyboardShortcut("p", modifiers: [.command, .shift])
          .disabled(profilesPresentation == nil)
      }
    }
    #endif
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

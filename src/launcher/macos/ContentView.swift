// The compact launcher keeps launch targets, settings and exact build identity visible.

import AppKit
import SwiftUI

struct ContentView: View {
  #if PROFILES_PREVIEW
  @State private var showingProfiles = false
  #endif

  var body: some View {
    #if PROFILES_PREVIEW
    ActionView(showingProfiles: $showingProfiles)
      // Commands follow the active window even before a control has keyboard focus.
      .focusedSceneValue(\.profilesPresentation, $showingProfiles)
      .sheet(isPresented: $showingProfiles) { ProfilesPreviewView() }
    #else
    ActionView()
    #endif
  }
}

enum LauncherPalette {
  static let background = adaptive(light: 0xf5f8fa, dark: 0x101a23)
  static let surface = adaptive(light: 0xffffff, dark: 0x172530)
  static let accent = Color(red: 0.35, green: 0.84, blue: 0.89)
  static let onAccent = Color(red: 0.06, green: 0.17, blue: 0.19)

  static var channel: Color {
    switch LauncherBuildIdentity.channelLabel {
    case "DEV": return adaptive(light: 0x6844a5, dark: 0xceb4ff)
    case "Pre": return adaptive(light: 0x855800, dark: 0xedb95f)
    case "CI": return adaptive(light: 0x426478, dark: 0xb9d8ea)
    default: return adaptive(light: 0x136875, dark: 0x59d7e4)
    }
  }

  private static func adaptive(light: UInt32, dark: UInt32) -> Color {
    Color(nsColor: NSColor(name: nil) { appearance in
      let value = appearance.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua ? dark : light
      return NSColor(
        red: CGFloat((value >> 16) & 0xff) / 255,
        green: CGFloat((value >> 8) & 0xff) / 255,
        blue: CGFloat(value & 0xff) / 255, alpha: 1)
    })
  }
}
